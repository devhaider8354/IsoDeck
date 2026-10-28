// IsoDeck — Direct2D / DirectWrite drawing layer
#include "gfx.h"
#include "resource.h"
#include <shlwapi.h>
#include <cmath>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

namespace isodeck {

static const wchar_t* kFont = L"Segoe UI";

Color ParseHex(const Str& s, Color fb) {
    if (s.size() != 7 || s[0] != L'#') return fb;
    wchar_t* end = nullptr;
    unsigned long v = wcstoul(s.c_str() + 1, &end, 16);
    if (!end || *end) return fb;
    return Hex((uint32_t)v);
}

bool Gfx::Init(HWND hwnd) {
    m_hwnd = hwnd;
    D2D1_FACTORY_OPTIONS opts{};
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &opts, (void**)m_factory.GetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1), (IUnknown**)m_dw.GetAddressOf()))) return false;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_wic));
    return CreateTarget();
}

bool Gfx::CreateTarget() {
    m_rt.Reset(); m_dc.Reset(); m_brush.Reset(); m_bitmaps.clear();
    RECT rc; GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U sz = D2D1::SizeU(std::max(1L, rc.right - rc.left), std::max(1L, rc.bottom - rc.top));
    auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), m_dpi, m_dpi);
    auto hprops = D2D1::HwndRenderTargetProperties(m_hwnd, sz, D2D1_PRESENT_OPTIONS_NONE);
    if (FAILED(m_factory->CreateHwndRenderTarget(props, hprops, &m_rt))) return false;
    m_rt.As(&m_dc);
    m_rt->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 1), &m_brush);
    m_rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    m_w = sz.width * 96.f / m_dpi; m_h = sz.height * 96.f / m_dpi;
    return true;
}

void Gfx::Resize(UINT w, UINT h) {
    if (!m_rt) return;
    m_rt->Resize(D2D1::SizeU(std::max(1u, w), std::max(1u, h)));
    m_w = w * 96.f / m_dpi; m_h = h * 96.f / m_dpi;
}
void Gfx::SetDpi(float dpi) {
    m_dpi = dpi;
    if (m_rt) { m_rt->SetDpi(dpi, dpi); RECT rc; GetClientRect(m_hwnd, &rc); Resize(rc.right - rc.left, rc.bottom - rc.top); }
}
bool Gfx::Begin() {
    if (!m_rt) { if (!CreateTarget()) return false; }
    m_rt->BeginDraw();
    m_rt->SetTransform(D2D1::Matrix3x2F::Identity());
    m_clipDepth = 0;
    return true;
}
bool Gfx::End() {
    while (m_clipDepth > 0) PopClip();
    HRESULT hr = m_rt->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) { CreateTarget(); return false; }
    return true;
}

ID2D1SolidColorBrush* Gfx::Brush(Color c) { m_brush->SetColor(D2D1::ColorF(c.r, c.g, c.b, c.a)); return m_brush.Get(); }

void Gfx::Clear(Color c) { m_rt->Clear(D2D1::ColorF(c.r, c.g, c.b, c.a)); }
void Gfx::FillRect(const Rect& r, Color c) { m_rt->FillRectangle(r.D(), Brush(c)); }
void Gfx::FillRound(const Rect& r, float rad, Color c) { m_rt->FillRoundedRectangle(D2D1::RoundedRect(r.D(), rad, rad), Brush(c)); }
void Gfx::StrokeRound(const Rect& r, float rad, Color c, float w) { m_rt->DrawRoundedRectangle(D2D1::RoundedRect(r.Inset(w / 2).D(), rad, rad), Brush(c), w); }
void Gfx::FillCircle(float cx, float cy, float rad, Color c) { m_rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), Brush(c)); }
void Gfx::StrokeCircle(float cx, float cy, float rad, Color c, float w) { m_rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), rad, rad), Brush(c), w); }
void Gfx::Line(float x1, float y1, float x2, float y2, Color c, float w) { m_rt->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), Brush(c), w); }
void Gfx::GradientRound(const Rect& r, float rad, Color a, Color b, bool vertical) {
    D2D1_GRADIENT_STOP stops[2] = { { 0.f, D2D1::ColorF(a.r, a.g, a.b, a.a) }, { 1.f, D2D1::ColorF(b.r, b.g, b.b, b.a) } };
    ComPtr<ID2D1GradientStopCollection> coll;
    m_rt->CreateGradientStopCollection(stops, 2, &coll);
    ComPtr<ID2D1LinearGradientBrush> br;
    m_rt->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(r.x, r.y), vertical ? D2D1::Point2F(r.x, r.b()) : D2D1::Point2F(r.r(), r.y)), coll.Get(), &br);
    m_rt->FillRoundedRectangle(D2D1::RoundedRect(r.D(), rad, rad), br.Get());
}
void Gfx::PushClip(const Rect& r) { m_rt->PushAxisAlignedClip(r.D(), D2D1_ANTIALIAS_MODE_ALIASED); ++m_clipDepth; }
void Gfx::PopClip() { if (m_clipDepth > 0) { m_rt->PopAxisAlignedClip(); --m_clipDepth; } }
void Gfx::Shadow(const Rect& r, float rad, float blur, Color c) {
    int steps = (int)std::max(2.f, blur / 2.f);
    for (int i = steps; i >= 1; --i) {
        float d = (float)i * blur / steps;
        Rect rr = { r.x - d, r.y - d + blur * 0.35f, r.w + 2 * d, r.h + 2 * d };
        FillRound(rr, rad + d, WithA(c, c.a / (steps * 1.4f)));
    }
}

ComPtr<IDWriteTextFormat> Gfx::Fmt(const TextStyle& st) {
    uint64_t key = ((uint64_t)(int)(st.size * 4) << 32) | ((uint64_t)(int)st.weight << 8) | (st.forceLTR ? 4 : 0) | (m_rtl ? 2 : 0) | (st.wrap ? 1 : 0);
    auto it = m_fmts.find(key);
    if (it != m_fmts.end()) return it->second;
    ComPtr<IDWriteTextFormat> f;
    m_dw->CreateTextFormat(kFont, nullptr, (DWRITE_FONT_WEIGHT)(int)st.weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, st.size, L"", &f);
    f->SetReadingDirection((m_rtl && !st.forceLTR) ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    f->SetWordWrapping(st.wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    m_fmts[key] = f;
    return f;
}

ComPtr<IDWriteTextLayout> Gfx::Layout(const Str& s, const TextStyle& st, float w, float h) {
    ComPtr<IDWriteTextLayout> lay;
    m_dw->CreateTextLayout(s.c_str(), (UINT32)s.size(), Fmt(st).Get(), std::max(1.f, w), std::max(1.f, h), &lay);
    if (!lay) return lay;
    bool rtl = m_rtl && !st.forceLTR;
    DWRITE_TEXT_ALIGNMENT ta = DWRITE_TEXT_ALIGNMENT_LEADING;
    // In RTL mode a forced-LTR text should still sit on the "start" side of the layout (right side) when Align::Start.
    if (st.align == Align::Center) ta = DWRITE_TEXT_ALIGNMENT_CENTER;
    else if (st.align == Align::End) ta = DWRITE_TEXT_ALIGNMENT_TRAILING;
    if (st.forceLTR && m_rtl && st.align != Align::Center) ta = (st.align == Align::Start) ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING;
    (void)rtl;
    lay->SetTextAlignment(ta);
    lay->SetParagraphAlignment(st.valign == Align::Center ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : st.valign == Align::End ? DWRITE_PARAGRAPH_ALIGNMENT_FAR : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (st.ellipsis && !st.wrap) {
        ComPtr<IDWriteInlineObject> sign;
        m_dw->CreateEllipsisTrimmingSign(lay.Get(), &sign);
        DWRITE_TRIMMING tr{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
        lay->SetTrimming(&tr, sign.Get());
    }
    return lay;
}

void Gfx::Text(const Rect& r, const Str& s, const TextStyle& st, Color c) {
    if (s.empty()) return;
    auto lay = Layout(s, st, r.w, r.h);
    if (lay) m_rt->DrawTextLayout(D2D1::Point2F(r.x, r.y), lay.Get(), Brush(c), D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
float Gfx::MeasureWidth(const Str& s, const TextStyle& st, float maxW) {
    TextStyle t = st; t.ellipsis = false;
    auto lay = Layout(s, t, maxW, 1000.f);
    DWRITE_TEXT_METRICS m{}; if (lay) lay->GetMetrics(&m);
    return std::ceil(m.widthIncludingTrailingWhitespace);
}
float Gfx::MeasureHeight(const Str& s, const TextStyle& st, float w) {
    TextStyle t = st; t.wrap = true; t.ellipsis = false;
    auto lay = Layout(s, t, w, 10000.f);
    DWRITE_TEXT_METRICS m{}; if (lay) lay->GetMetrics(&m);
    return std::ceil(m.height);
}
float Gfx::CaretX(const Rect& r, const Str& s, const TextStyle& st, size_t pos) {
    TextStyle t = st; t.ellipsis = false;
    auto lay = Layout(s.empty() ? L" " : s, t, r.w, r.h);
    float x = 0, y = 0; DWRITE_HIT_TEST_METRICS m{};
    if (!lay) return r.x;
    bool trailing = false;
    UINT32 p = (UINT32)pos;
    if (s.empty()) { p = 0; }
    else if (p >= s.size()) { p = (UINT32)s.size() - 1; trailing = true; }
    lay->HitTestTextPosition(p, trailing, &x, &y, &m);
    return r.x + x;
}
size_t Gfx::HitText(const Rect& r, const Str& s, const TextStyle& st, float px) {
    if (s.empty()) return 0;
    TextStyle t = st; t.ellipsis = false;
    auto lay = Layout(s, t, r.w, r.h);
    if (!lay) return s.size();
    BOOL trailing = FALSE, inside = FALSE; DWRITE_HIT_TEST_METRICS m{};
    lay->HitTestPoint(px - r.x, r.h / 2, &trailing, &inside, &m);
    size_t pos = m.textPosition + (trailing ? m.length : 0);
    return std::min(pos, s.size());
}

// ---------------------------------------------------------------- icons
void Gfx::DrawIcon(Icon ic, const Rect& r, Color c, float sw) {
    ID2D1SolidColorBrush* b = Brush(c);
    float x = r.x, y = r.y, w = r.w, h = r.h, cx = x + w / 2, cy = y + h / 2;
    auto P = [](float a, float b2) { return D2D1::Point2F(a, b2); };
    auto L = [&](float a, float b2, float c2, float d) { m_rt->DrawLine(P(a, b2), P(c2, d), b, sw); };
    ComPtr<ID2D1StrokeStyle> round;
    m_factory->CreateStrokeStyle(D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND), nullptr, 0, &round);
    auto LR = [&](float a, float b2, float c2, float d) { m_rt->DrawLine(P(a, b2), P(c2, d), b, sw, round.Get()); };
    switch (ic) {
    case Icon::Usb: {
        Rect body{ x + w * 0.30f, y + h * 0.40f, w * 0.40f, h * 0.55f };
        m_rt->DrawRoundedRectangle(D2D1::RoundedRect(body.D(), 3, 3), b, sw);
        Rect plug{ x + w * 0.38f, y + h * 0.10f, w * 0.24f, h * 0.30f };
        m_rt->DrawRectangle(plug.D(), b, sw);
        L(x + w * 0.44f, y + h * 0.18f, x + w * 0.44f, y + h * 0.26f); L(x + w * 0.56f, y + h * 0.18f, x + w * 0.56f, y + h * 0.26f);
        break; }
    case Icon::Disc:
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.42f, h * 0.42f), b, sw);
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.12f, h * 0.12f), b, sw);
        break;
    case Icon::Palette:
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.42f, h * 0.42f), b, sw);
        m_rt->FillEllipse(D2D1::Ellipse(P(x + w * 0.35f, y + h * 0.38f), w * 0.07f, w * 0.07f), b);
        m_rt->FillEllipse(D2D1::Ellipse(P(x + w * 0.55f, y + h * 0.30f), w * 0.07f, w * 0.07f), b);
        m_rt->FillEllipse(D2D1::Ellipse(P(x + w * 0.68f, y + h * 0.50f), w * 0.07f, w * 0.07f), b);
        m_rt->FillEllipse(D2D1::Ellipse(P(x + w * 0.40f, y + h * 0.62f), w * 0.07f, w * 0.07f), b);
        break;
    case Icon::Download:
        LR(cx, y + h * 0.15f, cx, y + h * 0.62f); LR(cx - w * 0.2f, y + h * 0.45f, cx, y + h * 0.65f); LR(cx + w * 0.2f, y + h * 0.45f, cx, y + h * 0.65f);
        LR(x + w * 0.18f, y + h * 0.82f, x + w * 0.82f, y + h * 0.82f);
        break;
    case Icon::Gear: {
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.16f, h * 0.16f), b, sw);
        for (int i = 0; i < 8; ++i) { float a = i * 3.14159f / 4; LR(cx + cosf(a) * w * 0.28f, cy + sinf(a) * h * 0.28f, cx + cosf(a) * w * 0.42f, cy + sinf(a) * h * 0.42f); }
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.30f, h * 0.30f), b, sw);
        break; }
    case Icon::Refresh: {
        ComPtr<ID2D1PathGeometry> g; m_factory->CreatePathGeometry(&g);
        ComPtr<ID2D1GeometrySink> s; g->Open(&s);
        float rad = w * 0.36f;
        s->BeginFigure(P(cx + rad * cosf(-0.6f), cy + rad * sinf(-0.6f)), D2D1_FIGURE_BEGIN_HOLLOW);
        s->AddArc(D2D1::ArcSegment(P(cx + rad * cosf(4.4f), cy + rad * sinf(4.4f)), D2D1::SizeF(rad, rad), 0, D2D1_SWEEP_DIRECTION_CLOCKWISE, D2D1_ARC_SIZE_LARGE));
        s->EndFigure(D2D1_FIGURE_END_OPEN); s->Close();
        m_rt->DrawGeometry(g.Get(), b, sw, round.Get());
        float ax = cx + rad * cosf(-0.6f), ay = cy + rad * sinf(-0.6f);
        LR(ax, ay, ax - w * 0.18f, ay - h * 0.02f); LR(ax, ay, ax + w * 0.02f, ay - h * 0.18f);
        break; }
    case Icon::Plus: LR(cx, y + h * 0.2f, cx, y + h * 0.8f); LR(x + w * 0.2f, cy, x + w * 0.8f, cy); break;
    case Icon::Trash:
        LR(x + w * 0.2f, y + h * 0.28f, x + w * 0.8f, y + h * 0.28f);
        LR(x + w * 0.4f, y + h * 0.28f, x + w * 0.4f, y + h * 0.16f); LR(x + w * 0.6f, y + h * 0.28f, x + w * 0.6f, y + h * 0.16f); LR(x + w * 0.4f, y + h * 0.16f, x + w * 0.6f, y + h * 0.16f);
        LR(x + w * 0.28f, y + h * 0.28f, x + w * 0.33f, y + h * 0.84f); LR(x + w * 0.72f, y + h * 0.28f, x + w * 0.67f, y + h * 0.84f); LR(x + w * 0.33f, y + h * 0.84f, x + w * 0.67f, y + h * 0.84f);
        break;
    case Icon::Check: LR(x + w * 0.2f, cy, x + w * 0.42f, y + h * 0.72f); LR(x + w * 0.42f, y + h * 0.72f, x + w * 0.82f, y + h * 0.3f); break;
    case Icon::Close: LR(x + w * 0.25f, y + h * 0.25f, x + w * 0.75f, y + h * 0.75f); LR(x + w * 0.75f, y + h * 0.25f, x + w * 0.25f, y + h * 0.75f); break;
    case Icon::Minimize: LR(x + w * 0.25f, cy, x + w * 0.75f, cy); break;
    case Icon::Info:
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.42f, h * 0.42f), b, sw);
        LR(cx, y + h * 0.45f, cx, y + h * 0.72f); m_rt->FillEllipse(D2D1::Ellipse(P(cx, y + h * 0.3f), sw * 0.8f, sw * 0.8f), b);
        break;
    case Icon::Warning:
        LR(cx, y + h * 0.12f, x + w * 0.9f, y + h * 0.85f); LR(x + w * 0.9f, y + h * 0.85f, x + w * 0.1f, y + h * 0.85f); LR(x + w * 0.1f, y + h * 0.85f, cx, y + h * 0.12f);
        LR(cx, y + h * 0.38f, cx, y + h * 0.6f); m_rt->FillEllipse(D2D1::Ellipse(P(cx, y + h * 0.72f), sw * 0.8f, sw * 0.8f), b);
        break;
    case Icon::Shield: {
        ComPtr<ID2D1PathGeometry> g; m_factory->CreatePathGeometry(&g);
        ComPtr<ID2D1GeometrySink> s; g->Open(&s);
        s->BeginFigure(P(cx, y + h * 0.1f), D2D1_FIGURE_BEGIN_HOLLOW);
        s->AddLine(P(x + w * 0.85f, y + h * 0.25f)); s->AddLine(P(x + w * 0.8f, y + h * 0.6f)); s->AddLine(P(cx, y + h * 0.9f));
        s->AddLine(P(x + w * 0.2f, y + h * 0.6f)); s->AddLine(P(x + w * 0.15f, y + h * 0.25f)); s->EndFigure(D2D1_FIGURE_END_CLOSED); s->Close();
        m_rt->DrawGeometry(g.Get(), b, sw, round.Get());
        LR(x + w * 0.35f, cy, x + w * 0.46f, y + h * 0.62f); LR(x + w * 0.46f, y + h * 0.62f, x + w * 0.66f, y + h * 0.38f);
        break; }
    case Icon::Folder:
        LR(x + w * 0.12f, y + h * 0.25f, x + w * 0.4f, y + h * 0.25f); LR(x + w * 0.4f, y + h * 0.25f, x + w * 0.48f, y + h * 0.35f); LR(x + w * 0.48f, y + h * 0.35f, x + w * 0.88f, y + h * 0.35f);
        LR(x + w * 0.88f, y + h * 0.35f, x + w * 0.88f, y + h * 0.8f); LR(x + w * 0.88f, y + h * 0.8f, x + w * 0.12f, y + h * 0.8f); LR(x + w * 0.12f, y + h * 0.8f, x + w * 0.12f, y + h * 0.25f);
        break;
    case Icon::Hash:
        LR(x + w * 0.35f, y + h * 0.2f, x + w * 0.28f, y + h * 0.8f); LR(x + w * 0.65f, y + h * 0.2f, x + w * 0.58f, y + h * 0.8f);
        LR(x + w * 0.18f, y + h * 0.4f, x + w * 0.85f, y + h * 0.4f); LR(x + w * 0.15f, y + h * 0.62f, x + w * 0.82f, y + h * 0.62f);
        break;
    case Icon::Globe:
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.4f, h * 0.4f), b, sw);
        m_rt->DrawEllipse(D2D1::Ellipse(P(cx, cy), w * 0.16f, h * 0.4f), b, sw);
        LR(x + w * 0.1f, cy, x + w * 0.9f, cy);
        break;
    case Icon::Bolt: {
        ComPtr<ID2D1PathGeometry> g; m_factory->CreatePathGeometry(&g);
        ComPtr<ID2D1GeometrySink> s; g->Open(&s);
        s->BeginFigure(P(x + w * 0.58f, y + h * 0.08f), D2D1_FIGURE_BEGIN_FILLED);
        s->AddLine(P(x + w * 0.22f, y + h * 0.56f)); s->AddLine(P(x + w * 0.47f, y + h * 0.56f)); s->AddLine(P(x + w * 0.4f, y + h * 0.92f));
        s->AddLine(P(x + w * 0.78f, y + h * 0.42f)); s->AddLine(P(x + w * 0.53f, y + h * 0.42f)); s->EndFigure(D2D1_FIGURE_END_CLOSED); s->Close();
        m_rt->FillGeometry(g.Get(), b);
        break; }
    case Icon::Image:
        m_rt->DrawRoundedRectangle(D2D1::RoundedRect(Rect(x + w * 0.12f, y + h * 0.18f, w * 0.76f, h * 0.64f).D(), 2, 2), b, sw);
        LR(x + w * 0.2f, y + h * 0.72f, x + w * 0.42f, y + h * 0.45f); LR(x + w * 0.42f, y + h * 0.45f, x + w * 0.58f, y + h * 0.62f); LR(x + w * 0.58f, y + h * 0.62f, x + w * 0.68f, y + h * 0.52f); LR(x + w * 0.68f, y + h * 0.52f, x + w * 0.8f, y + h * 0.72f);
        m_rt->FillEllipse(D2D1::Ellipse(P(x + w * 0.68f, y + h * 0.34f), w * 0.05f, w * 0.05f), b);
        break;
    case Icon::Arrow: LR(x + w * 0.2f, cy, x + w * 0.8f, cy); LR(x + w * 0.55f, y + h * 0.3f, x + w * 0.8f, cy); LR(x + w * 0.55f, y + h * 0.7f, x + w * 0.8f, cy); break;
    }
}

// -------------------------------------------------------------- bitmaps
ID2D1Bitmap* Gfx::BitmapFromStream(IStream* stream) {
    if (!m_wic) return nullptr;
    ComPtr<IWICBitmapDecoder> dec;
    if (FAILED(m_wic->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &dec))) return nullptr;
    ComPtr<IWICBitmapFrameDecode> frame; dec->GetFrame(0, &frame);
    if (!frame) return nullptr;
    ComPtr<IWICFormatConverter> conv; m_wic->CreateFormatConverter(&conv);
    if (!conv) return nullptr;
    if (FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut))) return nullptr;
    // downscale big images for preview
    UINT w = 0, h = 0; conv->GetSize(&w, &h);
    ComPtr<IWICBitmapSource> src = conv;
    if (w > 1280) {
        ComPtr<IWICBitmapScaler> sc; m_wic->CreateBitmapScaler(&sc);
        sc->Initialize(conv.Get(), 1280, h * 1280 / w, WICBitmapInterpolationModeFant);
        src = sc;
    }
    ComPtr<ID2D1Bitmap> bmp;
    if (FAILED(m_rt->CreateBitmapFromWicBitmap(src.Get(), nullptr, &bmp))) return nullptr;
    return bmp.Detach();
}
ID2D1Bitmap* Gfx::LoadBitmapFile(const Str& path) {
    auto it = m_bitmaps.find(path);
    if (it != m_bitmaps.end()) return it->second.Get();
    ComPtr<IStream> st;
    if (FAILED(SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, 0, FALSE, nullptr, &st))) return nullptr;
    ID2D1Bitmap* b = BitmapFromStream(st.Get());
    m_bitmaps[path].Attach(b);
    return b;
}
ID2D1Bitmap* Gfx::LoadBitmapResource(int id) {
    Str key = Format(L"res:%d", id);
    auto it = m_bitmaps.find(key);
    if (it != m_bitmaps.end()) return it->second.Get();
    std::string bytes; if (!LoadResourceBytes(id, L"PACKAGE", bytes)) return nullptr;
    ComPtr<IStream> st; st.Attach(SHCreateMemStream((const BYTE*)bytes.data(), (UINT)bytes.size()));
    if (!st) return nullptr;
    ID2D1Bitmap* b = BitmapFromStream(st.Get());
    m_bitmaps[key].Attach(b);
    return b;
}
void Gfx::DrawBitmapCover(ID2D1Bitmap* bmp, const Rect& r, float radius, float opacity) {
    if (!bmp) return;
    D2D1_SIZE_F sz = bmp->GetSize();
    float s = std::max(r.w / sz.width, r.h / sz.height);
    float dw = sz.width * s, dh = sz.height * s;
    float sx = (dw - r.w) / 2 / s, sy = (dh - r.h) / 2 / s;
    ComPtr<ID2D1Layer> layer;
    ComPtr<ID2D1RoundedRectangleGeometry> geo;
    m_factory->CreateRoundedRectangleGeometry(D2D1::RoundedRect(r.D(), radius, radius), &geo);
    m_rt->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), geo.Get()), nullptr);
    m_rt->DrawBitmap(bmp, r.D(), opacity, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, D2D1::RectF(sx, sy, sz.width - sx, sz.height - sy));
    m_rt->PopLayer();
}
void Gfx::ClearBitmapCache() { m_bitmaps.clear(); }

} // namespace isodeck
