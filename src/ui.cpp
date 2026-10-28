// IsoDeck — immediate-mode widget layer
#include "ui.h"
#include <cmath>
#include <algorithm>

namespace isodeck {

Id MakeId(const wchar_t* name, int index) {
    uint32_t h = 2166136261u;
    for (const wchar_t* p = name; *p; ++p) { h ^= (uint32_t)*p; h *= 16777619u; }
    h ^= (uint32_t)(index + 1) * 2654435761u;
    return h ? h : 1;
}

void UI::BeginFrame(float dt) {
    m_dt = std::min(0.05f, std::max(0.001f, dt));
    m_anim = false;
    time += dt;
    m_caretBlink += dt;
    if (in.pressed) { /* focus is cleared unless a field claims it this frame */ m_active = 0; }
}

Rect UI::R(const Rect& r) const {
    if (!g.RTL()) return r;
    return { g.Width() - r.x - r.w, r.y, r.w, r.h };
}
bool UI::Hover(const Rect& r) const {
    if (m_scrollActive && !m_scrollRect.Contains(in.mx, in.my)) return false;
    return r.Contains(in.mx, in.my);
}
bool UI::Clicked(Id id, const Rect& r) {
    bool h = Hover(r);
    if (h && in.pressed) m_active = id;
    if (in.released && m_active == id) { m_active = 0; return h; }
    return false;
}
float UI::Anim(Id id, bool target, float speed) {
    auto& a = m_anims[id];
    float goal = target ? 1.f : 0.f;
    if (std::fabs(a.v - goal) > 0.002f) {
        a.v += (goal - a.v) * std::min(1.f, speed * m_dt);
        m_anim = true;
    } else a.v = goal;
    return a.v;
}

void UI::Card(const Rect& lr, float radius, Color c, bool border) {
    Rect r = R(lr);
    g.FillRound(r, radius, c);
    if (border) g.StrokeRound(r, radius, pal::border, 1.f);
}
void UI::Label(const Rect& lr, const Str& s, float size, Color c, Weight w, Align a, bool latin) {
    TextStyle st; st.size = size; st.weight = w; st.align = a; st.forceLTR = latin;
    g.Text(R(lr), s, st, c);
}
void UI::Para(const Rect& lr, const Str& s, float size, Color c) {
    TextStyle st; st.size = size; st.wrap = true; st.valign = Align::Start; st.ellipsis = false;
    g.Text(R(lr), s, st, c);
}
void UI::Chip(const Rect& lr, const Str& s, Color bg, Color fg) {
    Rect r = R(lr);
    g.FillRound(r, r.h / 2, bg);
    TextStyle st; st.size = 11.5f; st.weight = Weight::SemiBold; st.align = Align::Center;
    g.Text(r, s, st, fg);
}
void UI::Spinner(float cx, float cy, float rad, Color c) {
    m_anim = true;
    float a0 = (float)std::fmod(time * 4.0, 6.28318);
    for (int i = 0; i < 8; ++i) {
        float a = a0 + i * 0.785f;
        g.FillCircle(cx + cosf(a) * rad, cy + sinf(a) * rad, rad * 0.22f, WithA(c, 0.15f + 0.85f * (i / 7.f)));
    }
}
void UI::Progress(const Rect& lr, float p, Color c) {
    Rect r = R(lr);
    g.FillRound(r, r.h / 2, pal::card2);
    p = std::max(0.f, std::min(1.f, p));
    if (p > 0) {
        float w = std::max(r.h, r.w * p);
        Rect f = g.RTL() ? Rect{ r.r() - w, r.y, w, r.h } : Rect{ r.x, r.y, w, r.h };
        g.GradientRound(f, r.h / 2, c, Lerp(c, pal::white, 0.25f));
    }
}

bool UI::Button(Id id, const Rect& lr, const Str& text, BtnStyle st, Gfx::Icon* icon, bool enabled) {
    Rect r = R(lr);
    bool hover = enabled && Hover(r);
    float t = Anim(id, hover);
    bool pressed = enabled && m_active == id && in.down;
    Color bg, fg, border = WithA(pal::white, 0.f);
    switch (st) {
    case BtnStyle::Primary: bg = Lerp(pal::accent, Lerp(pal::accent, pal::white, 0.18f), t); fg = pal::white; break;
    case BtnStyle::Success: bg = Lerp(pal::green, Lerp(pal::green, pal::white, 0.18f), t); fg = pal::white; break;
    case BtnStyle::Danger:  bg = Lerp(pal::red, Lerp(pal::red, pal::white, 0.15f), t); fg = pal::white; break;
    case BtnStyle::Secondary: bg = Lerp(pal::card2, Lerp(pal::card2, pal::white, 0.08f), t); fg = pal::text; border = pal::border; break;
    case BtnStyle::Ghost: bg = WithA(pal::white, 0.07f * t); fg = pal::text2; break;
    }
    if (!enabled) { bg = WithA(bg, 0.35f); fg = WithA(fg, 0.5f); }
    if (pressed) bg = Lerp(bg, pal::bg, 0.2f);
    if (st == BtnStyle::Primary && enabled) g.Shadow(r, 10, 8, WithA(pal::accent, 0.25f * (0.6f + 0.4f * t)));
    g.FillRound(r, 10, bg);
    if (border.a > 0) g.StrokeRound(r, 10, border);
    float pad = 14, ix = 0;
    Rect tr = r.Inset(pad, 0);
    if (icon) {
        float isz = std::min(20.f, r.h * 0.5f);
        Rect ir = g.RTL() ? Rect{ r.r() - pad - isz, r.y + (r.h - isz) / 2, isz, isz } : Rect{ r.x + pad, r.y + (r.h - isz) / 2, isz, isz };
        g.DrawIcon(*icon, ir, fg, 1.8f);
        ix = isz + 8;
        tr = g.RTL() ? Rect{ r.x + pad, r.y, r.w - 2 * pad - ix, r.h } : Rect{ r.x + pad + ix, r.y, r.w - 2 * pad - ix, r.h };
    }
    TextStyle ts; ts.size = 13.5f; ts.weight = Weight::SemiBold; ts.align = Align::Center;
    g.Text(tr, text, ts, fg);
    return enabled && Clicked(id, r);
}

bool UI::IconButton(Id id, const Rect& lr, Gfx::Icon icon, Color c, bool enabled, Color hoverBg) {
    Rect r = R(lr);
    bool hover = enabled && Hover(r);
    float t = Anim(id, hover);
    if (t > 0.01f) g.FillRound(r, 8, WithA(hoverBg, hoverBg.a * t));
    float isz = std::min(r.w, r.h) * 0.5f;
    g.DrawIcon(icon, { r.x + (r.w - isz) / 2, r.y + (r.h - isz) / 2, isz, isz }, enabled ? c : WithA(c, 0.4f), 1.8f);
    return enabled && Clicked(id, r);
}

bool UI::Toggle(Id id, const Rect& lr, const Str& label, bool& value, bool enabled) {
    Rect r = R(lr);
    bool hover = enabled && Hover(r);
    float t = Anim(id, value, 12.f);
    float sw = 42, sh = 24;
    Rect sw_r = g.RTL() ? Rect{ r.x, r.y + (r.h - sh) / 2, sw, sh } : Rect{ r.r() - sw, r.y + (r.h - sh) / 2, sw, sh };
    Color track = Lerp(pal::card2, pal::accent, t);
    if (!enabled) track = WithA(track, 0.4f);
    g.FillRound(sw_r, sh / 2, track);
    if (t < 0.99f) g.StrokeRound(sw_r, sh / 2, WithA(pal::border, 1 - t));
    float kx = g.RTL() ? sw_r.r() - 4 - 8 - (sw - 24) * t : sw_r.x + 4 + 8 + (sw - 24) * t;
    g.FillCircle(kx, sw_r.y + sh / 2, 8, hover ? pal::white : Lerp(pal::text2, pal::white, t));
    TextStyle ts; ts.size = 13.5f;
    Rect tr = g.RTL() ? Rect{ r.x + sw + 12, r.y, r.w - sw - 12, r.h } : Rect{ r.x, r.y, r.w - sw - 12, r.h };
    g.Text(tr, label, ts, enabled ? pal::text : pal::text3);
    if (enabled && Clicked(id, r)) { value = !value; return true; }
    return false;
}

bool UI::Radio(Id id, const Rect& lr, const Str& label, bool selected, bool enabled) {
    Rect r = R(lr);
    bool hover = enabled && Hover(r);
    float t = Anim(id, selected, 12.f), h = Anim(id ^ 0x5555, hover);
    g.FillRound(r, 10, Lerp(pal::card2, Lerp(pal::card2, pal::accent, 0.18f), std::max(t, h * 0.5f)));
    g.StrokeRound(r, 10, Lerp(pal::border, pal::accent, t));
    float cx = g.RTL() ? r.r() - 20 : r.x + 20, cy = r.y + r.h / 2;
    g.StrokeCircle(cx, cy, 8, Lerp(pal::text3, pal::accent, t), 1.6f);
    if (t > 0.01f) g.FillCircle(cx, cy, 4.5f * t, pal::accent);
    TextStyle ts; ts.size = 13.5f; ts.weight = selected ? Weight::SemiBold : Weight::Regular;
    Rect tr = g.RTL() ? Rect{ r.x + 12, r.y, r.w - 48, r.h } : Rect{ r.x + 36, r.y, r.w - 48, r.h };
    g.Text(tr, label, ts, enabled ? pal::text : pal::text3);
    return enabled && Clicked(id, r);
}

int UI::Segmented(Id id, const Rect& lr, const std::vector<Str>& items, int sel, bool enabled) {
    Rect r = R(lr);
    g.FillRound(r, 10, pal::bg);
    g.StrokeRound(r, 10, pal::border);
    int n = (int)items.size(); if (n == 0) return sel;
    float w = (r.w - 6) / n;
    int result = sel;
    for (int i = 0; i < n; ++i) {
        int li = g.RTL() ? (n - 1 - i) : i;      // visual order
        Rect s{ r.x + 3 + w * i, r.y + 3, w, r.h - 6 };
        bool hover = enabled && Hover(s);
        float t = Anim(id ^ (Id)(li * 7919 + 1), li == sel, 12.f);
        if (t > 0.01f) g.FillRound(s, 8, WithA(pal::accent, 0.9f * t));
        else if (hover) g.FillRound(s, 8, WithA(pal::white, 0.05f));
        TextStyle ts; ts.size = 13; ts.weight = Weight::SemiBold; ts.align = Align::Center; ts.forceLTR = true;
        g.Text(s, items[li], ts, enabled ? Lerp(pal::text2, pal::white, t) : pal::text3);
        if (enabled && Clicked(id ^ (Id)(li * 7919 + 1), s)) result = li;
    }
    return result;
}

bool UI::TextField(Id id, const Rect& lr, Str& value, const Str& placeholder, bool latin, bool numeric, bool enabled) {
    Rect r = R(lr);
    bool hover = enabled && Hover(r);
    bool focused = (m_focus == id);
    if (enabled && in.pressed && hover) {
        m_focus = id; focused = true; m_caretBlink = 0;
        TextStyle ts; ts.size = 13.5f; ts.forceLTR = latin;
        m_caret = g.HitText(r.Inset(12, 0), value, ts, in.mx);
    } else if (in.pressed && focused && !hover) { m_focus = 0; focused = false; }
    float t = Anim(id, focused, 14.f);
    g.FillRound(r, 10, pal::bg);
    g.StrokeRound(r, 10, Lerp(hover ? Lerp(pal::border, pal::text3, 0.5f) : pal::border, pal::accent, t), 1.f + t);
    bool changed = false;
    if (focused && enabled) {
        if (m_caret > value.size()) m_caret = value.size();
        for (int vk : in.keys) {
            bool rtlText = g.RTL() && !latin;
            switch (vk) {
            case VK_LEFT:  if (rtlText) { if (m_caret < value.size()) ++m_caret; } else if (m_caret > 0) --m_caret; break;
            case VK_RIGHT: if (rtlText) { if (m_caret > 0) --m_caret; } else if (m_caret < value.size()) ++m_caret; break;
            case VK_HOME: m_caret = 0; break;
            case VK_END: m_caret = value.size(); break;
            case VK_DELETE: if (m_caret < value.size()) { value.erase(m_caret, 1); changed = true; } break;
            case VK_ESCAPE: m_focus = 0; break;
            case 'V': if (in.ctrl && OpenClipboard(nullptr)) {
                HANDLE h = GetClipboardData(CF_UNICODETEXT);
                if (h) { const wchar_t* p = (const wchar_t*)GlobalLock(h); if (p) { Str s = p; for (auto& c : s) if (c == L'\r' || c == L'\n') c = L' '; value.insert(m_caret, s); m_caret += s.size(); changed = true; GlobalUnlock(h); } }
                CloseClipboard(); } break;
            }
            m_caretBlink = 0;
        }
        for (wchar_t c : in.chars) {
            if (c == 8) { if (m_caret > 0) { value.erase(m_caret - 1, 1); --m_caret; changed = true; } }
            else if (c >= 32 && c != 127) {
                if (numeric && !(c >= L'0' && c <= L'9')) continue;
                if (latin && c > 0x7E) continue;
                if (value.size() < 256) { value.insert(m_caret, 1, c); ++m_caret; changed = true; }
            }
            m_caretBlink = 0;
        }
    }
    Rect tr = r.Inset(12, 0);
    TextStyle ts; ts.size = 13.5f; ts.forceLTR = latin; ts.ellipsis = false;
    g.PushClip(tr);
    if (value.empty() && !focused) g.Text(tr, placeholder, ts, pal::text3);
    else {
        g.Text(tr, value, ts, enabled ? pal::text : pal::text3);
        if (focused) {
            m_anim = true;
            if (std::fmod(m_caretBlink, 1.0f) < 0.55f) {
                float cx = g.CaretX(tr, value, ts, m_caret);
                if (value.empty()) cx = (g.RTL() && !latin) ? tr.r() : tr.x;
                g.Line(cx, tr.y + 8, cx, tr.b() - 8, pal::accent, 1.5f);
            }
        }
    }
    g.PopClip();
    return changed;
}

float UI::BeginScroll(Id id, const Rect& lr, float contentH) {
    Rect r = R(lr);
    float& off = m_scrolls[id];
    float maxOff = std::max(0.f, contentH - r.h);
    if (r.Contains(in.mx, in.my) && in.wheel != 0) { off -= in.wheel * 60.f; }
    off = std::max(0.f, std::min(maxOff, off));
    m_scrollActive = true; m_scrollRect = r; m_scrollId = id; m_scrollMax = maxOff;
    g.PushClip(r);
    // scrollbar
    if (maxOff > 0) {
        float trackH = r.h - 8, thumbH = std::max(30.f, trackH * r.h / contentH);
        float ty = r.y + 4 + (trackH - thumbH) * (off / maxOff);
        float sx = g.RTL() ? r.x + 2 : r.r() - 6;
        g.FillRound({ sx, ty, 4, thumbH }, 2, WithA(pal::text3, 0.5f));
    }
    return -off;
}
void UI::EndScroll() { g.PopClip(); m_scrollActive = false; }

} // namespace isodeck
