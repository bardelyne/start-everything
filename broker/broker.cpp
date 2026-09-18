// The broker.
//
// Runs at normal integrity outside the search host, because everything the
// panel needs is something an AppContainer cannot do: enumerate installed
// apps, talk to Everything, fetch icons, and launch things.
//
// The loop is one direction plus two reads. It reads the query the panel
// publishes in its window title, searches, and pushes rows down by
// WM_COPYDATA. Clicks come back as the return value of that same call. None
// of it is a message travelling upward, because none of those arrive.

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "apps_index.h"
#include "everything_ipc.h"
#include "file_ranker.h"
#include "icon_util.h"
#include "panel_protocol.h"

namespace {

constexpr int kIconSize = 32;
constexpr size_t kMaxApps = 6;
constexpr size_t kMaxFiles = 12;

// How often the loop runs. This is the click-to-launch latency, so it wants
// to be short; it is two cheap reads plus one message, so it can be.
constexpr DWORD kTickMs = 50;

// How long the query must stop changing before it is searched. Everything
// answers in about 30 ms and the apps filter in microseconds, so this is not
// about cost -- it is about not making the panel flicker through five
// different result sets while someone types one word.
constexpr DWORD kDebounceMs = 120;

// The apps list changes when something is installed or removed, which is
// rare, and enumerating costs about 230 ms, which is far too much to do per
// keystroke.
constexpr DWORD kAppRefreshMs = 5 * 60 * 1000;

bool g_verbose = false;

void Log(const wchar_t* fmt, ...) {
    if (!g_verbose) {
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    wprintf(L"%02d:%02d:%02d.%03d  ", st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds);
    va_list args;
    va_start(args, fmt);
    vwprintf(fmt, args);
    va_end(args);
    putwchar(L'\n');
    fflush(stdout);
}

// Never a bare SendMessage. A window left behind by a previous build of the
// mod has a window procedure pointing into an unloaded image; sending to one
// of those blocked for 3.9 s and then took the search host down with it.
LRESULT SendToPanel(HWND panel, DWORD message, const void* data, DWORD bytes) {
    COPYDATASTRUCT cds{};
    cds.dwData = message;
    cds.cbData = bytes;
    cds.lpData = const_cast<void*>(data);
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(panel, WM_COPYDATA, 0,
                             reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG,
                             1000, &result)) {
        return 0;
    }
    return static_cast<LRESULT>(result);
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
    AppendDword(buf, kind);
    AppendDword(buf, flags);
    AppendDword(buf, static_cast<DWORD>(title.size()));
    AppendDword(buf, static_cast<DWORD>(subtitle.size()));
    AppendDword(buf, icon ? kIconSize : 0);
    AppendDword(buf, icon ? static_cast<DWORD>(icon->size()) : 0);
    AppendString(buf, title);
    AppendString(buf, subtitle);
    if (icon) {
        buf->insert(buf->end(), icon->begin(), icon->end());
    }
}

// What was last sent, kept so a click can be turned back into something to
// open.
//
// The app rows hold cloned PIDLs rather than pointers into the index: the
// index is rebuilt periodically and every pointer into it dies when that
// happens, which a click arriving a moment later would follow. Re-deriving
// the item from its display name is not an option either -- that fails for
// the entries whose parsing name is a folder GUID.
struct Sent {
    DWORD seq = 0;
    std::vector<apps::UniquePidl> apps;
    std::vector<std::wstring> files;  // full paths
};

std::wstring ReadPublishedQuery(HWND panel) {
    // The panel writes the typed text into its own window title. Reading
    // another process's window text is a read, not a message, so it is not
    // what UIPI blocks.
    wchar_t buffer[512] = {};
    int n = GetWindowTextW(panel, buffer, ARRAYSIZE(buffer));
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring text(buffer, static_cast<size_t>(n));
    // Before the panel publishes anything the title is still the class name
    // it was created with, which is not a query.
    if (text == protocol::kWindowClass) {
        return std::wstring();
    }
    return text;
}

void Launch(const Sent& sent, const protocol::Action& action) {
    if (action.kind == protocol::kRowApp) {
        if (action.index >= sent.apps.size() || !sent.apps[action.index]) {
            return;
        }
        SHELLEXECUTEINFOW ei{};
        ei.cbSize = sizeof(ei);
        ei.fMask = SEE_MASK_IDLIST | SEE_MASK_FLAG_NO_UI;
        ei.lpIDList = sent.apps[action.index].get();
        ei.lpVerb = L"open";
        ei.nShow = SW_SHOWNORMAL;
        Log(L"launching app %lu", action.index);
        ShellExecuteExW(&ei);
        return;
    }
    if (action.index >= sent.files.size()) {
        return;
    }
    const std::wstring& path = sent.files[action.index];
    Log(L"opening %ls", path.c_str());
    SHELLEXECUTEINFOW ei{};
    ei.cbSize = sizeof(ei);
    ei.fMask = SEE_MASK_FLAG_NO_UI;
    ei.lpFile = path.c_str();
    ei.lpVerb = nullptr;  // the item's default verb
    ei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&ei);
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    for (int i = 1; i < argc; i++) {
        if (wcscmp(argv[i], L"-v") == 0 || wcscmp(argv[i], L"--verbose") == 0) {
            g_verbose = true;
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    apps::Index index;
    index.Rebuild();
    DWORD lastAppRefresh = GetTickCount();
    wprintf(L"apps indexed: %zu\n", index.Count());

    everything::Client client;
    bool everythingReady = client.Init();
    if (!everything::FindIpcWindow()) {
        wprintf(L"Everything is not running -- files will be empty until it "
                L"is\n");
    }

    icons::ExtensionCache extensionIcons(kIconSize);
    std::unordered_map<std::wstring, std::vector<BYTE>> appIconCache;

    HWND panel = nullptr;
    std::wstring appliedQuery;   // what the panel is currently showing
    std::wstring pendingQuery;   // what the box says now
    DWORD pendingSince = 0;
    bool pendingValid = false;
    DWORD seq = 0;
    Sent sent;

    wprintf(L"broker running -- open search and type. Ctrl-C to stop.\n");
    fflush(stdout);

    for (;;) {
        Sleep(kTickMs);

        if (!panel || !IsWindow(panel)) {
            HWND found = protocol::FindPanel();
            if (found != panel) {
                panel = found;
                appliedQuery.clear();
                pendingValid = false;
                if (panel) {
                    Log(L"panel found: %p", panel);
                }
            }
            if (!panel) {
                continue;
            }
        }

        // --- the query ----------------------------------------------------
        std::wstring current = ReadPublishedQuery(panel);
        if (current != pendingQuery) {
            pendingQuery = current;
            pendingSince = GetTickCount();
            pendingValid = true;
        }

        bool settled = pendingValid &&
                       (GetTickCount() - pendingSince) >= kDebounceMs &&
                       pendingQuery != appliedQuery;

        if (settled) {
            pendingValid = false;
            appliedQuery = pendingQuery;

            if (appliedQuery.empty()) {
                // Nothing typed: give the surface back rather than showing an
                // empty panel. This is what makes an empty box look like the
                // Start menu it is.
                SendToPanel(panel, protocol::kMsgRelease, nullptr, 0);
                sent = Sent{};
                Log(L"query cleared; released the surface");
                continue;
            }

            if (GetTickCount() - lastAppRefresh > kAppRefreshMs) {
                index.Rebuild();
                appIconCache.clear();
                lastAppRefresh = GetTickCount();
                Log(L"apps re-indexed: %zu", index.Count());
            }

            auto appHits = index.Search(appliedQuery, kMaxApps);

            std::vector<everything::Result> files;
            DWORD fileTotal = 0;
            std::wstring status;
            if (!everything::FindIpcWindow()) {
                status = L"Everything is not running";
            } else if (!everythingReady) {
                status = L"Could not talk to Everything";
            } else if (client.Query(appliedQuery, ranker::kDefaultPool, &files,
                                    &fileTotal)) {
                ranker::Rank(&files, appliedQuery, kMaxFiles);
            } else {
                status = L"Everything did not answer";
            }
            if (appHits.empty() && files.empty() && status.empty()) {
                status = L"No results";
            }

            seq = (seq + 1) & 0x7FFF;  // the panel sends back 15 bits of it

            Sent fresh;
            fresh.seq = seq;
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

            for (const auto& hit : appHits) {
                auto cached = appIconCache.find(hit.app->name);
                if (cached == appIconCache.end()) {
                    std::vector<BYTE> pixels;
                    HBITMAP bmp = apps::Index::LoadIcon(*hit.app, kIconSize);
                    if (bmp) {
                        icons::BitmapToBgra(bmp, kIconSize, &pixels);
                        DeleteObject(bmp);
                    }
                    cached = appIconCache.emplace(hit.app->name,
                                                  std::move(pixels)).first;
                }
                AppendRow(&payload, protocol::kRowApp, 0, hit.app->name,
                          L"App",
                          cached->second.empty() ? nullptr : &cached->second);

                ITEMIDLIST* clone = ILCloneFull(hit.app->pidl.get());
                fresh.apps.push_back(apps::UniquePidl(clone));
            }

            for (const auto& r : files) {
                const std::vector<BYTE>* icon =
                    extensionIcons.Get(r.name, r.isFolder);
                AppendRow(&payload, protocol::kRowFile,
                          r.isFolder ? protocol::kRowFolder : 0, r.name,
                          r.path, icon);

                std::wstring full = r.path;
                if (!full.empty() && full.back() != L'\\') {
                    full += L'\\';
                }
                full += r.name;
                fresh.files.push_back(std::move(full));
            }

            sent = std::move(fresh);
            LRESULT reply = SendToPanel(panel, protocol::kMsgResults,
                                        payload.data(),
                                        static_cast<DWORD>(payload.size()));
            Log(L"'%ls' -> %zu apps, %zu of %lu files, %zu bytes",
                appliedQuery.c_str(), appHits.size(), files.size(), fileTotal,
                payload.size());

            // The push carries a click back like any other message.
            protocol::Action action = protocol::DecodeAction(reply);
            if (action.valid && action.seq == sent.seq) {
                Launch(sent, action);
            }
            continue;
        }

        // --- clicks -------------------------------------------------------
        DWORD ping = protocol::kMsgPoll;
        LRESULT reply =
            SendToPanel(panel, protocol::kMsgPoll, &ping, sizeof(ping));
        protocol::Action action = protocol::DecodeAction(reply);
        if (!action.valid) {
            continue;
        }
        if (action.seq != sent.seq) {
            // The click was against a result set that has since been
            // replaced, so the index it carries no longer means anything.
            Log(L"stale click (seq %lu, current %lu) ignored", action.seq,
                sent.seq);
            continue;
        }
        Launch(sent, action);
    }

    CoUninitialize();
    return 0;
}
