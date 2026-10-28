// IsoDeck — embedded boot-engine package (Ventoy) : extraction + install/update
#include "ventoy.h"
#include "resource.h"
#include <mutex>

namespace isodeck {

static std::mutex g_msgMutex;

Engine& Engine::I() { static Engine e; return e; }

bool Engine::PackageEmbedded() const {
    return FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_VENTOY_ZIP), L"PACKAGE") != nullptr;
}

Str Engine::Message() { std::lock_guard<std::mutex> lk(g_msgMutex); return m_status.message; }

static void SetMsg(EngineStatus& st, const Str& m) { std::lock_guard<std::mutex> lk(g_msgMutex); st.message = m; }

bool Engine::Unzip(const Str& zip, const Str& dest) {
    MakeDirs(dest);
    // 1) bsdtar ships with Windows 10 1803+ and understands .zip
    wchar_t sys[MAX_PATH]; GetSystemDirectoryW(sys, MAX_PATH);
    Str tar = JoinPath(sys, L"tar.exe");
    DWORD code = 1;
    if (FileExists(tar)) {
        Str cmd = L"\"" + tar + L"\" -xf \"" + zip + L"\" -C \"" + dest + L"\"";
        if (RunHidden(cmd, dest, &code, 120000) && code == 0) return true;
        SLOG(L"tar.exe failed with code %lu, trying PowerShell", code);
    }
    // 2) PowerShell fallback
    Str ps = JoinPath(sys, L"WindowsPowerShell\\v1.0\\powershell.exe");
    Str cmd = L"\"" + ps + L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '" + zip + L"' -DestinationPath '" + dest + L"' -Force\"";
    return RunHidden(cmd, dest, &code, 180000) && code == 0;
}

bool Engine::Locate(const Str& root) {
    // Ventoy2Disk.exe may be at root or inside a "ventoy-x.y.z" folder.
    std::vector<Str> found = ListFiles(root, true, { L"Ventoy2Disk.exe" }, L"");
    for (auto& f : found) {
        if (ToLower(FileName(f)) == L"ventoy2disk.exe" && DirExists(JoinPath(DirName(f), L"ventoy"))) {
            m_exe = f;
            std::string raw;
            if (ReadFileBytes(JoinPath(DirName(f), L"ventoy\\version"), raw)) m_version = Trim(Utf8ToWide(raw));
            if (m_version.empty()) { Str dir = FileName(DirName(f)); size_t dash = dir.find(L'-'); m_version = dash != Str::npos ? dir.substr(dash + 1) : L""; }
            return true;
        }
    }
    return false;
}

bool Engine::EnsureExtracted(std::function<void(const Str&)> status) {
    std::lock_guard<std::mutex> lk(m_extractMx);
    if (!m_exe.empty() && FileExists(m_exe)) return true;
    if (!PackageEmbedded()) { SLOG(L"No embedded package resource"); return false; }

    Str pkgDir = JoinPath(AppDataDir(), L"engine");
    Str stamp = JoinPath(pkgDir, L"stamp.txt");
    std::string res;
    if (!LoadResourceBytes(IDR_VENTOY_ZIP, L"PACKAGE", res)) return false;

    // Stamp = resource size + exe mtime; re-extract only when the embedded package changed.
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    GetFileAttributesExW(ExePath().c_str(), GetFileExInfoStandard, &fad);
    Str want = Format(L"%zu|%lu|%lu", res.size(), fad.ftLastWriteTime.dwHighDateTime, fad.ftLastWriteTime.dwLowDateTime);
    std::string have;
    if (ReadFileBytes(stamp, have) && Utf8ToWide(have) == want && Locate(pkgDir)) return true;

    if (status) status(L"extract");
    Str zip = JoinPath(pkgDir, L"package.zip");
    MakeDirs(pkgDir);
    if (!WriteFileBytes(zip, res)) { SLOG(L"Cannot write package zip"); return false; }
    if (!Unzip(zip, pkgDir)) { SLOG(L"Unzip failed"); return false; }
    DeleteFileSafe(zip);
    // neutral folder name for the extracted engine
    {
        WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(JoinPath(pkgDir, L"ventoy-*").c_str(), &fd);
        if (fh != INVALID_HANDLE_VALUE) {
            do { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) MoveFileExW(JoinPath(pkgDir, fd.cFileName).c_str(), JoinPath(pkgDir, L"core").c_str(), MOVEFILE_REPLACE_EXISTING); } while (FindNextFileW(fh, &fd));
            FindClose(fh);
        }
    }
    if (!Locate(pkgDir)) { SLOG(L"Ventoy2Disk.exe not found after extraction"); return false; }
    WriteFileBytes(stamp, WideToUtf8(want));
    SLOG(L"Engine ready: %s (v%s)", m_exe.c_str(), m_version.c_str());
    return true;
}

bool Engine::Start(const InstallOptions& opt) {
    if (m_status.running) return false;
    if (!EnsureExtracted()) return false;
    if (m_thread.joinable()) m_thread.join();
    m_status.running = true; m_status.percent = 0; m_status.result = -1;
    SetMsg(m_status, L"");
    m_thread = std::thread(&Engine::Worker, this, opt);
    return true;
}

void Engine::Worker(InstallOptions opt) {
    Str dir = EngineDir();
    Str fLog = JoinPath(dir, L"cli_log.txt"), fPct = JoinPath(dir, L"cli_percent.txt"), fDone = JoinPath(dir, L"cli_done.txt");
    DeleteFileSafe(fLog); DeleteFileSafe(fPct); DeleteFileSafe(fDone);

    // Ventoy2Disk.exe VTOYCLI /I|/U /PhyDrive:N [/GPT] [/NOSB] [/FS:xxx] [/R:MB] [/NOUSBCheck]
    Str cmd = L"\"" + m_exe + L"\" VTOYCLI " + (opt.update ? L"/U" : L"/I") + Format(L" /PhyDrive:%d", opt.physDrive);
    if (!opt.update) {
        if (opt.gpt) cmd += L" /GPT";
        if (!opt.secureBoot) cmd += L" /NOSB";
        if (!opt.fs.empty()) cmd += L" /FS:" + ToLower(opt.fs);
        if (opt.reserveMB > 0) cmd += Format(L" /R:%d", opt.reserveMB);
    }
    if (opt.noUsbCheck) cmd += L" /NOUSBCheck";
    SLOG(L"Run: %s", cmd.c_str());

    HANDLE proc = nullptr;
    if (!RunHidden(cmd, dir, nullptr, INFINITE, &proc)) {
        SetMsg(m_status, L"CreateProcess failed: " + LastErrorText());
        m_status.result = 1; m_status.running = false; return;
    }
    DWORD start = GetTickCount();
    for (;;) {
        DWORD w = WaitForSingleObject(proc, 300);
        std::string s;
        if (ReadFileBytes(fPct, s)) { int p = atoi(s.c_str()); if (p >= 0 && p <= 100) m_status.percent = p; }
        if (ReadFileBytes(fLog, s)) {
            Str all = Utf8ToWide(s);
            size_t e = all.find_last_not_of(L"\r\n");
            if (e != Str::npos) { size_t b = all.find_last_of(L"\r\n", e); SetMsg(m_status, Trim(all.substr(b == Str::npos ? 0 : b + 1, e - (b == Str::npos ? 0 : b + 1) + 1))); }
        }
        if (w == WAIT_OBJECT_0) break;
        if (GetTickCount() - start > 30u * 60u * 1000u) { TerminateProcess(proc, 1); SetMsg(m_status, L"Timeout"); break; }
    }
    DWORD code = 1; GetExitCodeProcess(proc, &code); CloseHandle(proc);
    std::string done;
    int result = 1;
    if (ReadFileBytes(fDone, done)) result = (Trim(Utf8ToWide(done)) == L"0") ? 0 : 1;
    else result = (code == 0) ? 0 : 1;
    if (result == 0) m_status.percent = 100;
    SLOG(L"Engine finished: result=%d exit=%lu", result, code);
    m_status.result = result;
    m_status.running = false;
}

} // namespace isodeck
