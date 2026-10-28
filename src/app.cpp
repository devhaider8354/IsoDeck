// IsoDeck — application window & pages
#include "app.h"
#include "resource.h"
#include <dwmapi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dbt.h>
#include <cmath>
#include <algorithm>
#include <thread>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace isodeck {

static const float kTitleH = 48.f, kSideW = 236.f, kPad = 28.f;
static const wchar_t* kClass = L"IsoDeckWindow";
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

// ------------------------------------------------------------ lifecycle
bool App::Create(HINSTANCE inst) {
    m_inst = inst;
    QueryPerformanceFrequency(&m_freq); QueryPerformanceCounter(&m_last);
    LoadConfig();

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProcS; wc.hInstance = inst; wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP)); wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);

    UINT dpi = GetDpiForSystem();
    int w = MulDiv(1120, dpi, 96), h = MulDiv(720, dpi, 96);
    int sx = (GetSystemMetrics(SM_CXSCREEN) - w) / 2, sy = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
    m_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, kClass, T(S_APP_NAME).c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        sx, sy, w, h, nullptr, nullptr, inst, this);
    if (!m_hwnd) return false;

    BOOL dark = TRUE; DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DWORD corner = 2; DwmSetWindowAttribute(m_hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    MARGINS m{ 0, 0, 1, 0 }; DwmExtendFrameIntoClientArea(m_hwnd, &m);
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    m_gfx.SetDpi((float)GetDpiForWindow(m_hwnd));
    if (!m_gfx.Init(m_hwnd)) return false;
    m_gfx.SetRTL(IsRTL());
    m_ui = new UI(m_gfx, m_in);

    m_pkgMissing = !Engine::I().PackageEmbedded();
    if (!m_pkgMissing) std::thread([] { Engine::I().EnsureExtracted(); }).detach();   // warm up in background
    m_catalog = LoadCatalog();
    RefreshDisks();
    ShowWindow(m_hwnd, SW_SHOW);
    SetTimer(false);
    if (!IsRunningAsAdmin()) Toast(T(S_ADMIN_NEEDED), false);
    return true;
}

int App::Run() {
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    SaveConfig();
    return (int)msg.wParam;
}

void App::SetTimer(bool fast) {
    if (fast == m_fastTimer && m_fastTimer) return;
    ::SetTimer(m_hwnd, 1, fast ? 16 : 400, nullptr);
    m_fastTimer = fast;
}

LRESULT CALLBACK App::WndProcS(HWND h, UINT m, WPARAM w, LPARAM l) {
    App* self = (App*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE) { self = (App*)((CREATESTRUCTW*)l)->lpCreateParams; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self); self->m_hwnd = h; }
    return self ? self->WndProc(h, m, w, l) : DefWindowProcW(h, m, w, l);
}

LRESULT App::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto dip = [&](int px) { return px / m_gfx.Scale(); };
    switch (msg) {
    case WM_NCCALCSIZE:
        if (wp) {
            auto* p = (NCCALCSIZE_PARAMS*)lp;
            if (IsZoomed(h)) {
                int fx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                int fy = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                p->rgrc[0].left += fx; p->rgrc[0].right -= fx; p->rgrc[0].top += fy; p->rgrc[0].bottom -= fy;
            }
            return 0;
        }
        break;
    case WM_NCHITTEST: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; ScreenToClient(h, &pt);
        RECT rc; GetClientRect(h, &rc);
        int b = (int)(6 * m_gfx.Scale());
        if (!IsZoomed(h)) {
            bool L = pt.x < b, R = pt.x >= rc.right - b, T = pt.y < b, B = pt.y >= rc.bottom - b;
            if (T && L) return HTTOPLEFT; if (T && R) return HTTOPRIGHT; if (B && L) return HTBOTTOMLEFT; if (B && R) return HTBOTTOMRIGHT;
            if (L) return HTLEFT; if (R) return HTRIGHT; if (T) return HTTOP; if (B) return HTBOTTOM;
        }
        float x = dip(pt.x), y = dip(pt.y);
        if (y < kTitleH) {
            // window buttons occupy 2 x 46 px at the trailing edge (leading when RTL)
            float bw = 92.f;
            bool onButtons = m_gfx.RTL() ? (x < bw) : (x > m_gfx.Width() - bw);
            if (!onButtons) return HTCAPTION;
        }
        return HTCLIENT; }
    case WM_GETMINMAXINFO: {
        auto* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = (int)(940 * m_gfx.Scale()); mmi->ptMinTrackSize.y = (int)(600 * m_gfx.Scale());
        return 0; }
    case WM_SIZE:
        m_gfx.Resize(LOWORD(lp), HIWORD(lp));
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED: {
        m_gfx.SetDpi((float)HIWORD(wp));
        RECT* r = (RECT*)lp;
        SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0; }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h, &ps); Render(); EndPaint(h, &ps); return 0; }
    case WM_TIMER: Frame(); return 0;
    case WM_APP + 1: Frame(); return 0;
    case WM_MOUSEMOVE: {
        m_in.mx = dip(GET_X_LPARAM(lp)); m_in.my = dip(GET_Y_LPARAM(lp));
        TRACKMOUSEEVENT t{ sizeof(t), TME_LEAVE, h, 0 }; TrackMouseEvent(&t);
        InvalidateRect(h, nullptr, FALSE); return 0; }
    case WM_MOUSELEAVE: m_in.mx = m_in.my = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        SetCapture(h); m_in.down = true; m_in.pressed = true;
        m_in.mx = dip(GET_X_LPARAM(lp)); m_in.my = dip(GET_Y_LPARAM(lp));
        SetFocus(h); Frame(); return 0;
    case WM_LBUTTONUP:
        ReleaseCapture(); m_in.down = false; m_in.released = true;
        m_in.mx = dip(GET_X_LPARAM(lp)); m_in.my = dip(GET_Y_LPARAM(lp));
        Frame(); return 0;
    case WM_MOUSEWHEEL: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; ScreenToClient(h, &pt);
        m_in.mx = dip(pt.x); m_in.my = dip(pt.y);
        m_in.wheel += GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        Frame(); return 0; }
    case WM_CHAR: m_in.chars.push_back((wchar_t)wp); Frame(); return 0;
    case WM_KEYDOWN:
        m_in.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        m_in.keys.push_back((int)wp);
        if (wp == VK_ESCAPE && m_modal.open && m_modal.withCancel) m_modal.open = false;
        if (wp == VK_F5 && !m_engineBusy) { RefreshDisks(); RefreshIsos(); }
        Frame(); return 0;
    case WM_DROPFILES: {
        HDROP d = (HDROP)wp; UINT n = DragQueryFileW(d, 0xFFFFFFFF, nullptr, 0);
        std::vector<Str> files;
        for (UINT i = 0; i < n; ++i) { wchar_t p[MAX_PATH]; DragQueryFileW(d, i, p, MAX_PATH); files.push_back(p); }
        DragFinish(d);
        SetPage(PG_ISOS); AddIsoFiles(files);
        return 0; }
    case WM_DEVICECHANGE:
        if (wp == DBT_DEVNODES_CHANGED && !m_engineBusy) { m_isoRefreshTick = GetTickCount() + 1200; }
        return TRUE;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) { SetCursor(LoadCursor(nullptr, IDC_ARROW)); return TRUE; }
        break;
    case WM_CLOSE:
        if (m_engineBusy || Jobs::I().Busy()) {
            ShowModal(T(S_CLOSE), T(S_BUSY_EXIT), T(S_CLOSE), BtnStyle::Danger, [this] { Jobs::I().Cancel(); DestroyWindow(m_hwnd); }, true, Gfx::Icon::Warning);
            return 0;
        }
        DestroyWindow(h); return 0;
    case WM_DESTROY: SaveConfig(); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void App::RunPending() {
    if (!m_pending) return;
    m_pending = false;
    InstallOptions o = m_pendingOpt;
    m_extracting = true; Render();               // show "extracting" state first (blocking step follows)
    bool ok = Engine::I().EnsureExtracted();
    m_extracting = false;
    if (!ok) { Toast(T(S_DONE_FAIL), false); return; }
    if (Engine::I().Start(o)) { m_engineBusy = true; m_lastResult = -1; m_engineMsg.clear(); }
    else Toast(T(S_DONE_FAIL), false);
}

void App::Frame() {
    if (m_deferred) { auto f = std::move(m_deferred); m_deferred = nullptr; f(); }
    RunPending();
    // deferred device refresh (debounced)
    if (m_isoRefreshTick && GetTickCount() >= m_isoRefreshTick) {
        m_isoRefreshTick = 0; RefreshDisks(); RefreshIsos();
        if (m_autoTheme) {                       // right after a successful prepare: brand the drive immediately
            if (!m_isoDrive.empty()) { Str err; SaveConfig(); if (ApplyTheme(m_isoDrive, m_theme, &err)) m_autoTheme = false; else SLOG(L"auto theme failed: %s", err.c_str()); }
            if (++m_autoThemeTries > 5) m_autoTheme = false; else if (m_autoTheme) m_isoRefreshTick = GetTickCount() + 1500;
        }
    }
    if (Jobs::I().TakeFinishedFlag()) {
        JobProgress p = Jobs::I().Snapshot();
        if (p.kind == JobKind::Hash && !p.hashResult.empty()) {
            if (OpenClipboard(m_hwnd)) { EmptyClipboard(); size_t n = (p.hashResult.size() + 1) * 2; HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, n); if (g) { memcpy(GlobalLock(g), p.hashResult.c_str(), n); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); } CloseClipboard(); }
            ShowModal(T(S_VERIFY_SHA), Format(T(S_SHA_RESULT).c_str(), p.hashFile.c_str(), p.hashResult.c_str()), T(S_OK_BTN), BtnStyle::Primary, nullptr, false, Gfx::Icon::Hash);
        } else {
            Toast((p.kind == JobKind::Download ? (p.lastOk ? T(S_DOWNLOAD_DONE) : T(S_DOWNLOAD_FAIL)) : (p.lastOk ? T(S_COPYING) + L" ✓" : T(S_DONE_FAIL))) + L"  " + p.lastResult, p.lastOk);
            RefreshIsos();
        }
    }
    // engine state transitions
    bool running = Engine::I().Status().running;
    if (m_engineBusy && !running) {
        m_engineBusy = false;
        m_lastResult = Engine::I().Status().result;
        m_engineMsg = Engine::I().Message();
        Toast(m_lastResult == 0 ? T(S_DONE_OK) : T(S_DONE_FAIL), m_lastResult == 0);
        m_isoRefreshTick = GetTickCount() + 1500;
        if (m_lastResult == 0) { m_autoTheme = true; m_autoThemeTries = 0; }
    }
    Render();
    if (m_pending || m_deferred) PostMessageW(m_hwnd, WM_APP + 1, 0, 0);
}

// --------------------------------------------------------------- config
void App::LoadConfig() {
    m_cfg.Load(JoinPath(AppDataDir(), L"config.txt"));
    SetLang(m_cfg.GetInt(L"lang", 0) == 1 ? Lang::EN : Lang::AR);
    m_theme.LoadFrom(m_cfg);
    m_timeoutText = Format(L"%d", m_theme.timeoutSec);
    m_opt.gpt = m_cfg.GetInt(L"opt.gpt", 1) != 0;
    m_opt.secureBoot = m_cfg.GetInt(L"opt.sb", 1) != 0;
    m_opt.fs = m_cfg.Get(L"opt.fs", L"exFAT");
    m_reserveText = m_cfg.Get(L"opt.reserve", L"0");
}
void App::SaveConfig() {
    m_cfg.SetInt(L"lang", GetLang() == Lang::EN ? 1 : 0);
    m_theme.timeoutSec = std::max(0, std::min(600, _wtoi(m_timeoutText.c_str())));
    m_theme.SaveTo(m_cfg);
    m_cfg.SetInt(L"opt.gpt", m_opt.gpt ? 1 : 0);
    m_cfg.SetInt(L"opt.sb", m_opt.secureBoot ? 1 : 0);
    m_cfg.Set(L"opt.fs", m_opt.fs);
    m_cfg.Set(L"opt.reserve", m_reserveText);
    m_cfg.Save(JoinPath(AppDataDir(), L"config.txt"));
}
void App::ApplyLang(Lang l) {
    SetLang(l); m_gfx.SetRTL(IsRTL()); SaveConfig();
    SetWindowTextW(m_hwnd, T(S_APP_NAME).c_str());
}

// --------------------------------------------------------------- helpers
void App::RefreshDisks() {
    m_disks = EnumerateDisks(m_showAll);
    bool found = false;
    for (auto& d : m_disks) if (d.number == m_selDisk) found = true;
    if (!found) { m_selDisk = -1; if (m_disks.size() == 1 && !m_disks[0].isSystem) m_selDisk = m_disks[0].number; }
    RefreshIsos();
}
const DiskInfo* App::Selected() const { for (auto& d : m_disks) if (d.number == m_selDisk) return &d; return nullptr; }
Str App::DataDriveOrEmpty() const { const DiskInfo* d = Selected(); return (d && d->ventoyInstalled && !d->dataLetter.empty()) ? d->dataLetter : L""; }
void App::RefreshIsos() {
    m_isoDrive = DataDriveOrEmpty();
    m_isos.clear();
    if (m_isoDrive.empty()) return;
    // Never show the engine's default volume label to the customer.
    if (ToLower(VolumeLabel(m_isoDrive)) == L"ventoy") SetVolumeLabelW((m_isoDrive.substr(0, 2) + L"\\").c_str(), L"ISODECK");
    m_isos = ListFiles(m_isoDrive + L"\\", true, { L".iso", L".wim", L".img", L".vhd", L".vhdx", L".efi", L".vtoy" }, L"ventoy");
    for (auto& d : m_disks) if (d.number == m_selDisk && !d.dataLetter.empty()) d.freeBytes = VolumeFree(d.dataLetter);
    EnsureAliasEntries();
}
void App::EnsureAliasEntries() {
    for (auto& f : m_isos) {
        Str rel = DriveRelPath(m_isoDrive, f);
        bool has = false;
        for (auto& a : m_theme.aliases) if (ToLower(a.relPath) == ToLower(rel)) { has = true; break; }
        if (!has) { AliasEntry e; e.relPath = rel; m_theme.aliases.push_back(e); }
    }
}
void App::Toast(const Str& msg, bool ok) { m_toast.text = msg; m_toast.ok = ok; m_toast.until = m_time + 4.5; m_ui->RequestAnim(); }
void App::ShowModal(const Str& title, const Str& text, const Str& ok, BtnStyle style, std::function<void()> onOk, bool withCancel, Gfx::Icon icon) {
    m_modal.open = true; m_modal.title = title; m_modal.text = text; m_modal.ok = ok; m_modal.style = style; m_modal.onOk = onOk; m_modal.withCancel = withCancel; m_modal.icon = icon;
    m_ui->ClearFocus();
}
void App::SetPage(int p) { if (p != m_page) { m_page = p; m_pageAnim = 0.f; m_ui->ClearFocus(); if (p == PG_ISOS || p == PG_THEME) RefreshIsos(); } }

void App::StartInstall(bool update) {
    const DiskInfo* d = Selected();
    if (!d) { Toast(T(S_SELECT_DEVICE_FIRST), false); return; }
    if (d->isSystem) return;
    if (m_pkgMissing) { ShowModal(T(S_NEED_PKG), T(S_NEED_PKG_HINT), T(S_OK_BTN), BtnStyle::Primary, nullptr, false, Gfx::Icon::Warning); return; }
    InstallOptions o = m_opt;
    o.physDrive = d->number; o.update = update; o.noUsbCheck = !d->isUsb;
    o.reserveMB = std::max(0, _wtoi(m_reserveText.c_str()));
    Str title = d->Title() + (d->letters.empty() ? L"" : L"  [" + d->letters[0] + L"]");
    auto go = [this, o] { m_pending = true; m_pendingOpt = o; };
    if (update) go();
    else ShowModal(T(S_INSTALL_WARN_TITLE), Format(T(S_INSTALL_WARN).c_str(), title.c_str()), T(S_YES_ERASE), BtnStyle::Danger, go, true, Gfx::Icon::Warning);
}

void App::AddIsoFiles(const std::vector<Str>& files) {
    Str drive = DataDriveOrEmpty();
    if (drive.empty()) { Toast(T(S_NOT_VENTOY_DISK), false); return; }
    for (auto& f : files) {
        bool okExt = false;
        for (auto e : { L".iso", L".wim", L".img", L".vhd", L".vhdx", L".efi" }) if (EndsWithNoCase(f, e)) okExt = true;
        if (!okExt) continue;
        Job j; j.kind = JobKind::Copy; j.source = f; j.dest = drive + L"\\" + FileName(f); j.label = FileName(f);
        Jobs::I().Enqueue(j);
    }
}

// --------------------------------------------------------------- render
void App::Render() {
    if (m_inRender) return;                  // a modal dialog may pump WM_PAINT while we are drawing
    struct Guard { bool& f; Guard(bool& b) : f(b) { f = true; } ~Guard() { f = false; } } guard(m_inRender);
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - m_last.QuadPart) / (float)m_freq.QuadPart; m_last = now;
    m_time += dt;
    if (!m_gfx.Begin()) return;
    m_ui->BeginFrame(dt);
    if (m_pageAnim < 1.f) { m_pageAnim = std::min(1.f, m_pageAnim + dt * 5.f); m_ui->RequestAnim(); }

    m_gfx.Clear(pal::bg);
    // subtle radial-ish glow top-right (or top-left when RTL)
    float gx = m_gfx.RTL() ? 0 : m_gfx.Width(), gy = 0;
    for (int i = 6; i >= 1; --i) m_gfx.FillCircle(gx, gy, 90.f * i, WithA(pal::accent, 0.012f));

    // While a modal is open the pages must not see any input.
    Input saved = m_in;
    if (m_modal.open) { m_in.pressed = m_in.released = false; m_in.wheel = 0; m_in.chars.clear(); m_in.keys.clear(); m_in.mx = m_in.my = -1; }

    DrawSidebar();
    DrawTitleBar();
    Rect area{ kSideW, kTitleH, m_gfx.Width() - kSideW, m_gfx.Height() - kTitleH };
    // slide-in page animation
    float slide = (1.f - m_pageAnim) * 18.f;
    Rect content{ area.x + kPad, area.y + kPad - slide + 10, area.w - 2 * kPad, area.h - 2 * kPad + slide };
    m_gfx.PushClip(m_ui->R(area));
    switch (m_page) {
    case PG_DEVICES: DrawDevices(content); break;
    case PG_ISOS:    DrawIsos(content); break;
    case PG_THEME:   DrawTheme(content); break;
    case PG_CATALOG: DrawCatalog(content); break;
    case PG_ABOUT:   DrawAbout(content); break;
    }
    m_gfx.PopClip();
    if (m_modal.open) m_in = saved;
    DrawModal();
    DrawToast();

    m_in.EndFrame();
    m_gfx.End();
    bool busy = m_engineBusy || Jobs::I().Busy() || m_extracting || m_toast.until > m_time;
    SetTimer(m_ui->NeedsAnimation() || busy || m_pageAnim < 1.f);
}

void App::DrawTitleBar() {
    UI& ui = *m_ui;
    Rect bar{ kSideW, 0, m_gfx.Width() - kSideW, kTitleH };
    // buttons at the trailing edge
    float bx = m_gfx.Width() - 46;
    if (ui.IconButton(MakeId(L"close"), { bx, 0, 46, kTitleH }, Gfx::Icon::Close, pal::text2, true, WithA(pal::red, 0.85f))) PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    if (ui.IconButton(MakeId(L"min"), { bx - 46, 0, 46, kTitleH }, Gfx::Icon::Minimize, pal::text2)) ShowWindow(m_hwnd, SW_MINIMIZE);
    // status text (engine version) next to the window buttons
    Str ver = Engine::I().Version();
    if (!ver.empty()) {
        TextStyle ts; ts.size = 11.5f; ts.align = Align::End; ts.forceLTR = true;
        Rect tr = ui.R({ bar.x, 0, bar.w - 108, kTitleH });
        m_gfx.Text(tr, L"engine " + ver, ts, pal::text3);
    }
}

void App::DrawSidebar() {
    UI& ui = *m_ui;
    Rect side{ 0, 0, kSideW, m_gfx.Height() };
    m_gfx.FillRect(ui.R(side), pal::side);
    m_gfx.Line(ui.R(side).x + (m_gfx.RTL() ? 0 : kSideW), 0, ui.R(side).x + (m_gfx.RTL() ? 0 : kSideW), m_gfx.Height(), pal::border);

    // logo
    Rect logo{ 20, 16, 40, 40 };
    Rect lr = ui.R(logo);
    m_gfx.Shadow(lr, 12, 10, WithA(pal::accent, 0.35f));
    m_gfx.GradientRound(lr, 12, pal::accent, pal::accent2, true);
    m_gfx.DrawIcon(Gfx::Icon::Bolt, lr.Inset(7), pal::white);
    ui.Label({ 70, 14, kSideW - 90, 24 }, T(S_APP_NAME), 17, pal::text, Weight::Bold);
    ui.Label({ 70, 38, kSideW - 90, 18 }, T(S_APP_TAG), 11.5f, pal::text3);

    struct Nav { S name; Gfx::Icon icon; };
    static const Nav nav[PG_COUNT] = { { S_NAV_DEVICES, Gfx::Icon::Usb }, { S_NAV_ISOS, Gfx::Icon::Disc }, { S_NAV_THEME, Gfx::Icon::Palette }, { S_NAV_CATALOG, Gfx::Icon::Download }, { S_NAV_ABOUT, Gfx::Icon::Gear } };
    float y = 88;
    for (int i = 0; i < PG_COUNT; ++i) {
        Rect r{ 12, y, kSideW - 24, 44 };
        Rect mr = ui.R(r);
        bool sel = (m_page == i), hover = ui.Hover(mr);
        float t = ui.Anim(MakeId(L"nav", i), sel, 12.f), ht = ui.Anim(MakeId(L"navh", i), hover);
        if (t > 0.01f) m_gfx.FillRound(mr, 10, WithA(pal::accent, 0.16f * t));
        if (ht > 0.01f && !sel) m_gfx.FillRound(mr, 10, WithA(pal::white, 0.05f * ht));
        if (t > 0.01f) { Rect ind = m_gfx.RTL() ? Rect{ mr.r() - 3, mr.y + 12, 3, 20 } : Rect{ mr.x, mr.y + 12, 3, 20 }; m_gfx.FillRound(ind, 1.5f, WithA(pal::accent, t)); }
        Color ic = Lerp(pal::text2, pal::accent, t);
        Rect ir = m_gfx.RTL() ? Rect{ mr.r() - 40, mr.y + 12, 20, 20 } : Rect{ mr.x + 16, mr.y + 12, 20, 20 };
        m_gfx.DrawIcon(nav[i].icon, ir, ic, 1.7f);
        TextStyle ts; ts.size = 14; ts.weight = sel ? Weight::SemiBold : Weight::Medium;
        Rect tr = m_gfx.RTL() ? Rect{ mr.x + 12, mr.y, mr.w - 60, mr.h } : Rect{ mr.x + 48, mr.y, mr.w - 60, mr.h };
        m_gfx.Text(tr, T(nav[i].name), ts, Lerp(pal::text2, pal::text, std::max(t, ht)));
        if (ui.Clicked(MakeId(L"navc", i), mr)) SetPage(i);
        y += 48;
    }

    // selected drive summary at the bottom
    const DiskInfo* d = Selected();
    Rect box{ 12, m_gfx.Height() - 112, kSideW - 24, 96 };
    ui.Card(box, 12, pal::card, true);
    if (d) {
        ui.Label({ box.x + 14, box.y + 10, box.w - 28, 18 }, T(S_NAV_DEVICES), 11, pal::text3);
        ui.Label({ box.x + 14, box.y + 28, box.w - 28, 22 }, d->Title(), 13, pal::text, Weight::SemiBold, Align::Start, true);
        Str st = d->ventoyInstalled ? T(S_VENTOY_INSTALLED) : T(S_VENTOY_NOT_INSTALLED);
        ui.Chip({ box.x + 14, box.y + 58, m_gfx.MeasureWidth(st, TextStyle{ 11.5f, Weight::SemiBold }) + 20, 22 }, st, WithA(d->ventoyInstalled ? pal::green : pal::amber, 0.18f), d->ventoyInstalled ? pal::green : pal::amber);
    } else {
        ui.Label({ box.x + 14, box.y + 10, box.w - 28, 18 }, T(S_NAV_DEVICES), 11, pal::text3);
        ui.Para({ box.x + 14, box.y + 30, box.w - 28, 60 }, T(S_SELECT_DEVICE_FIRST), 12.5f, pal::text2);
    }
}

// ------------------------------------------------------------- devices
void App::DrawDevices(const Rect& a) {
    UI& ui = *m_ui;
    float y = a.y;
    ui.Label({ a.x, y, a.w - 260, 32 }, T(S_DEVICES_TITLE), 24, pal::text, Weight::Bold);
    Gfx::Icon ref = Gfx::Icon::Refresh;
    if (ui.Button(MakeId(L"refresh"), { a.r() - 120, y, 120, 36 }, T(S_REFRESH), BtnStyle::Secondary, &ref, !m_engineBusy)) RefreshDisks();
    y += 36;
    ui.Para({ a.x, y, a.w - 260, 40 }, T(S_DEVICES_SUB), 13, pal::text2);
    bool prevAll = m_showAll;
    ui.Toggle(MakeId(L"showall"), { a.r() - 240, y - 2, 240, 30 }, T(S_SHOW_ALL_DISKS), m_showAll, !m_engineBusy);
    if (prevAll != m_showAll) RefreshDisks();
    y += 48;

    // scrollable body
    Rect body{ a.x, y, a.w, a.b() - y };
    float cardW = (a.w - 16) / 2, cardH = 96;
    int n = (int)m_disks.size();
    float rows = (float)((n + 1) / 2);
    float optionsH = 0;
    const DiskInfo* sel = Selected();
    bool showOptions = sel && !sel->isSystem;
    if (showOptions) optionsH = 330 + (m_engineBusy || m_lastResult >= 0 || m_extracting ? 120 : 0);
    float contentH = (n == 0 ? 200 : rows * (cardH + 14)) + optionsH + 30;
    float off = ui.BeginScroll(MakeId(L"devscroll"), body, contentH);
    float cy = body.y + off;

    if (n == 0) {
        Rect e{ a.x, cy, a.w, 180 };
        ui.Card(e, 16, pal::card);
        Rect ic = ui.R({ a.x + a.w / 2 - 24, cy + 36, 48, 48 });
        m_gfx.DrawIcon(Gfx::Icon::Usb, ic, pal::text3, 2);
        ui.Label({ a.x, cy + 96, a.w, 24 }, T(S_NO_DEVICES), 16, pal::text, Weight::SemiBold, Align::Center);
        ui.Label({ a.x, cy + 122, a.w, 20 }, T(S_NO_DEVICES_HINT), 13, pal::text3, Weight::Regular, Align::Center);
        cy += 200;
    }
    for (int i = 0; i < n; ++i) {
        const DiskInfo& d = m_disks[i];
        Rect r{ a.x + (i % 2) * (cardW + 16), cy + (i / 2) * (cardH + 14), cardW, cardH };
        Rect mr = ui.R(r);
        bool isSel = d.number == m_selDisk, hover = ui.Hover(mr) && !d.isSystem;
        float t = ui.Anim(MakeId(L"dsel", d.number), isSel, 12.f), ht = ui.Anim(MakeId(L"dhov", d.number), hover);
        if (t > 0.01f) m_gfx.Shadow(mr, 14, 12, WithA(pal::accent, 0.22f * t));
        m_gfx.FillRound(mr, 14, Lerp(pal::card, pal::card2, std::max(t, ht * 0.5f)));
        m_gfx.StrokeRound(mr, 14, Lerp(pal::border, pal::accent, t), 1 + t);
        Rect ic = m_gfx.RTL() ? Rect{ mr.r() - 66, mr.y + 22, 52, 52 } : Rect{ mr.x + 14, mr.y + 22, 52, 52 };
        m_gfx.FillRound(ic, 12, WithA(d.isSystem ? pal::red : pal::accent, 0.12f));
        m_gfx.DrawIcon(Gfx::Icon::Usb, ic.Inset(12), d.isSystem ? pal::red : Lerp(pal::text2, pal::accent, t), 1.8f);
        Rect tx = m_gfx.RTL() ? Rect{ mr.x + 14, mr.y + 14, mr.w - 94, 24 } : Rect{ mr.x + 80, mr.y + 14, mr.w - 94, 24 };
        TextStyle ts; ts.size = 14.5f; ts.weight = Weight::SemiBold; ts.forceLTR = true;
        m_gfx.Text(tx, d.Title(), ts, pal::text);
        Str line2 = Format(L"PhysicalDrive%d", d.number);
        if (!d.letters.empty()) { line2 += L"  ·  "; for (size_t k = 0; k < d.letters.size(); ++k) line2 += (k ? L" " : L"") + d.letters[k]; }
        if (!d.dataLetter.empty()) line2 += L"  ·  " + T(S_FREE_SPACE) + L" " + HumanSize(d.freeBytes);
        TextStyle t2; t2.size = 12; t2.forceLTR = true;
        m_gfx.Text({ tx.x, tx.y + 26, tx.w, 18 }, line2, t2, pal::text3);
        Str st = d.isSystem ? T(S_SYSTEM_DISK) : d.ventoyInstalled ? T(S_VENTOY_INSTALLED) : T(S_VENTOY_NOT_INSTALLED);
        Color sc = d.isSystem ? pal::red : d.ventoyInstalled ? pal::green : pal::amber;
        float cw = m_gfx.MeasureWidth(st, TextStyle{ 11.5f, Weight::SemiBold }) + 20;
        Rect chip = m_gfx.RTL() ? Rect{ tx.x + tx.w - cw, tx.y + 50, cw, 22 } : Rect{ tx.x, tx.y + 50, cw, 22 };
        m_gfx.FillRound(chip, 11, WithA(sc, 0.16f));
        TextStyle t3; t3.size = 11.5f; t3.weight = Weight::SemiBold; t3.align = Align::Center;
        m_gfx.Text(chip, st, t3, sc);
        if (!d.isSystem && !m_engineBusy && ui.Clicked(MakeId(L"dclick", d.number), mr)) { m_selDisk = d.number; m_lastResult = -1; RefreshIsos(); }
    }
    cy += (n == 0 ? 0 : rows * (cardH + 14)) + 10;

    if (showOptions) {
        Rect card{ a.x, cy, a.w, 300 };
        ui.Card(card, 16, pal::card);
        float x = card.x + 22, yy = card.y + 18;
        bool en = !m_engineBusy && !m_extracting;
        ui.Label({ x, yy, card.w - 44, 24 }, T(S_OPTIONS), 16, pal::text, Weight::SemiBold); yy += 34;
        float colW = (card.w - 44 - 24) / 2;
        // column 1: partition style
        ui.Label({ x, yy, colW, 20 }, T(S_PART_STYLE), 12.5f, pal::text3);
        ui.Label({ x + colW + 24, yy, colW, 20 }, T(S_FILESYSTEM), 12.5f, pal::text3); yy += 24;
        if (ui.Radio(MakeId(L"gpt"), { x, yy, colW, 40 }, T(S_GPT), m_opt.gpt, en)) m_opt.gpt = true;
        std::vector<Str> fss = { L"exFAT", L"NTFS", L"FAT32" };
        int fsi = 0; for (int i = 0; i < 3; ++i) if (ToLower(fss[i]) == ToLower(m_opt.fs)) fsi = i;
        int nfs = ui.Segmented(MakeId(L"fs"), { x + colW + 24, yy, colW, 40 }, fss, fsi, en);
        if (nfs != fsi) m_opt.fs = fss[nfs];
        yy += 46;
        if (ui.Radio(MakeId(L"mbr"), { x, yy, colW, 40 }, T(S_MBR), !m_opt.gpt, en)) m_opt.gpt = false;
        ui.Toggle(MakeId(L"sb"), { x + colW + 24, yy, colW, 40 }, T(S_SECURE_BOOT), m_opt.secureBoot, en);
        yy += 52;
        ui.Label({ x, yy + 10, colW - 130, 20 }, T(S_RESERVE), 12.5f, pal::text2);
        ui.TextField(MakeId(L"reserve"), { x + colW - 120, yy, 120, 38 }, m_reserveText, L"0", true, true, en);
        ui.Label({ x + colW + 24, yy + 10, colW, 20 }, T(S_RESERVE_HINT), 12, pal::text3);
        yy += 56;
        Gfx::Icon bolt = Gfx::Icon::Bolt, shield = Gfx::Icon::Refresh;
        if (ui.Button(MakeId(L"install"), { x, yy, colW, 44 }, T(S_INSTALL), BtnStyle::Primary, &bolt, en)) StartInstall(false);
        if (sel->ventoyInstalled && ui.Button(MakeId(L"update"), { x + colW + 24, yy, colW, 44 }, T(S_UPDATE), BtnStyle::Secondary, &shield, en)) StartInstall(true);
        cy += 314;

        if (m_engineBusy || m_lastResult >= 0 || m_extracting) {
            Rect pc{ a.x, cy, a.w, 108 };
            ui.Card(pc, 16, pal::card);
            float px = pc.x + 22, py = pc.y + 16;
            int pct = Engine::I().Status().percent;
            Str msg = m_extracting ? T(S_PREPARING) : m_engineBusy ? Engine::I().Message() : (m_lastResult == 0 ? T(S_DONE_OK) : T(S_DONE_FAIL));
            if (m_engineBusy || m_extracting) {
                ui.Spinner(ui.R({ px + 12, py + 12, 0, 0 }).x, py + 12, 9, pal::accent);
                ui.Label({ px + 34, py, pc.w - 200, 24 }, T(S_WORKING) + Format(L"  %d%%", pct), 14, pal::text, Weight::SemiBold);
            } else {
                Rect ir = ui.R({ px, py, 24, 24 });
                m_gfx.DrawIcon(m_lastResult == 0 ? Gfx::Icon::Check : Gfx::Icon::Warning, ir, m_lastResult == 0 ? pal::green : pal::red, 2);
                ui.Label({ px + 34, py, pc.w - 200, 24 }, msg, 14, m_lastResult == 0 ? pal::green : pal::red, Weight::SemiBold);
            }
            if (!m_engineBusy && !m_extracting && ui.Button(MakeId(L"openlog"), { pc.r() - 140, py - 4, 120, 32 }, T(S_OPEN_LOG), BtnStyle::Ghost))
                OpenUrl(FileExists(Engine::I().LogPath()) ? Engine::I().LogPath() : JoinPath(AppDataDir(), L"isodeck.log"));
            ui.Progress({ px, py + 40, pc.w - 44, 8 }, m_engineBusy || m_extracting ? (m_extracting ? 0.f : pct / 100.f) : (m_lastResult == 0 ? 1.f : 0.f), m_lastResult == 1 && !m_engineBusy ? pal::red : pal::accent);
            if (m_engineBusy) { TextStyle t2; t2.size = 11.5f; t2.forceLTR = true; m_gfx.Text(ui.R({ px, py + 58, pc.w - 44, 20 }), Engine::I().Message(), t2, pal::text3); }
            else if (m_lastResult == 1) { TextStyle t2; t2.size = 11.5f; t2.forceLTR = true; m_gfx.Text(ui.R({ px, py + 58, pc.w - 44, 20 }), m_engineMsg, t2, pal::text3); }
            cy += 122;
        }
    }
    ui.EndScroll();
}

// ---------------------------------------------------------------- isos
void App::DrawJobBar(const Rect& r) {
    UI& ui = *m_ui;
    JobProgress p = Jobs::I().Snapshot();
    if (!p.active) return;
    ui.RequestAnim();
    ui.Card(r, 14, pal::card2, true);
    float x = r.x + 18, y = r.y + 12;
    Str kind = p.kind == JobKind::Copy ? T(S_COPYING) : p.kind == JobKind::Download ? T(S_DOWNLOADING) : T(S_VERIFY_SHA);
    ui.Label({ x, y, 160, 20 }, kind + (p.queued ? Format(L" (+%d)", p.queued) : L""), 12.5f, pal::text3);
    ui.Label({ x + 160, y, r.w - 360, 20 }, p.label, 13, pal::text, Weight::SemiBold, Align::Start, true);
    float pct = p.total ? (float)p.done / (float)p.total : 0.f;
    Str right = p.total ? Format(L"%s / %s", HumanSize(p.done).c_str(), HumanSize(p.total).c_str()) : HumanSize(p.done);
    if (p.speedBps > 0) right += L"  ·  " + HumanSize((uint64_t)p.speedBps) + L"/s";
    ui.Label({ r.r() - 340, y, 240, 20 }, right, 12, pal::text2, Weight::Regular, Align::End, true);
    if (ui.IconButton(MakeId(L"jobcancel"), { r.r() - 46, y - 6, 32, 32 }, Gfx::Icon::Close, pal::text2)) Jobs::I().Cancel();
    ui.Progress({ x, y + 30, r.w - 36, 8 }, pct);
}

void App::DrawIsos(const Rect& a) {
    UI& ui = *m_ui;
    float y = a.y;
    ui.Label({ a.x, y, a.w - 320, 32 }, T(S_ISOS_TITLE), 24, pal::text, Weight::Bold);
    Gfx::Icon plus = Gfx::Icon::Plus, folder = Gfx::Icon::Folder;
    bool have = !m_isoDrive.empty();
    if (ui.Button(MakeId(L"addiso"), { a.r() - 150, y, 150, 36 }, T(S_ADD_ISO), BtnStyle::Primary, &plus, have)) {
        m_deferred = [this] {
            std::vector<Str> files;
            PickFile(m_hwnd, L"Boot images (*.iso;*.wim;*.img;*.vhd;*.vhdx;*.efi)\0*.iso;*.wim;*.img;*.vhd;*.vhdx;*.efi\0All files\0*.*\0\0", true, &files);
            AddIsoFiles(files);
        };
    }
    if (ui.Button(MakeId(L"opendrive"), { a.r() - 150 - 12 - 130, y, 130, 36 }, T(S_OPEN_FOLDER), BtnStyle::Secondary, &folder, have)) OpenUrl(m_isoDrive + L"\\");
    y += 36;
    ui.Para({ a.x, y, a.w - 320, 40 }, T(S_ISOS_SUB), 13, pal::text2);
    const DiskInfo* d = Selected();
    if (d && have) {
        Str info = d->dataLetter + L"  ·  " + T(S_FREE_SPACE) + L" " + HumanSize(d->freeBytes);
        ui.Label({ a.r() - 300, y + 4, 300, 20 }, info, 12.5f, pal::text3, Weight::Regular, Align::End, true);
    }
    y += 48;

    bool jobActive = Jobs::I().Snapshot().active;
    float bottomH = jobActive ? 76 : 0;
    Rect body{ a.x, y, a.w, a.b() - y - bottomH };
    if (!have) {
        ui.Card(body, 16, pal::card);
        ui.Label({ body.x, body.y + body.h / 2 - 16, body.w, 32 }, T(S_NOT_VENTOY_DISK), 15, pal::text2, Weight::SemiBold, Align::Center);
        return;
    }
    float rowH = 64;
    float contentH = m_isos.empty() ? 220 : m_isos.size() * (rowH + 8);
    float off = ui.BeginScroll(MakeId(L"isoscroll"), body, contentH);
    float cy = body.y + off;
    if (m_isos.empty()) {
        Rect e{ a.x, cy, a.w, 200 };
        Rect me = ui.R(e);
        m_gfx.FillRound(me, 16, pal::card);
        // dashed-look border
        m_gfx.StrokeRound(me, 16, WithA(pal::accent, 0.35f), 1.5f);
        m_gfx.DrawIcon(Gfx::Icon::Disc, ui.R({ a.x + a.w / 2 - 24, cy + 40, 48, 48 }), pal::text3, 2);
        ui.Label({ a.x, cy + 100, a.w, 24 }, T(S_NO_ISOS), 16, pal::text, Weight::SemiBold, Align::Center);
        ui.Label({ a.x, cy + 128, a.w, 20 }, T(S_DROP_HINT), 13, pal::text3, Weight::Regular, Align::Center);
    }
    for (size_t i = 0; i < m_isos.size(); ++i) {
        const Str& f = m_isos[i];
        Rect r{ a.x, cy + i * (rowH + 8), a.w, rowH };
        Rect mr = ui.R(r);
        bool hover = ui.Hover(mr);
        float ht = ui.Anim(MakeId(L"isoh", (int)i), hover);
        m_gfx.FillRound(mr, 12, Lerp(pal::card, pal::card2, ht * 0.7f));
        m_gfx.StrokeRound(mr, 12, pal::border);
        Rect ic = m_gfx.RTL() ? Rect{ mr.r() - 54, mr.y + 14, 36, 36 } : Rect{ mr.x + 18, mr.y + 14, 36, 36 };
        Str lower = ToLower(f);
        Color icc = lower.find(L"win") != Str::npos ? pal::accent : lower.find(L"ubuntu") != Str::npos || lower.find(L"linux") != Str::npos || lower.find(L"mint") != Str::npos ? pal::amber : pal::green;
        m_gfx.FillRound(ic, 9, WithA(icc, 0.14f));
        m_gfx.DrawIcon(Gfx::Icon::Disc, ic.Inset(8), icc, 1.8f);
        Rect tx = m_gfx.RTL() ? Rect{ mr.x + 250, mr.y + 10, mr.w - 320, 24 } : Rect{ mr.x + 70, mr.y + 10, mr.w - 320, 24 };
        TextStyle ts; ts.size = 14; ts.weight = Weight::SemiBold; ts.forceLTR = true;
        m_gfx.Text(tx, FileName(f), ts, pal::text);
        Str rel = DriveRelPath(m_isoDrive, f);
        Str alias; for (auto& al : m_theme.aliases) if (ToLower(al.relPath) == ToLower(rel)) alias = al.alias;
        TextStyle t2; t2.size = 12; t2.forceLTR = true;
        Str sub = HumanSize(FileSize(f)) + L"  ·  " + rel + (alias.empty() ? L"" : L"  ·  " + alias);
        m_gfx.Text({ tx.x, tx.y + 26, tx.w, 18 }, sub, t2, pal::text3);
        // actions
        Gfx::Icon hash = Gfx::Icon::Hash, trash = Gfx::Icon::Trash;
        float bx = a.r() - 16;
        if (ui.Button(MakeId(L"isodel", (int)i), { bx - 100, r.y + 14, 100, 36 }, T(S_DELETE), BtnStyle::Ghost, &trash)) {
            Str path = f;
            ShowModal(T(S_DELETE), Format(T(S_DELETE_CONFIRM).c_str(), FileName(path).c_str()), T(S_DELETE), BtnStyle::Danger, [this, path] {
                if (DeleteFileSafe(path)) { Toast(T(S_DELETE) + L" ✓  " + FileName(path)); } else Toast(LastErrorText(), false);
                RefreshIsos();
            }, true, Gfx::Icon::Trash);
        }
        if (ui.Button(MakeId(L"isohash", (int)i), { bx - 100 - 8 - 110, r.y + 14, 110, 36 }, T(S_VERIFY_SHA), BtnStyle::Ghost, &hash)) {
            Job j; j.kind = JobKind::Hash; j.source = f; j.label = FileName(f); Jobs::I().Enqueue(j);
        }
    }
    ui.EndScroll();
    if (jobActive) DrawJobBar({ a.x, a.b() - 66, a.w, 66 });
}

// --------------------------------------------------------------- theme
void App::DrawTheme(const Rect& a) {
    UI& ui = *m_ui;
    float y = a.y;
    ui.Label({ a.x, y, a.w - 200, 32 }, T(S_THEME_TITLE), 24, pal::text, Weight::Bold);
    Gfx::Icon chk = Gfx::Icon::Check;
    bool have = !m_isoDrive.empty();
    if (ui.Button(MakeId(L"applytheme"), { a.r() - 170, y, 170, 36 }, T(S_APPLY_THEME), BtnStyle::Primary, &chk, have)) {
        SaveConfig();
        Str err;
        if (ApplyTheme(m_isoDrive, m_theme, &err)) Toast(T(S_THEME_APPLIED)); else Toast(T(S_THEME_FAILED) + L"  " + err, false);
    }
    y += 36;
    ui.Para({ a.x, y, a.w - 200, 40 }, T(S_THEME_SUB) + L"  " + (have ? L"" : T(S_NOT_VENTOY_DISK)), 13, have ? pal::text2 : pal::amber);
    y += 48;

    Rect body{ a.x, y, a.w, a.b() - y };
    float leftW = 360, gap = 20, rightW = a.w - leftW - gap;
    float aliasesH = 40 + (float)std::max<size_t>(1, m_isos.size()) * 52;
    float contentH = 470 + aliasesH + 40;
    float off = ui.BeginScroll(MakeId(L"themescroll"), body, contentH);
    float cy = body.y + off;

    // ---- left: settings card
    Rect card{ a.x, cy, leftW, 450 };
    ui.Card(card, 16, pal::card);
    float x = card.x + 20, yy = card.y + 18, w = card.w - 40;
    ui.Label({ x, yy, w, 20 }, T(S_BRAND_TEXT), 12.5f, pal::text3); yy += 24;
    ui.TextField(MakeId(L"brand"), { x, yy, w, 38 }, m_theme.brand, L"ISODECK", true); yy += 50;
    ui.Label({ x, yy, w, 20 }, T(S_ACCENT), 12.5f, pal::text3); yy += 26;
    for (size_t i = 0; i < m_accentPresets.size(); ++i) {
        Rect c{ x + i * 36, yy, 30, 30 };
        Rect mc = ui.R(c);
        Color col = ParseHex(m_accentPresets[i], pal::accent);
        bool selc = ToLower(m_theme.accent) == ToLower(m_accentPresets[i]);
        float t = ui.Anim(MakeId(L"acc", (int)i), selc, 12.f);
        m_gfx.FillCircle(mc.x + 15, mc.y + 15, 13, col);
        if (t > 0.01f) m_gfx.StrokeCircle(mc.x + 15, mc.y + 15, 15.5f, WithA(pal::white, t), 2);
        if (ui.Clicked(MakeId(L"accc", (int)i), mc)) m_theme.accent = m_accentPresets[i];
    }
    ui.TextField(MakeId(L"accenthex"), { x + w - 90, yy - 4, 90, 38 }, m_theme.accent, L"#00A3FF", true);
    yy += 48;
    ui.Label({ x, yy, w, 20 }, T(S_BACKGROUND), 12.5f, pal::text3); yy += 24;
    ID2D1Bitmap* bg = (!m_theme.backgroundPath.empty() && FileExists(m_theme.backgroundPath)) ? m_gfx.LoadBitmapFile(m_theme.backgroundPath) : m_gfx.LoadBitmapResource(IDR_BACKGROUND);
    Rect thumb = ui.R({ x, yy, w, 110 });
    m_gfx.FillRound(thumb, 10, pal::bg);
    m_gfx.DrawBitmapCover(bg, thumb, 10);
    m_gfx.StrokeRound(thumb, 10, pal::border);
    yy += 120;
    Gfx::Icon img = Gfx::Icon::Image;
    if (ui.Button(MakeId(L"pickbg"), { x, yy, w / 2 - 6, 36 }, T(S_CHOOSE_IMAGE), BtnStyle::Secondary, &img)) {
        m_deferred = [this] { Str p = PickImageFile(m_hwnd); if (!p.empty()) { m_theme.backgroundPath = p; m_gfx.ClearBitmapCache(); } };
    }
    if (ui.Button(MakeId(L"defbg"), { x + w / 2 + 6, yy, w / 2 - 6, 36 }, T(S_USE_DEFAULT_BG), BtnStyle::Ghost)) {
        m_deferred = [this] { m_theme.backgroundPath.clear(); m_gfx.ClearBitmapCache(); };
    }
    yy += 50;
    ui.Label({ x, yy + 9, w - 100, 20 }, T(S_MENU_TIMEOUT), 12.5f, pal::text2);
    ui.TextField(MakeId(L"timeout"), { x + w - 90, yy, 90, 38 }, m_timeoutText, L"30", true, true);
    yy += 48;
    ui.Toggle(MakeId(L"win11"), { x, yy, w, 36 }, T(S_WIN11_BYPASS), m_theme.win11Bypass);

    // ---- right: live preview of the boot menu
    Rect pv{ a.x + leftW + gap, cy, rightW, 450 };
    ui.Card(pv, 16, pal::card);
    ui.Label({ pv.x + 20, pv.y + 14, 200, 20 }, T(S_PREVIEW), 12.5f, pal::text3);
    Rect screen = ui.R({ pv.x + 20, pv.y + 40, pv.w - 40, pv.h - 60 });
    // keep 16:9
    float sw = screen.w, sh = sw * 9 / 16; if (sh > screen.h) { sh = screen.h; sw = sh * 16 / 9; }
    Rect sc{ screen.x + (screen.w - sw) / 2, screen.y, sw, sh };
    m_gfx.FillRound(sc, 8, Hex(0x0B1220));
    m_gfx.DrawBitmapCover(bg, sc, 8);
    Color acc = ParseHex(m_theme.accent, pal::accent);
    // fake GRUB menu items
    float mx = sc.x + sc.w * 0.08f, my = sc.y + sc.h * 0.24f, mw = sc.w * 0.84f, ih = std::max(16.f, sc.h * 0.062f);
    int shown = 0;
    std::vector<Str> names;
    for (auto& f : m_isos) {
        Str rel = DriveRelPath(m_isoDrive, f); Str nm = FileName(f);
        for (auto& al : m_theme.aliases) if (ToLower(al.relPath) == ToLower(rel) && !al.alias.empty()) nm = al.alias;
        names.push_back(nm); if (names.size() >= 7) break;
    }
    if (names.empty()) names = { L"Windows 11 Pro", L"Windows 10", L"Ubuntu 24.04", L"Hiren's BootCD PE" };
    for (auto& nm : names) {
        Rect ir{ mx, my + shown * (ih + 4), mw, ih };
        if (shown == 0) m_gfx.FillRound(ir, 4, WithA(acc, 0.25f));
        TextStyle ts; ts.size = std::max(9.f, ih * 0.55f); ts.forceLTR = true; ts.weight = shown == 0 ? Weight::SemiBold : Weight::Regular; ts.align = Align::Start;
        Rect tr{ ir.x + ih * 1.4f, ir.y, ir.w - ih * 1.4f, ir.h };
        // preview is always LTR (GRUB)
        bool wasRtl = m_gfx.RTL(); m_gfx.SetRTL(false);
        m_gfx.DrawIcon(Gfx::Icon::Disc, { ir.x + ih * 0.25f, ir.y + ih * 0.15f, ih * 0.7f, ih * 0.7f }, shown == 0 ? pal::white : Hex(0xC7D1E3), 1.2f);
        m_gfx.Text(tr, nm, ts, shown == 0 ? pal::white : Hex(0xC7D1E3));
        m_gfx.SetRTL(wasRtl);
        ++shown;
    }
    Rect pb{ sc.x + sc.w * 0.08f, sc.y + sc.h * 0.84f, sc.w * 0.84f, std::max(2.f, sc.h * 0.012f) };
    m_gfx.FillRound(pb, pb.h / 2, Hex(0x1A2438));
    m_gfx.FillRound({ pb.x, pb.y, pb.w * 0.6f, pb.h }, pb.h / 2, acc);
    { bool wasRtl = m_gfx.RTL(); m_gfx.SetRTL(false);
      TextStyle bs; bs.size = std::max(8.f, sc.h * 0.04f); bs.weight = Weight::Bold; bs.align = Align::Center; bs.forceLTR = true;
      m_gfx.Text({ sc.x, sc.y + sc.h * 0.87f, sc.w, sc.h * 0.06f }, m_theme.brand, bs, acc);
      TextStyle hs; hs.size = std::max(6.f, sc.h * 0.026f); hs.align = Align::Center; hs.forceLTR = true;
      m_gfx.Text({ sc.x, sc.y + sc.h * 0.925f, sc.w, sc.h * 0.05f }, L"Enter: Boot   F2: Power   F3: Tree view   F5: Tools", hs, Hex(0x7F8EA3));
      m_gfx.SetRTL(wasRtl); }
    cy += 470;

    // ---- aliases list
    Rect al{ a.x, cy, a.w, aliasesH };
    ui.Card(al, 16, pal::card);
    ui.Label({ al.x + 20, al.y + 12, al.w - 40, 22 }, T(S_MENU_ALIASES), 15, pal::text, Weight::SemiBold);
    ui.Label({ al.x + 20, al.y + 32, al.w - 40, 18 }, T(S_ALIAS_LATIN_NOTE) + L"  " + T(S_ALIAS_HINT), 11.5f, pal::text3);
    float ay = al.y + 56;
    if (m_isos.empty()) ui.Label({ al.x + 20, ay, al.w - 40, 30 }, T(S_NO_ISOS), 13, pal::text3);
    for (size_t i = 0; i < m_isos.size(); ++i) {
        Str rel = DriveRelPath(m_isoDrive, m_isos[i]);
        AliasEntry* e = nullptr;
        for (auto& en : m_theme.aliases) if (ToLower(en.relPath) == ToLower(rel)) e = &en;
        if (!e) continue;
        float ax = al.x + 20;
        ui.Label({ ax, ay + 9, 260, 20 }, FileName(m_isos[i]), 12.5f, pal::text2, Weight::Medium, Align::Start, true);
        ui.TextField(MakeId(L"alias", (int)i), { ax + 270, ay, al.w - 40 - 270 - 250, 38 }, e->alias, T(S_SHOW_IN_MENU), true);
        Str ub = e->unattendRel.empty() ? T(S_UNATTEND_PICK) : FileName(e->unattendRel);
        if (ui.Button(MakeId(L"unat", (int)i), { al.r() - 20 - 240, ay, 170, 38 }, ub, BtnStyle::Secondary)) {
            Str rel = e->relPath;
            m_deferred = [this, rel] {
                Str p = PickFile(m_hwnd, L"Unattend XML (*.xml)\0*.xml\0\0", false, nullptr);
                if (p.empty() || m_isoDrive.empty()) return;
                Str dir = m_isoDrive + L"\\ventoy\\unattend"; MakeDirs(dir);
                Str dst = JoinPath(dir, FileName(p));
                if (!CopyFileW(p.c_str(), dst.c_str(), FALSE)) { Toast(LastErrorText(), false); return; }
                for (auto& en : m_theme.aliases) if (ToLower(en.relPath) == ToLower(rel)) en.unattendRel = L"/ventoy/unattend/" + FileName(p);
            };
        }
        if (!e->unattendRel.empty() && ui.Button(MakeId(L"unatx", (int)i), { al.r() - 20 - 62, ay, 62, 38 }, T(S_UNATTEND_CLEAR), BtnStyle::Ghost)) e->unattendRel.clear();
        ay += 52;
    }
    ui.EndScroll();
}

// -------------------------------------------------------------- catalog
void App::DrawCatalog(const Rect& a) {
    UI& ui = *m_ui;
    float y = a.y;
    ui.Label({ a.x, y, a.w - 200, 32 }, T(S_CATALOG_TITLE), 24, pal::text, Weight::Bold);
    Gfx::Icon fold = Gfx::Icon::Folder;
    if (ui.Button(MakeId(L"editcat"), { a.r() - 150, y, 150, 36 }, T(S_EDIT_CATALOG), BtnStyle::Secondary, &fold)) { OpenUrl(CatalogUserPath()); }
    y += 36;
    bool have = !m_isoDrive.empty();
    ui.Para({ a.x, y, a.w - 200, 40 }, T(S_CATALOG_SUB) + L"  " + T(S_CATALOG_NOTE) + (have ? L"" : L"  " + T(S_NOT_VENTOY_DISK)), 12.5f, have ? pal::text2 : pal::amber);
    if (ui.Button(MakeId(L"reloadcat"), { a.r() - 150, y + 2, 150, 32 }, T(S_REFRESH), BtnStyle::Ghost)) m_catalog = LoadCatalog();
    y += 52;

    // custom url row
    Rect cu{ a.x, y, a.w, 50 };
    ui.Card(cu, 12, pal::card);
    ui.Label({ cu.x + 16, cu.y + 15, 150, 20 }, T(S_CUSTOM_URL), 12.5f, pal::text3);
    ui.TextField(MakeId(L"customurl"), { cu.x + 170, cu.y + 6, cu.w - 170 - 150 - 24, 38 }, m_customUrl, L"https://…/file.iso", true);
    Gfx::Icon dl = Gfx::Icon::Download;
    if (ui.Button(MakeId(L"dlcustom"), { cu.r() - 12 - 150, cu.y + 6, 150, 38 }, T(S_DOWNLOAD), BtnStyle::Primary, &dl, have && m_customUrl.size() > 8)) {
        Str name = FileName(m_customUrl); size_t q = name.find(L'?'); if (q != Str::npos) name = name.substr(0, q);
        if (name.empty() || name.find(L'.') == Str::npos) name = L"download.iso";
        Job j; j.kind = JobKind::Download; j.source = m_customUrl; j.dest = m_isoDrive + L"\\" + name; j.label = name; Jobs::I().Enqueue(j);
    }
    y += 62;

    bool jobActive = Jobs::I().Snapshot().active;
    float bottomH = jobActive ? 76 : 0;
    Rect body{ a.x, y, a.w, a.b() - y - bottomH };
    float rowH = 62;
    float off = ui.BeginScroll(MakeId(L"catscroll"), body, m_catalog.size() * (rowH + 8));
    float cy = body.y + off;
    for (size_t i = 0; i < m_catalog.size(); ++i) {
        const CatalogItem& it = m_catalog[i];
        Rect r{ a.x, cy + i * (rowH + 8), a.w, rowH };
        Rect mr = ui.R(r);
        float ht = ui.Anim(MakeId(L"cath", (int)i), ui.Hover(mr));
        m_gfx.FillRound(mr, 12, Lerp(pal::card, pal::card2, ht * 0.7f));
        m_gfx.StrokeRound(mr, 12, pal::border);
        Color cc = ToLower(it.category).find(L"windows") != Str::npos ? pal::accent : ToLower(it.category).find(L"linux") != Str::npos ? pal::amber : pal::green;
        float cw = m_gfx.MeasureWidth(it.category, TextStyle{ 11.5f, Weight::SemiBold }) + 20;
        ui.Chip({ r.x + 16, r.y + 20, cw, 22 }, it.category, WithA(cc, 0.16f), cc);
        ui.Label({ r.x + 16 + cw + 14, r.y + 10, r.w - cw - 260, 22 }, it.name, 14, pal::text, Weight::SemiBold, Align::Start, true);
        ui.Label({ r.x + 16 + cw + 14, r.y + 34, r.w - cw - 260, 18 }, it.note.empty() ? it.url : it.note, 11.5f, pal::text3, Weight::Regular, Align::Start, true);
        Gfx::Icon ic = it.direct ? Gfx::Icon::Download : Gfx::Icon::Globe;
        if (ui.Button(MakeId(L"catbtn", (int)i), { r.r() - 16 - 190, r.y + 13, 190, 36 }, it.direct ? T(S_DOWNLOAD) : T(S_OPEN_PAGE), it.direct ? BtnStyle::Primary : BtnStyle::Secondary, &ic, it.direct ? have : true)) {
            if (!it.direct) OpenUrl(it.url);
            else {
                Str name = FileName(it.url); size_t q = name.find(L'?'); if (q != Str::npos) name = name.substr(0, q);
                if (name.empty() || name.find(L'.') == Str::npos) name = it.name + L".iso";
                Job j; j.kind = JobKind::Download; j.source = it.url; j.dest = m_isoDrive + L"\\" + name; j.label = name; Jobs::I().Enqueue(j);
            }
        }
    }
    ui.EndScroll();
    if (jobActive) DrawJobBar({ a.x, a.b() - 66, a.w, 66 });
}

// ---------------------------------------------------------------- about
void App::DrawAbout(const Rect& a) {
    UI& ui = *m_ui;
    float y = a.y;
    ui.Label({ a.x, y, a.w, 32 }, T(S_ABOUT_TITLE), 24, pal::text, Weight::Bold); y += 52;
    Rect c1{ a.x, y, a.w, 96 };
    ui.Card(c1, 16, pal::card);
    ui.Label({ c1.x + 20, c1.y + 16, 300, 22 }, T(S_LANGUAGE), 14, pal::text, Weight::SemiBold);
    int li = GetLang() == Lang::EN ? 1 : 0;
    int nl = ui.Segmented(MakeId(L"lang"), { c1.x + 20, c1.y + 46, 260, 38 }, { L"العربية", L"English" }, li);
    if (nl != li) ApplyLang(nl == 1 ? Lang::EN : Lang::AR);
    y += 112;

    Rect c2{ a.x, y, a.w, 200 };
    ui.Card(c2, 16, pal::card);
    float x = c2.x + 20, yy = c2.y + 16;
    Rect lr = ui.R({ x, yy, 56, 56 });
    m_gfx.GradientRound(lr, 14, pal::accent, pal::accent2, true);
    m_gfx.DrawIcon(Gfx::Icon::Bolt, lr.Inset(10), pal::white);
    ui.Label({ x + 70, yy, c2.w - 100, 26 }, T(S_APP_NAME) + L"  ·  IsoDeck", 18, pal::text, Weight::Bold);
    ui.Label({ x + 70, yy + 28, c2.w - 100, 20 }, T(S_VERSION) + L" " + ISODECK_VERSION_W + L"   ·   " + T(S_VENTOY_VERSION) + L" " + (Engine::I().Version().empty() ? (m_pkgMissing ? T(S_NEED_PKG) : L"(" + T(S_PREPARING) + L")") : Engine::I().Version()), 12.5f, pal::text3);
    yy += 70;
    ui.Para({ x, yy, c2.w - 40, 24 }, T(S_CREDITS), 13, pal::text2); yy += 26;
    ui.Para({ x, yy, c2.w - 40, 24 }, T(S_LICENSE_NOTE), 12.5f, pal::text3); yy += 36;
    Gfx::Icon fold = Gfx::Icon::Folder, doc = Gfx::Icon::Info;
    if (ui.Button(MakeId(L"opendata"), { x, yy, 190, 36 }, T(S_OPEN_DATA_DIR), BtnStyle::Secondary, &fold)) OpenUrl(AppDataDir());
    if (ui.Button(MakeId(L"openlog2"), { x + 200, yy, 150, 36 }, T(S_OPEN_LOG), BtnStyle::Ghost, &doc)) OpenUrl(JoinPath(AppDataDir(), L"isodeck.log"));
    if (ui.Button(MakeId(L"site"), { x + 360, yy, 150, 36 }, L"flowp.uk", BtnStyle::Ghost)) OpenUrl(L"https://flowp.uk");
}

// ---------------------------------------------------------------- modal
void App::DrawModal() {
    if (!m_modal.open) { m_ui->Anim(MakeId(L"modal"), false, 14.f); return; }
    UI& ui = *m_ui;
    float t = ui.Anim(MakeId(L"modal"), true, 14.f);
    m_gfx.FillRect({ 0, 0, m_gfx.Width(), m_gfx.Height() }, WithA(Hex(0x000000), 0.55f * t));
    float w = 460;
    float textH = m_gfx.MeasureHeight(m_modal.text, TextStyle{ 13.5f }, w - 48) + 8;
    float h = 92 + textH + 70;
    Rect r{ (m_gfx.Width() - w) / 2, (m_gfx.Height() - h) / 2 - 20 * (1 - t), w, h };
    Rect mr = ui.R(r);
    m_gfx.Shadow(mr, 18, 24, WithA(Hex(0x000000), 0.5f * t));
    m_gfx.FillRound(mr, 18, Lerp(pal::card, pal::card2, 0.4f));
    m_gfx.StrokeRound(mr, 18, pal::border);
    Color ic = m_modal.style == BtnStyle::Danger ? pal::red : pal::accent;
    Rect icr = ui.R({ r.x + 24, r.y + 24, 40, 40 });
    m_gfx.FillRound(icr, 12, WithA(ic, 0.15f));
    m_gfx.DrawIcon(m_modal.icon, icr.Inset(9), ic, 2);
    ui.Label({ r.x + 76, r.y + 24, w - 100, 40 }, m_modal.title, 17, pal::text, Weight::Bold);
    ui.Para({ r.x + 24, r.y + 80, w - 48, textH }, m_modal.text, 13.5f, pal::text2);
    float by = r.b() - 56;
    bool ok = ui.Button(MakeId(L"modalok"), { r.r() - 24 - 170, by, 170, 40 }, m_modal.ok, m_modal.style);
    bool cancel = m_modal.withCancel && ui.Button(MakeId(L"modalcancel"), { r.r() - 24 - 170 - 12 - 110, by, 110, 40 }, T(S_CANCEL), BtnStyle::Ghost);
    // clicking the dim area cancels
    if (ok) { m_modal.open = false; if (m_modal.onOk) m_modal.onOk(); }
    else if (cancel) m_modal.open = false;
    else if (m_in.released && m_modal.withCancel && !mr.Contains(m_in.mx, m_in.my) && m_in.my > 0) { /* keep modal; avoid accidental close */ }
    // swallow input under modal
    m_in.pressed = m_in.released = false; m_in.wheel = 0;
}

void App::DrawToast() {
    if (m_toast.until <= m_time) return;
    UI& ui = *m_ui;
    ui.RequestAnim();
    double left = m_toast.until - m_time;
    float t = (float)std::min(1.0, std::min(left / 0.35, (4.5 - left) / 0.25));
    TextStyle ts; ts.size = 13; ts.weight = Weight::Medium; ts.align = Align::Center;
    float w = std::min(m_gfx.Width() - 80, m_gfx.MeasureWidth(m_toast.text, ts) + 64);
    Rect r{ kSideW + (m_gfx.Width() - kSideW - w) / 2, m_gfx.Height() - 64 + 12 * (1 - t), w, 42 };
    Rect mr = ui.R(r);
    Color c = m_toast.ok ? pal::green : pal::red;
    m_gfx.Shadow(mr, 21, 12, WithA(Hex(0x000000), 0.4f * t));
    m_gfx.FillRound(mr, 21, WithA(Lerp(pal::card2, c, 0.25f), t));
    m_gfx.StrokeRound(mr, 21, WithA(c, 0.6f * t));
    Rect ir = m_gfx.RTL() ? Rect{ mr.r() - 34, mr.y + 12, 18, 18 } : Rect{ mr.x + 16, mr.y + 12, 18, 18 };
    m_gfx.DrawIcon(m_toast.ok ? Gfx::Icon::Check : Gfx::Icon::Warning, ir, WithA(c, t), 2);
    Rect tr = m_gfx.RTL() ? Rect{ mr.x + 12, mr.y, mr.w - 52, mr.h } : Rect{ mr.x + 40, mr.y, mr.w - 52, mr.h };
    m_gfx.Text(tr, m_toast.text, ts, WithA(pal::text, t));
}

} // namespace isodeck
