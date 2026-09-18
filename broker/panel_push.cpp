// Pushes one batch of real results into the live panel, then watches for a
// click coming back.
//
// This is the broker's transport half, exercised by hand: it takes the query
// on the command line instead of reading it out of the search box. That is
// the only piece missing between this and the real broker, and keeping it
// manual means the panel can be tested without also having to be driven.
//
// Usage: panel_push.exe <query>
#include <windows.h>

#include <cstdio>

#include "apps_index.h"
#include "everything_ipc.h"
#include "file_ranker.h"
#include "icon_util.h"
#include "panel_protocol.h"

namespace {

constexpr int kIconSize = 32;
constexpr size_t kMaxApps = 6;
constexpr size_t kMaxFiles = 12;

double Ms(LARGE_INTEGER a, LARGE_INTEGER b, LARGE_INTEGER f) {
    return (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart;
}

void AppendDword(std::vector<BYTE>* buf, DWORD value) {
    const BYTE* p = reinterpret_cast<const BYTE*>(&value);
    buf->insert(buf->end(), p, p + sizeof(value));
}

void AppendString(std::vector<BYTE>* buf, const std::wstring& s) {
    const BYTE* p = reinterpret_cast<const BYTE*>(s.c_str());
    buf->insert(buf->end(), p, p + (s.size() + 1) * sizeof(wchar_t));
}

void AppendRow(std::vector<BYTE>* buf, DWORD kind, DWORD flags,
               const std::wstring& title, const std::wstring& subtitle,
               const std::vector<BYTE>* icon) {
    DWORD iconSize = icon ? kIconSize : 0;
    DWORD iconBytes = icon ? static_cast<DWORD>(icon->size()) : 0;
    AppendDword(buf, kind);
    AppendDword(buf, flags);
    AppendDword(buf, static_cast<DWORD>(title.size()));
    AppendDword(buf, static_cast<DWORD>(subtitle.size()));
    AppendDword(buf, iconSize);
    AppendDword(buf, iconBytes);
    AppendString(buf, title);
    AppendString(buf, subtitle);
    if (icon) {
        buf->insert(buf->end(), icon->begin(), icon->end());
    }
}

// Never a bare SendMessage. The panel lives in another process, and a window
// left behind by a previous mod build has a window procedure pointing into an
// unloaded image: sending to one of those blocked for 3.9 s and then took
// SearchHost down with it. A timeout turns that into a failed call.
LRESULT SendToPanel(HWND panel, DWORD message, const void* data, DWORD bytes) {
    COPYDATASTRUCT cds{};
    cds.dwData = message;
    cds.cbData = bytes;
    cds.lpData = const_cast<void*>(data);
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(panel, WM_COPYDATA, 0,
                             reinterpret_cast<LPARAM>(&cds),
                             SMTO_ABORTIFHUNG, 1000, &result)) {
        return 0;
    }
    return static_cast<LRESULT>(result);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        wprintf(L"usage: panel_push.exe <query>\n");
        return 2;
    }
    const std::wstring query = argv[1];

    HWND panel = protocol::FindPanel();
    if (!panel) {
        wprintf(L"panel window not found.\n");
        wprintf(L"  - is the everything-search mod enabled?\n");
        wprintf(L"  - has the search page been opened at least once?\n");
        return 1;
    }
    DWORD panelPid = 0;
    GetWindowThreadProcessId(panel, &panelPid);
    wprintf(L"panel: hwnd=%p pid=%lu\n", panel, panelPid);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);

    // --- apps -------------------------------------------------------------
    apps::Index index;
    QueryPerformanceCounter(&t0);
    index.Rebuild();
    auto appHits = index.Search(query, kMaxApps);
    QueryPerformanceCounter(&t1);
    wprintf(L"apps: %zu hits (%.0f ms incl. rebuild)\n", appHits.size(),
            Ms(t0, t1, f));

    // --- files ------------------------------------------------------------
    std::vector<everything::Result> files;
    DWORD fileTotal = 0;
    std::wstring status;
    everything::Client client;
    if (!everything::FindIpcWindow()) {
        status = L"Everything is not running";
    } else if (!client.Init()) {
        status = L"Could not talk to Everything";
    } else {
        QueryPerformanceCounter(&t0);
        if (client.Query(query, ranker::kDefaultPool, &files, &fileTotal)) {
            ranker::Rank(&files, query, kMaxFiles);
        } else {
            status = L"Everything did not answer";
        }
        QueryPerformanceCounter(&t1);
        wprintf(L"files: %zu of %lu (%.0f ms)\n", files.size(), fileTotal,
                Ms(t0, t1, f));
    }
    if (appHits.empty() && files.empty() && status.empty()) {
        status = L"No results";
    }

    // --- icons ------------------------------------------------------------
    QueryPerformanceCounter(&t0);
    std::vector<std::vector<BYTE>> appIcons(appHits.size());
    for (size_t i = 0; i < appHits.size(); i++) {
        HBITMAP bmp = apps::Index::LoadIcon(*appHits[i].app, kIconSize);
        if (bmp) {
            icons::BitmapToBgra(bmp, kIconSize, &appIcons[i]);
            DeleteObject(bmp);
        }
    }
    icons::ExtensionCache extensionIcons(kIconSize);
    QueryPerformanceCounter(&t1);
    wprintf(L"icons: %zu app icons (%.0f ms)\n", appIcons.size(),
            Ms(t0, t1, f));

    // --- serialise --------------------------------------------------------
    static DWORD seq = 1;
    std::vector<BYTE> payload;
    protocol::Header header{};
    header.magic = protocol::kMagic;
    header.version = protocol::kVersion;
    header.seq = seq;
    header.appCount = static_cast<DWORD>(appHits.size());
    header.fileCount = static_cast<DWORD>(files.size());
    header.fileTotal = fileTotal;
    header.statusLen = static_cast<DWORD>(status.size());
    const BYTE* hp = reinterpret_cast<const BYTE*>(&header);
    payload.insert(payload.end(), hp, hp + sizeof(header));
    AppendString(&payload, status);

    for (size_t i = 0; i < appHits.size(); i++) {
        AppendRow(&payload, protocol::kRowApp, 0, appHits[i].app->name, L"App",
                  appIcons[i].empty() ? nullptr : &appIcons[i]);
    }
    for (const auto& r : files) {
        const std::vector<BYTE>* icon =
            extensionIcons.Get(r.name, r.isFolder);
        AppendRow(&payload, protocol::kRowFile,
                  r.isFolder ? protocol::kRowFolder : 0, r.name, r.path, icon);
    }
    wprintf(L"payload: %zu bytes (%zu cached extension icons)\n",
            payload.size(), extensionIcons.size());

    // --- send -------------------------------------------------------------
    QueryPerformanceCounter(&t0);
    LRESULT sent = SendToPanel(panel, protocol::kMsgResults, payload.data(),
                               static_cast<DWORD>(payload.size()));
    QueryPerformanceCounter(&t1);
    wprintf(L"push: returned %lld in %.1f ms\n", static_cast<long long>(sent),
            Ms(t0, t1, f));

    // --- watch for a click ------------------------------------------------
    // This is the half that cannot be assumed to work: the panel has no way
    // to send anything upward, so it answers by returning a value from its
    // window procedure. Whether that value survives the integrity boundary is
    // exactly what this loop is here to find out.
    wprintf(L"\nclick a row -- polling for 20 s (ctrl-c to stop)\n");
    for (int i = 0; i < 200; i++) {
        Sleep(100);
        // A real payload, not a null one. WM_COPYDATA is marshalled by the
        // system between processes, and it is not worth finding out the hard
        // way whether a zero-length block survives that trip.
        DWORD ping = protocol::kMsgPoll;
        LRESULT value =
            SendToPanel(panel, protocol::kMsgPoll, &ping, sizeof(ping));
        if (value != 0) {
            wprintf(L"raw poll return: %lld (0x%llX)%lc",
                    static_cast<long long>(value),
                    static_cast<unsigned long long>(value), (wchar_t)10);
            fflush(stdout);
        }
        protocol::Action action = protocol::DecodeAction(value);
        if (!action.valid) {
            continue;
        }
        const wchar_t* kindName =
            action.kind == protocol::kRowApp ? L"app" : L"file";
        wprintf(L"ACTION: %ls index %lu (seq %lu)\n", kindName, action.index,
                action.seq);
        if (action.seq != seq) {
            wprintf(L"  stale -- that was a different batch, ignoring\n");
            continue;
        }
        if (action.kind == protocol::kRowApp && action.index < appHits.size()) {
            wprintf(L"  would launch: %ls\n",
                    appHits[action.index].app->name.c_str());
        } else if (action.kind == protocol::kRowFile &&
                   action.index < files.size()) {
            wprintf(L"  would open:   %ls\\%ls\n", files[action.index].path.c_str(),
                    files[action.index].name.c_str());
        }
    }

    CoUninitialize();
    return 0;
}
