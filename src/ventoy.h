// IsoDeck — embedded boot-engine package (Ventoy) : extraction + install/update
#pragma once
#include "util.h"
#include <atomic>
#include <thread>
#include <mutex>

namespace isodeck {

struct InstallOptions {
    int  physDrive = -1;
    bool update = false;        // /U instead of /I
    bool gpt = true;
    bool secureBoot = true;
    Str  fs = L"exFAT";         // exFAT | NTFS | FAT32
    int  reserveMB = 0;
    bool noUsbCheck = false;
};

struct EngineStatus {
    std::atomic<bool> running{ false };
    std::atomic<int>  percent{ 0 };
    std::atomic<int>  result{ -1 };    // -1 unknown, 0 ok, 1 fail
    Str  message;                       // last line of the CLI log (guarded by mutex in impl)
};

class Engine {
public:
    static Engine& I();

    bool   PackageEmbedded() const;                 // resource present?
    bool   EnsureExtracted(std::function<void(const Str&)> status = nullptr); // extracts zip to AppData if needed
    Str    Ventoy2DiskPath() const { return m_exe; }
    Str    Version() const { return m_version; }    // e.g. "1.1.05"
    Str    EngineDir() const { return DirName(m_exe); }

    // Asynchronous. Poll status; completion when running==false.
    bool   Start(const InstallOptions& opt);
    EngineStatus& Status() { return m_status; }
    Str    LogPath() const { return JoinPath(EngineDir(), L"cli_log.txt"); }
    Str    Message();

private:
    Engine() = default;
    ~Engine() { if (m_thread.joinable()) m_thread.join(); }
    void   Worker(InstallOptions opt);
    bool   Unzip(const Str& zip, const Str& dest);
    bool   Locate(const Str& root);

    Str m_exe, m_version;
    std::mutex m_extractMx;
    EngineStatus m_status;
    std::thread m_thread;
};

} // namespace isodeck
