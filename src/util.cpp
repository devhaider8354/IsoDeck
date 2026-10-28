// IsoDeck — common utilities
#include "util.h"
#include "resource.h"
#include <shlobj.h>
#include <shellapi.h>
#include <commdlg.h>
#include <bcrypt.h>
#include <cstdarg>
#include <cstdio>
#include <algorithm>
#include <mutex>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "advapi32.lib")

namespace isodeck {

// ---------------------------------------------------------------- strings
Str Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    Str out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n);
    return out;
}
std::string WideToUtf8(const Str& s) {
    if (s.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], n, nullptr, nullptr);
    return out;
}
Str Format(const wchar_t* fmt, ...) {
    wchar_t buf[2048];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    return buf;
}
Str HumanSize(uint64_t b) {
    const wchar_t* u[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    double v = (double)b; int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    return i == 0 ? Format(L"%llu %s", b, u[i]) : Format(L"%.1f %s", v, u[i]);
}
Str ToLower(Str s) { std::transform(s.begin(), s.end(), s.begin(), ::towlower); return s; }
bool EndsWithNoCase(const Str& s, const Str& suf) {
    if (suf.size() > s.size()) return false;
    return ToLower(s.substr(s.size() - suf.size())) == ToLower(suf);
}
Str Trim(const Str& s) {
    size_t a = s.find_first_not_of(L" \t\r\n"), b = s.find_last_not_of(L" \t\r\n");
    return a == Str::npos ? L"" : s.substr(a, b - a + 1);
}
std::vector<Str> Split(const Str& s, wchar_t sep) {
    std::vector<Str> v; Str cur;
    for (wchar_t c : s) { if (c == sep) { v.push_back(cur); cur.clear(); } else cur += c; }
    v.push_back(cur);
    return v;
}
Str JsonEscape(const Str& s) {
    Str o;
    for (wchar_t c : s) {
        switch (c) {
        case L'"': o += L"\\\""; break;
        case L'\\': o += L"\\\\"; break;
        case L'\n': o += L"\\n"; break;
        case L'\r': break;
        case L'\t': o += L"\\t"; break;
        default: if (c < 0x20) o += Format(L"\\u%04x", c); else o += c;
        }
    }
    return o;
}

// ------------------------------------------------------------- filesystem
Str AppDataDir() {
    static Str cached;
    if (!cached.empty()) return cached;
    wchar_t buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, buf)))
        cached = JoinPath(buf, L"IsoDeck");
    else
        cached = JoinPath(DirName(ExePath()), L"data");
    MakeDirs(cached);
    return cached;
}
Str ExePath() { wchar_t b[MAX_PATH]; GetModuleFileNameW(nullptr, b, MAX_PATH); return b; }
Str JoinPath(const Str& a, const Str& b) {
    if (a.empty()) return b;
    if (a.back() == L'\\' || a.back() == L'/') return a + b;
    return a + L"\\" + b;
}
Str FileName(const Str& p) { size_t i = p.find_last_of(L"\\/"); return i == Str::npos ? p : p.substr(i + 1); }
Str DirName(const Str& p) { size_t i = p.find_last_of(L"\\/"); return i == Str::npos ? L"" : p.substr(0, i); }
bool FileExists(const Str& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }
bool DirExists(const Str& p) { DWORD a = GetFileAttributesW(p.c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
bool MakeDirs(const Str& p) {
    if (p.empty() || DirExists(p)) return true;
    Str parent = DirName(p);
    if (!parent.empty() && parent != p) MakeDirs(parent);
    return CreateDirectoryW(p.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}
bool ReadFileBytes(const Str& p, std::string& out) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
    out.resize((size_t)sz.QuadPart);
    DWORD rd = 0; bool ok = true;
    size_t off = 0;
    while (off < out.size()) {
        DWORD chunk = (DWORD)std::min<size_t>(1 << 20, out.size() - off);
        if (!ReadFile(h, &out[off], chunk, &rd, nullptr) || rd == 0) { ok = false; break; }
        off += rd;
    }
    CloseHandle(h);
    return ok;
}
bool WriteFileBytes(const Str& p, const std::string& data) {
    MakeDirs(DirName(p));
    HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0; bool ok = true; size_t off = 0;
    while (off < data.size()) {
        DWORD chunk = (DWORD)std::min<size_t>(1 << 20, data.size() - off);
        if (!WriteFile(h, data.data() + off, chunk, &wr, nullptr)) { ok = false; break; }
        off += wr;
    }
    CloseHandle(h);
    return ok;
}
uint64_t FileSize(const Str& p) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
bool DeleteFileSafe(const Str& p) {
    SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
    return DeleteFileW(p.c_str()) != 0;
}
static void ListFilesRec(const Str& dir, bool rec, const std::vector<Str>& exts, const Str& skip, std::vector<Str>& out, int depth) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(JoinPath(dir, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        Str name = fd.cFileName;
        if (name == L"." || name == L"..") continue;
        Str full = JoinPath(dir, name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (rec && depth < 6 && ToLower(name) != ToLower(skip) && name != L"System Volume Information" && name != L"$RECYCLE.BIN")
                ListFilesRec(full, rec, exts, skip, out, depth + 1);
        } else {
            for (auto& e : exts) if (EndsWithNoCase(name, e)) { out.push_back(full); break; }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}
std::vector<Str> ListFiles(const Str& dir, bool recursive, const std::vector<Str>& exts, const Str& skipDirNamed) {
    std::vector<Str> out;
    ListFilesRec(dir, recursive, exts, skipDirNamed, out, 0);
    std::sort(out.begin(), out.end(), [](const Str& a, const Str& b) { return ToLower(a) < ToLower(b); });
    return out;
}

// ----------------------------------------------------------------- config
Str Config::Get(const Str& k, const Str& def) const { for (auto& p : kv) if (p.first == k) return p.second; return def; }
int Config::GetInt(const Str& k, int def) const { Str v = Get(k); return v.empty() ? def : _wtoi(v.c_str()); }
void Config::Set(const Str& k, const Str& v) { for (auto& p : kv) if (p.first == k) { p.second = v; return; } kv.push_back({ k, v }); }
void Config::SetInt(const Str& k, int v) { Set(k, Format(L"%d", v)); }
bool Config::Load(const Str& path) {
    std::string raw; if (!ReadFileBytes(path, raw)) return false;
    kv.clear();
    for (auto& line : Split(Utf8ToWide(raw), L'\n')) {
        Str l = Trim(line); if (l.empty() || l[0] == L'#') continue;
        size_t eq = l.find(L'='); if (eq == Str::npos) continue;
        kv.push_back({ Trim(l.substr(0, eq)), Trim(l.substr(eq + 1)) });
    }
    return true;
}
bool Config::Save(const Str& path) const {
    Str out = L"# IsoDeck config\n";
    for (auto& p : kv) out += p.first + L"=" + p.second + L"\n";
    return WriteFileBytes(path, WideToUtf8(out));
}

// ------------------------------------------------------------------- misc
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID sid = nullptr;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) {
        CheckTokenMembership(nullptr, sid, &isAdmin);
        FreeSid(sid);
    }
    return isAdmin == TRUE;
}
bool RelaunchAsAdmin() {
    SHELLEXECUTEINFOW sei{ sizeof(sei) };
    Str exe = ExePath();
    sei.lpVerb = L"runas"; sei.lpFile = exe.c_str(); sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) == TRUE;
}
Str LastErrorText(DWORD err) {
    wchar_t* msg = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, 0, (LPWSTR)&msg, 0, nullptr);
    Str s = msg ? Trim(msg) : Format(L"error %lu", err);
    if (msg) LocalFree(msg);
    return s;
}
bool RunHidden(const Str& cmdLine, const Str& workDir, DWORD* exitCode, DWORD timeoutMs, HANDLE* outProcess) {
    STARTUPINFOW si{ sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    Str cmd = cmdLine; // CreateProcess may modify the buffer
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
        workDir.empty() ? nullptr : workDir.c_str(), &si, &pi)) {
        SLOG(L"CreateProcess failed: %s (%s)", cmdLine.c_str(), LastErrorText().c_str());
        return false;
    }
    CloseHandle(pi.hThread);
    if (outProcess) { *outProcess = pi.hProcess; return true; }
    DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs);
    DWORD code = 0; GetExitCodeProcess(pi.hProcess, &code);
    if (w == WAIT_TIMEOUT) { TerminateProcess(pi.hProcess, 1); code = 1; }
    CloseHandle(pi.hProcess);
    if (exitCode) *exitCode = code;
    return true;
}
Str Sha256File(const Str& path, std::function<bool(uint64_t, uint64_t)> progress) {
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return L"";
    DWORD objLen = 0, cb = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
    std::string obj(objLen, 0);
    if (BCryptCreateHash(alg, &hash, (PUCHAR)&obj[0], objLen, nullptr, 0, 0) != 0) { BCryptCloseAlgorithmProvider(alg, 0); return L""; }
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    Str result;
    if (h != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER sz; GetFileSizeEx(h, &sz);
        std::vector<unsigned char> buf(4 << 20);
        DWORD rd = 0; uint64_t done = 0; bool cancel = false;
        while (ReadFile(h, buf.data(), (DWORD)buf.size(), &rd, nullptr) && rd > 0) {
            BCryptHashData(hash, buf.data(), rd, 0);
            done += rd;
            if (progress && !progress(done, (uint64_t)sz.QuadPart)) { cancel = true; break; }
        }
        CloseHandle(h);
        if (!cancel) {
            unsigned char digest[32];
            BCryptFinishHash(hash, digest, 32, 0);
            for (int i = 0; i < 32; ++i) result += Format(L"%02x", digest[i]);
        }
    }
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    return result;
}
void OpenUrl(const Str& url) { ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }

Str PickFile(HWND owner, const wchar_t* filter, bool multi, std::vector<Str>* many) {
    std::vector<wchar_t> buf(multi ? 65536 : MAX_PATH, 0);
    OPENFILENAMEW ofn{ sizeof(ofn) };
    ofn.hwndOwner = owner; ofn.lpstrFilter = filter; ofn.lpstrFile = buf.data(); ofn.nMaxFile = (DWORD)buf.size();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR | (multi ? OFN_ALLOWMULTISELECT : 0);
    if (!GetOpenFileNameW(&ofn)) return L"";
    if (!multi) return buf.data();
    // multi: dir\0file1\0file2\0\0  (or a single full path)
    const wchar_t* p = buf.data();
    Str dir = p; p += dir.size() + 1;
    if (*p == 0) { if (many) many->push_back(dir); return dir; }
    while (*p) { Str f = p; p += f.size() + 1; if (many) many->push_back(JoinPath(dir, f)); }
    return dir;
}
Str PickImageFile(HWND owner) {
    return PickFile(owner, L"Images (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All files\0*.*\0\0", false, nullptr);
}
bool LoadResourceBytes(int id, const wchar_t* type, std::string& out) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), type);
    if (!r) return false;
    HGLOBAL g = LoadResource(nullptr, r); if (!g) return false;
    DWORD sz = SizeofResource(nullptr, r);
    const char* p = (const char*)LockResource(g);
    if (!p) return false;
    out.assign(p, sz);
    return true;
}
Str NowStamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    return Format(L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}
void LogLine(const Str& line) {
    static std::mutex m;
    std::lock_guard<std::mutex> lk(m);
    static Str path = JoinPath(AppDataDir(), L"isodeck.log");
    if (FileSize(path) > (2u << 20)) DeleteFileSafe(path);
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    std::string s = WideToUtf8(L"[" + NowStamp() + L"] " + line + L"\r\n");
    DWORD wr; WriteFile(h, s.data(), (DWORD)s.size(), &wr, nullptr);
    CloseHandle(h);
}

} // namespace isodeck
