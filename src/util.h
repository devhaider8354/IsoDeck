// IsoDeck — common utilities
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace isodeck {

using Str = std::wstring;

// ---- strings ------------------------------------------------------------
Str  Utf8ToWide(const std::string& s);
std::string WideToUtf8(const Str& s);
Str  Format(const wchar_t* fmt, ...);
Str  HumanSize(uint64_t bytes);
Str  ToLower(Str s);
bool EndsWithNoCase(const Str& s, const Str& suffix);
Str  Trim(const Str& s);
std::vector<Str> Split(const Str& s, wchar_t sep);
Str  JsonEscape(const Str& s);          // escapes for JSON string literal (UTF-16 -> \uXXXX for non-ASCII kept raw as UTF-8)

// ---- filesystem ----------------------------------------------------------
Str  AppDataDir();                       // %LOCALAPPDATA%\IsoDeck   (created)
Str  ExePath();
Str  JoinPath(const Str& a, const Str& b);
Str  FileName(const Str& path);
Str  DirName(const Str& path);
bool FileExists(const Str& p);
bool DirExists(const Str& p);
bool MakeDirs(const Str& p);
bool ReadFileBytes(const Str& p, std::string& out);
bool WriteFileBytes(const Str& p, const std::string& data);
uint64_t FileSize(const Str& p);
bool DeleteFileSafe(const Str& p);
std::vector<Str> ListFiles(const Str& dir, bool recursive, const std::vector<Str>& exts, const Str& skipDirNamed);

// ---- config (tiny ini-like key=value in UTF-8) ---------------------------
struct Config {
    std::vector<std::pair<Str, Str>> kv;
    Str  Get(const Str& k, const Str& def = L"") const;
    int  GetInt(const Str& k, int def) const;
    void Set(const Str& k, const Str& v);
    void SetInt(const Str& k, int v);
    bool Load(const Str& path);
    bool Save(const Str& path) const;
};

// ---- misc -----------------------------------------------------------------
bool  IsRunningAsAdmin();
bool  RelaunchAsAdmin();
Str   LastErrorText(DWORD err = GetLastError());
bool  RunHidden(const Str& cmdLine, const Str& workDir, DWORD* exitCode, DWORD timeoutMs = INFINITE, HANDLE* outProcess = nullptr);
Str   Sha256File(const Str& path, std::function<bool(uint64_t done, uint64_t total)> progress = nullptr);
void  OpenUrl(const Str& url);
Str   PickFile(HWND owner, const wchar_t* filter, bool multi, std::vector<Str>* many);
Str   PickImageFile(HWND owner);
bool  LoadResourceBytes(int id, const wchar_t* type, std::string& out);
Str   NowStamp();

// ---- logging ---------------------------------------------------------------
void  LogLine(const Str& line);          // appends to %LOCALAPPDATA%\IsoDeck\isodeck.log
#define SLOG(...) ::isodeck::LogLine(::isodeck::Format(__VA_ARGS__))

} // namespace isodeck
