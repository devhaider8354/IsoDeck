// IsoDeck — immediate-mode widget layer on top of Gfx (RTL aware)
#pragma once
#include "gfx.h"
#include <unordered_map>

namespace isodeck {

typedef uint32_t Id;
Id MakeId(const wchar_t* name, int index = 0);

struct Input {
    float mx = -1, my = -1;         // DIPs
    bool  down = false;             // left button currently down
    bool  pressed = false;          // went down this frame
    bool  released = false;         // went up this frame
    float wheel = 0;                // notches this frame
    std::vector<wchar_t> chars;     // WM_CHAR this frame
    std::vector<int>     keys;      // WM_KEYDOWN vk codes this frame
    bool ctrl = false;
    void EndFrame() { pressed = released = false; wheel = 0; chars.clear(); keys.clear(); }
};

enum class BtnStyle { Primary, Secondary, Ghost, Danger, Success };

class UI {
public:
    UI(Gfx& g, Input& in) : g(g), in(in) {}
    Gfx& g; Input& in;

    void BeginFrame(float dtSec);
    bool NeedsAnimation() const { return m_anim; }
    void RequestAnim() { m_anim = true; }

    // Mirroring: layout is authored LTR and mirrored automatically when RTL.
    Rect R(const Rect& ltr) const;              // mirror if RTL
    bool Hover(const Rect& mirrored) const;
    bool Clicked(Id id, const Rect& mirrored);  // press+release inside
    float Anim(Id id, bool target, float speed = 10.f); // 0..1 eased toward target

    // widgets (all take LTR-authored rects; they mirror internally)
    bool Button(Id id, const Rect& r, const Str& text, BtnStyle st = BtnStyle::Secondary, Gfx::Icon* icon = nullptr, bool enabled = true);
    bool IconButton(Id id, const Rect& r, Gfx::Icon icon, Color c, bool enabled = true, Color hoverBg = WithA(pal::white, 0.08f));
    bool Toggle(Id id, const Rect& r, const Str& label, bool& value, bool enabled = true);
    bool Radio(Id id, const Rect& r, const Str& label, bool selected, bool enabled = true);
    int  Segmented(Id id, const Rect& r, const std::vector<Str>& items, int sel, bool enabled = true);
    bool TextField(Id id, const Rect& r, Str& value, const Str& placeholder, bool latin = false, bool numeric = false, bool enabled = true);
    void Progress(const Rect& r, float pct01, Color c = pal::accent);
    void Card(const Rect& r, float radius = 14.f, Color c = pal::card, bool border = true);
    void Label(const Rect& r, const Str& s, float size, Color c, Weight w = Weight::Regular, Align a = Align::Start, bool latin = false);
    void Para(const Rect& r, const Str& s, float size, Color c);   // wrapped text
    void Chip(const Rect& r, const Str& s, Color bg, Color fg);
    void Spinner(float cx, float cy, float rad, Color c);

    // Scroll region: content is laid out relative to rect top; returns offset to add to Y.
    float BeginScroll(Id id, const Rect& r, float contentH);
    void  EndScroll();
    bool  InScroll() const { return m_scrollActive; }

    // text focus
    Id   Focus() const { return m_focus; }
    void ClearFocus() { m_focus = 0; }
    bool AnyFieldFocused() const { return m_focus != 0; }

    double time = 0;
private:
    struct AnimState { float v = 0; bool target = false; };
    std::unordered_map<Id, AnimState> m_anims;
    std::unordered_map<Id, float> m_scrolls;
    Id m_focus = 0, m_active = 0;
    size_t m_caret = 0;
    float m_dt = 0.016f;
    bool m_anim = false;
    bool m_scrollActive = false;
    Rect m_scrollRect; Id m_scrollId = 0; float m_scrollMax = 0;
    float m_caretBlink = 0;
};

} // namespace isodeck
