// IsoDeck — entry point
#include "app.h"
#include "resource.h"
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    // Requires administrator (manifest asks for it); if somehow started without, relaunch elevated.
    if (!isodeck::IsRunningAsAdmin()) {
        if (isodeck::RelaunchAsAdmin()) return 0;
    }
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"IsoDeck.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND prev = FindWindowW(L"IsoDeckWindow", nullptr);
        if (prev) { ShowWindow(prev, SW_RESTORE); SetForegroundWindow(prev); }
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SLOG(L"---- IsoDeck %s start ----", ISODECK_VERSION_W);
    int rc = 1;
    {
        isodeck::App app;
        if (app.Create(inst)) rc = app.Run();
        else MessageBoxW(nullptr, L"Failed to initialize Direct2D.", L"IsoDeck", MB_ICONERROR);
    }
    CoUninitialize();
    ReleaseMutex(mutex); CloseHandle(mutex);
    return rc;
}
