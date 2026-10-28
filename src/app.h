// IsoDeck — application window & pages
#pragma once
#include "ui.h"
#include "disk.h"
#include "ventoy.h"
#include "jobs.h"
#include "theme.h"
#include "i18n.h"

namespace isodeck {

enum Page { PG_DEVICES = 0, PG_ISOS, PG_THEME, PG_CATALOG, PG_ABOUT, PG_COUNT };

class App {
public:
    bool Create(HINSTANCE inst);
    int  Run();

private:
    static LRESULT CALLBACK WndProcS(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);
    void Render();
    void Frame();

    // layout pieces
    void DrawTitleBar();
    void DrawSidebar();
    void DrawDevices(const Rect& area);
    void DrawIsos(const Rect& area);
    void DrawTheme(const Rect& area);
    void DrawCatalog(const Rect& area);
    void DrawAbout(const Rect& area);
    void DrawModal();
    void DrawJobBar(const Rect& area);
    void DrawToast();

    // state helpers
    void RefreshDisks();
    const DiskInfo* Selected() const;
    void RefreshIsos();
    void LoadConfig();
    void SaveConfig();
    void StartInstall(bool update);
    void RunPending();
    void AddIsoFiles(const std::vector<Str>& files);
    void Toast(const Str& msg, bool ok = true);
    void ShowModal(const Str& title, const Str& text, const Str& ok, BtnStyle style, std::function<void()> onOk, bool withCancel = true, Gfx::Icon icon = Gfx::Icon::Info);
    void SetPage(int p);
    void ApplyLang(Lang l);
    Str  DataDriveOrEmpty() const;
    void EnsureAliasEntries();
    void SetTimer(bool fast);

    HWND m_hwnd = nullptr;
    HINSTANCE m_inst = nullptr;
    Gfx m_gfx;
    Input m_in;
    UI* m_ui = nullptr;
    LARGE_INTEGER m_freq{}, m_last{};
    bool m_fastTimer = false;

    Config m_cfg;
    ThemeSettings m_theme;
    int m_page = PG_DEVICES;
    float m_pageAnim = 1.f;

    // devices
    std::vector<DiskInfo> m_disks;
    int m_selDisk = -1;           // physical drive number
    bool m_showAll = false;
    InstallOptions m_opt;
    Str m_reserveText = L"0";
    bool m_engineBusy = false;
    int  m_lastResult = -1;
    Str  m_engineMsg;
    bool m_extracting = false;
    bool m_pkgMissing = false;
    bool m_pending = false;
    bool m_inRender = false;
    std::function<void()> m_deferred;        // dialogs etc. must run outside Render()
    bool m_autoTheme = false; int m_autoThemeTries = 0;
    InstallOptions m_pendingOpt;

    // isos
    std::vector<Str> m_isos;
    Str m_isoDrive;
    DWORD m_isoRefreshTick = 0;

    // theme
    Str m_timeoutText = L"30";
    std::vector<Str> m_accentPresets = { L"#00A3FF", L"#22C55E", L"#F59E0B", L"#EF4444", L"#A855F7", L"#EC4899", L"#14B8A6", L"#FFFFFF" };

    // catalog
    std::vector<CatalogItem> m_catalog;
    Str m_customUrl;

    // modal / toast
    struct Modal { bool open = false; Str title, text, ok; BtnStyle style = BtnStyle::Primary; std::function<void()> onOk; bool withCancel = true; Gfx::Icon icon = Gfx::Icon::Info; } m_modal;
    struct ToastS { Str text; bool ok = true; double until = 0; } m_toast;
    double m_time = 0;
};

} // namespace isodeck
