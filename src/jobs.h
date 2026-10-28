// IsoDeck — background jobs: file copy, download, hashing (single worker queue)
#pragma once
#include "util.h"
#include <atomic>
#include <mutex>
#include <deque>
#include <thread>

namespace isodeck {

enum class JobKind { Copy, Download, Hash };

struct Job {
    JobKind kind;
    Str     source;     // file path or URL
    Str     dest;       // destination file path (Copy/Download); for Hash: unused
    Str     label;      // shown in UI
};

struct JobProgress {
    bool     active = false;
    JobKind  kind = JobKind::Copy;
    Str      label;
    uint64_t done = 0, total = 0;
    double   speedBps = 0;
    Str      lastResult;     // message of the last finished job (localized by caller)
    bool     lastOk = true;
    Str      hashResult;     // for Hash jobs
    Str      hashFile;
    int      queued = 0;
};

class Jobs {
public:
    static Jobs& I();
    void Enqueue(const Job& j);
    void Cancel();                      // cancels the current job and clears the queue
    JobProgress Snapshot();
    bool Busy();
    bool TakeFinishedFlag();            // returns true once after a job finishes (UI refresh)

protected:
    Jobs();
    void Loop();
    void RunCopy(const Job& j);
    void RunDownload(const Job& j);
    void RunHash(const Job& j);
    void Report(uint64_t done, uint64_t total);

    std::mutex m_mx;
    std::deque<Job> m_queue;
    JobProgress m_prog;
    std::atomic<bool> m_cancel{ false }, m_finished{ false }, m_busy{ false };
    std::thread m_thread;
    uint64_t m_lastBytes = 0; DWORD m_lastTick = 0;
};

// Simple JSON writer for our own catalog / config (no parser needed; catalog is line-based)
struct CatalogItem { Str name; Str category; Str url; Str note; bool direct = true; };
std::vector<CatalogItem> LoadCatalog();      // user override %LOCALAPPDATA%\IsoDeck\catalog.txt else embedded
Str CatalogUserPath();

} // namespace isodeck
