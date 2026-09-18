// Exercises the broker data sources against the real machine, outside any
// injected process, so the wire formats and the ordering can be checked
// before any of this is wired into the shell.
#include <windows.h>

#include <cstdio>

#include "apps_index.h"
#include "everything_ipc.h"
#include "file_ranker.h"

static double Ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart;
}

static void PrintAge(const FILETIME& ft) {
    SYSTEMTIME st{};
    if (!FileTimeToSystemTime(&ft, &st)) {
        wprintf(L"     -      ");
        return;
    }
    wprintf(L"%04d-%02d-%02d  ", st.wYear, st.wMonth, st.wDay);
}

int wmain(int argc, wchar_t** argv) {
    const wchar_t* query = (argc > 1) ? argv[1] : L"read";
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);

    wprintf(L"=== apps column ===\n");
    apps::Index index;
    QueryPerformanceCounter(&t0);
    bool built = index.Rebuild();
    QueryPerformanceCounter(&t1);
    wprintf(L"  rebuild: %ls, %zu apps, %.0f ms\n", built ? L"ok" : L"FAILED",
            index.Count(), Ms(t0, t1, f));

    QueryPerformanceCounter(&t0);
    auto hits = index.Search(query, 6);
    QueryPerformanceCounter(&t1);
    wprintf(L"  search '%ls': %zu hits in %.3f ms\n", query, hits.size(),
            Ms(t0, t1, f));
    for (const auto& h : hits) {
        wprintf(L"    [%d] %ls\n", h.score, h.app->name.c_str());
    }
    if (!hits.empty()) {
        QueryPerformanceCounter(&t0);
        HBITMAP bmp = apps::Index::LoadIcon(*hits[0].app, 32);
        QueryPerformanceCounter(&t1);
        wprintf(L"  icon for '%ls': %ls (%.1f ms)\n", hits[0].app->name.c_str(),
                bmp ? L"ok" : L"FAILED", Ms(t0, t1, f));
        if (bmp) DeleteObject(bmp);
    }

    wprintf(L"\n=== files column ===\n");
    HWND ipc = everything::FindIpcWindow();
    wprintf(L"  ipc window: %p\n", ipc);
    if (!ipc) {
        wprintf(L"  Everything is not running - cannot test\n");
        CoUninitialize();
        return 0;
    }
    everything::Client client;
    if (!client.Init()) {
        wprintf(L"  client init FAILED\n");
        CoUninitialize();
        return 1;
    }

    std::vector<everything::Result> pool;
    DWORD total = 0;
    QueryPerformanceCounter(&t0);
    bool ok = client.Query(query, ranker::kDefaultPool, &pool, &total);
    QueryPerformanceCounter(&t1);
    double queryMs = Ms(t0, t1, f);
    wprintf(L"  query '%ls': %ls via %ls, %u total, %zu in pool, %.1f ms\n",
            query, ok ? L"ok" : L"FAILED",
            client.usedQuery2() ? L"QUERY2" : L"QUERYW (legacy)", total,
            pool.size(), queryMs);
    if (!ok) {
        CoUninitialize();
        return 1;
    }

    wprintf(L"\n  -- unranked (what Everything returned, name ascending) --\n");
    for (size_t i = 0; i < pool.size() && i < 6; i++) {
        wprintf(L"    %-36ls  %ls\n", pool[i].name.c_str(),
                pool[i].path.c_str());
    }

    QueryPerformanceCounter(&t0);
    ranker::Rank(&pool, query, 8);
    QueryPerformanceCounter(&t1);
    wprintf(L"\n  -- ranked (%.2f ms) --\n", Ms(t0, t1, f));
    wprintf(L"    %-36ls %-12ls %5ls  %ls\n", L"name", L"modified", L"runs",
            L"path");
    for (const auto& r : pool) {
        wprintf(L"    %-36ls ", r.name.c_str());
        PrintAge(r.dateModified);
        wprintf(L"%5lu  %ls%ls\n", r.runCount, r.path.c_str(),
                r.isFolder ? L"  [folder]" : L"");
    }

    CoUninitialize();
    return 0;
}
