// IsoDeck — Direct2D / DirectWrite drawing layer
#pragma once
#include "util.h"
#include <d2d1_1.h>
#include <dwrite_1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <unordered_map>

namespace isodeck {

using Microsoft::WRL::ComPtr;

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    Rect() = default;
    Rect(float X, float Y, float W, float H) : x(X), y(Y), w(W), h(H) {}
    float r() const { return x + w; }
    float b() const { return y + h; }
    bool Contains(float px, float py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    Rect Inset(float d) const { return { x + d, y + d, w - 2 * d, h - 2 * d }; }
    Rect Inset(float dx, float dy) const { return { x + dx, y + dy, w - 2 * dx, h - 2 * dy }; }
    D2D1_RECT_F D() const { return D2D1::RectF(x, y, x + w, y + h); }
};

struct Color { float r, g, b, a; };
inline Color Hex(uint32_t rgb, float a = 1.f) { return { ((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, a }; }
inline Color WithA(Color c, float a) { c.a = a; return c; }
inline Color Lerp(Color a, Color b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t }; }
Color ParseHex(const Str& s, Color fallback);

// Palette (dark, blue accent)
namespace pal {
    const Color bg       = Hex(0x0C1220);
    const Color side     = Hex(0x0F172A);
    const Color card     = Hex(0x141D31);
    const Color card2    = Hex(0x1A2540);
    const Color border   = Hex(0x223050);
    const Color text     = Hex(0xE6ECF7);
    const Color text2    = Hex(0x9AA8C2);
    const Color text3    = Hex(0x647391);
    const Color accent   = Hex(0x00A3FF);
    const Color accent2  = Hex(0x0084D6);
    const Color green    = Hex(0x22C55E);
    const Color red      = Hex(0xEF4444);
    const Color amber    = Hex(0xF59E0B);
    const Color white    = Hex(0xFFFFFF);
}

enum class Align { Start, Center, End };
enum class Weight { Regular = 400, Medium = 500, SemiBold = 600, Bold = 700 };

struct TextStyle {
    float  size = 14.f;
    Weight weight = Weight::Regular;
    Align  align = Align::Start;      // Start = leading (right in RTL)
    Align  valign = Align::Center;
    bool   wrap = false;
    bool   ellipsis = true;
    bool   forceLTR = false;          // for file names, URLs, hashes, latin text
};

class Gfx {
public:
    bool  Init(HWND hwnd);
    void  Resize(UINT w, UINT h);
    void  SetDpi(float dpi);
    float Scale() const { return m_dpi / 96.f; }
    bool  Begin();                   // returns false if device lost & recreated (caller should redraw)
    bool  End();
    void  SetRTL(bool rtl) { m_rtl = rtl; }
    bool  RTL() const { return m_rtl; }
    float Width() const { return m_w; }
    float Height() const { return m_h; }
    ID2D1DeviceContext* DC() { return m_dc.Get(); }

    void  Clear(Color c);
    void  FillRect(const Rect& r, Color c);
    void  FillRound(const Rect& r, float radius, Color c);
    void  StrokeRound(const Rect& r, float radius, Color c, float width = 1.f);
    void  FillCircle(float cx, float cy, float rad, Color c);
    void  StrokeCircle(float cx, float cy, float rad, Color c, float width = 1.f);
    void  Line(float x1, float y1, float x2, float y2, Color c, float width = 1.f);
    void  GradientRound(const Rect& r, float radius, Color a, Color b, bool vertical = false);
    void  PushClip(const Rect& r);
    void  PopClip();
    void  Shadow(const Rect& r, float radius, float blur, Color c);   // cheap fake shadow rings

    void  Text(const Rect& r, const Str& s, const TextStyle& st, Color c);
    float MeasureWidth(const Str& s, const TextStyle& st, float maxW = 10000.f);
    float MeasureHeight(const Str& s, const TextStyle& st, float w);
    float CaretX(const Rect& r, const Str& s, const TextStyle& st, size_t pos);  // x of caret for text field
    size_t HitText(const Rect& r, const Str& s, const TextStyle& st, float x);  // caret position from x

    // vector icons
    enum class Icon { Usb, Disc, Palette, Download, Gear, Refresh, Plus, Trash, Check, Close, Minimize, Info, Warning, Shield, Folder, Hash, Globe, Bolt, Image, Arrow };
    void  DrawIcon(Icon ic, const Rect& r, Color c, float stroke = 2.f);

    // bitmaps (preview)
    ID2D1Bitmap* LoadBitmapFile(const Str& path);
    ID2D1Bitmap* LoadBitmapResource(int id);
    void  DrawBitmapCover(ID2D1Bitmap* bmp, const Rect& r, float radius, float opacity = 1.f);
    void  ClearBitmapCache();

private:
    ComPtr<IDWriteTextLayout> Layout(const Str& s, const TextStyle& st, float w, float h);
    ComPtr<IDWriteTextFormat> Fmt(const TextStyle& st);
    ID2D1SolidColorBrush* Brush(Color c);
    bool CreateTarget();
    ID2D1Bitmap* BitmapFromStream(IStream* s);

    HWND m_hwnd = nullptr;
    float m_dpi = 96.f, m_w = 0, m_h = 0;
    bool m_rtl = false;
    ComPtr<ID2D1Factory1> m_factory;
    ComPtr<IDWriteFactory1> m_dw;
    ComPtr<IWICImagingFactory> m_wic;
    ComPtr<ID2D1HwndRenderTarget> m_rt;
    ComPtr<ID2D1DeviceContext> m_dc;
    ComPtr<ID2D1SolidColorBrush> m_brush;
    std::unordered_map<uint64_t, ComPtr<IDWriteTextFormat>> m_fmts;
    std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap>> m_bitmaps;
    int m_clipDepth = 0;
};

} // namespace isodeck
