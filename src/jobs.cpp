// IsoDeck — background jobs
#include "jobs.h"
#include "resource.h"
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace isodeck {

Jobs& Jobs::I() { static Jobs j; return j; }

Jobs::Jobs() { m_thread = std::thread(&Jobs::Loop, this); m_thread.detach(); }

void Jobs::Enqueue(const Job& j) { std::lock_guard<std::mutex> lk(m_mx); m_queue.push_back(j); m_prog.queued = (int)m_queue.size(); }
void Jobs::Cancel() { { std::lock_guard<std::mutex> lk(m_mx); m_queue.clear(); m_prog.queued = 0; } m_cancel = true; }
JobProgress Jobs::Snapshot() { std::lock_guard<std::mutex> lk(m_mx); return m_prog; }
bool Jobs::Busy() { return m_busy; }
bool Jobs::TakeFinishedFlag() { return m_finished.exchange(false); }

void Jobs::Report(uint64_t done, uint64_t total) {
    DWORD now = GetTickCount();
    std::lock_guard<std::mutex> lk(m_mx);
    m_prog.done = done; m_prog.total = total;
    if (now - m_lastTick >= 500) {
        if (m_lastTick) m_prog.speedBps = (double)(done - m_lastBytes) * 1000.0 / (double)(now - m_lastTick);
        m_lastTick = now; m_lastBytes = done;
    }
}

void Jobs::Loop() {
    for (;;) {
        Job j;
        {
            std::lock_guard<std::mutex> lk(m_mx);
            if (m_queue.empty()) { m_busy = false; }
            else {
                j = m_queue.front(); m_queue.pop_front();
                m_prog.queued = (int)m_queue.size();
                m_prog.active = true; m_prog.kind = j.kind; m_prog.label = j.label;
                m_prog.done = m_prog.total = 0; m_prog.speedBps = 0;
                m_lastBytes = 0; m_lastTick = 0;
                m_busy = true;
            }
        }
        if (!m_busy) { Sleep(150); continue; }
        m_cancel = false;
        switch (j.kind) {
        case JobKind::Copy:     RunCopy(j); break;
        case JobKind::Download: RunDownload(j); break;
        case JobKind::Hash:     RunHash(j); break;
        }
        { std::lock_guard<std::mutex> lk(m_mx); m_prog.active = false; }
        m_finished = true;
    }
}

static DWORD CALLBACK CopyCb(LARGE_INTEGER total, LARGE_INTEGER transferred, LARGE_INTEGER, LARGE_INTEGER, DWORD, DWORD, HANDLE, HANDLE, LPVOID p) {
    auto* self = (Jobs*)p;
    // Report through a public-ish path: we use a static trampoline via Snapshot-free method.
    struct Access : Jobs { void R(uint64_t d, uint64_t t) { Report(d, t); } bool C() { return m_cancel; } };
    ((Access*)self)->R((uint64_t)transferred.QuadPart, (uint64_t)total.QuadPart);
    return ((Access*)self)->C() ? PROGRESS_CANCEL : PROGRESS_CONTINUE;
}

void Jobs::RunCopy(const Job& j) {
    BOOL cancel = FALSE;
    bool ok = CopyFileExW(j.source.c_str(), j.dest.c_str(), CopyCb, this, &cancel, COPY_FILE_FAIL_IF_EXISTS) != 0;
    Str err = ok ? L"" : LastErrorText();
    if (!ok && m_cancel) DeleteFileSafe(j.dest);
    std::lock_guard<std::mutex> lk(m_mx);
    m_prog.lastOk = ok; m_prog.lastResult = ok ? FileName(j.dest) : FileName(j.dest) + L" — " + err;
    SLOG(L"Copy %s -> %s : %s", j.source.c_str(), j.dest.c_str(), ok ? L"ok" : err.c_str());
}

void Jobs::RunDownload(const Job& j) {
    bool ok = false; Str err;
    URL_COMPONENTS uc{ sizeof(uc) };
    wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    HINTERNET ses = nullptr, con = nullptr, req = nullptr;
    HANDLE out = INVALID_HANDLE_VALUE;
    Str url = j.source;
    for (int redirect = 0; redirect < 8 && !ok; ++redirect) {
        uc = URL_COMPONENTS{ sizeof(uc) };
        uc.lpszHostName = host; uc.dwHostNameLength = 256; uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
        if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) { err = L"bad url"; break; }
        ses = WinHttpOpen(L"IsoDeck/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!ses) { err = LastErrorText(); break; }
        WinHttpSetTimeouts(ses, 20000, 20000, 30000, 60000);
        con = WinHttpConnect(ses, host, uc.nPort, 0);
        if (!con) { err = LastErrorText(); break; }
        req = WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
        if (!req) { err = LastErrorText(); break; }
        DWORD noRedir = WINHTTP_DISABLE_REDIRECTS;
        WinHttpSetOption(req, WINHTTP_OPTION_DISABLE_FEATURE, &noRedir, sizeof(noRedir));
        if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(req, nullptr)) { err = LastErrorText(); break; }
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status >= 300 && status < 400) {
            wchar_t loc[2048]; DWORD ls = sizeof(loc);
            if (WinHttpQueryHeaders(req, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, loc, &ls, WINHTTP_NO_HEADER_INDEX)) {
                Str l = loc;
                if (l.rfind(L"http", 0) != 0) l = Format(L"%s://%s%s", uc.nScheme == INTERNET_SCHEME_HTTPS ? L"https" : L"http", host, l.c_str());
                url = l;
                WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses); req = con = ses = nullptr;
                continue;
            }
        }
        if (status != 200) { err = Format(L"HTTP %lu", status); break; }
        uint64_t total = 0;
        wchar_t clen[64]; DWORD cls = sizeof(clen);
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, clen, &cls, WINHTTP_NO_HEADER_INDEX)) total = _wtoi64(clen);
        Str tmp = j.dest + L".part";
        out = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (out == INVALID_HANDLE_VALUE) { err = LastErrorText(); break; }
        std::vector<char> buf(1 << 20);
        uint64_t done = 0; DWORD rd = 0; bool fail = false;
        for (;;) {
            if (m_cancel) { fail = true; err = L"cancelled"; break; }
            if (!WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &rd)) { fail = true; err = LastErrorText(); break; }
            if (rd == 0) break;
            DWORD wr = 0;
            if (!WriteFile(out, buf.data(), rd, &wr, nullptr) || wr != rd) { fail = true; err = LastErrorText(); break; }
            done += rd; Report(done, total);
        }
        CloseHandle(out); out = INVALID_HANDLE_VALUE;
        if (fail || (total && done != total)) { DeleteFileSafe(tmp); if (err.empty()) err = L"incomplete"; break; }
        DeleteFileSafe(j.dest);
        ok = MoveFileW(tmp.c_str(), j.dest.c_str()) != 0;
        if (!ok) err = LastErrorText();
    }
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    if (req) WinHttpCloseHandle(req); if (con) WinHttpCloseHandle(con); if (ses) WinHttpCloseHandle(ses);
    std::lock_guard<std::mutex> lk(m_mx);
    m_prog.lastOk = ok; m_prog.lastResult = ok ? FileName(j.dest) : FileName(j.dest) + L" — " + err;
    SLOG(L"Download %s : %s", j.source.c_str(), ok ? L"ok" : err.c_str());
}

void Jobs::RunHash(const Job& j) {
    Str h = Sha256File(j.source, [this](uint64_t d, uint64_t t) { Report(d, t); return !m_cancel.load(); });
    std::lock_guard<std::mutex> lk(m_mx);
    m_prog.lastOk = !h.empty(); m_prog.hashResult = h; m_prog.hashFile = FileName(j.source);
    m_prog.lastResult = FileName(j.source);
}

// ------------------------------------------------------------------ catalog
Str CatalogUserPath() { return JoinPath(AppDataDir(), L"catalog.txt"); }

std::vector<CatalogItem> LoadCatalog() {
    std::string raw;
    if (!ReadFileBytes(CatalogUserPath(), raw)) {
        if (LoadResourceBytes(IDR_CATALOG, L"PACKAGE", raw)) WriteFileBytes(CatalogUserPath(), raw); // seed editable copy
    }
    std::vector<CatalogItem> items;
    for (auto& line : Split(Utf8ToWide(raw), L'\n')) {
        Str l = Trim(line); if (l.empty() || l[0] == L'#') continue;
        auto parts = Split(l, L'|');
        if (parts.size() < 3) continue;
        CatalogItem it; it.category = Trim(parts[0]); it.name = Trim(parts[1]); it.url = Trim(parts[2]);
        if (parts.size() > 3) it.note = Trim(parts[3]);
        it.direct = EndsWithNoCase(it.url, L".iso") || EndsWithNoCase(it.url, L".img") || EndsWithNoCase(it.url, L".wim") || it.url.find(L"/download") != Str::npos;
        if (parts.size() > 4) it.direct = Trim(parts[4]) == L"direct";
        items.push_back(it);
    }
    return items;
}

} // namespace isodeck
