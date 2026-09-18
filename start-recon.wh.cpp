// ==WindhawkMod==
// @id              start-recon
// @name            Start menu reconnaissance (temporary)
// @description     Logs the window lifecycle of the Start menu and the search page. Diagnostic only, changes nothing.
// @version         0.1
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         StartMenuExperienceHost.exe
// @architecture    x86-64
// @license         GPL-3.0
// @compilerOptions -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Start menu reconnaissance

Temporary diagnostic mod. It watches, from inside `StartMenuExperienceHost`,
what happens to that process's own windows when the search page takes over --
the question being whether Start's window survives the handoff or is destroyed.

It renders nothing and changes no behaviour. Output goes to
`%TEMP%\start-recon.log`.

Written because measuring this from outside needs someone at the keyboard at
the right moment; from inside, it simply records whenever the menu is used.
*/
// ==/WindhawkModReadme==

#include <windows.h>

#include <dwmapi.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

// This process is medium integrity and not an AppContainer, so a plain %TEMP%
// write lands where it says it does -- unlike SearchHost, whose writes are
// redirected into its package's private AC\Temp.
void Rec(const wchar_t* fmt, ...) {
    wchar_t body[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, fmt, args);
    va_end(args);

    wchar_t path[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, path);
    if (!n || n > MAX_PATH - 32) {
        return;
    }
    wcscat_s(path, MAX_PATH, L"start-recon.log");

    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    static const wchar_t kCrLf[] = {13, 10, 0};
    wchar_t line[1200];
    int len = wsprintfW(line, L"%02d:%02d:%02d.%03d  %ls%ls", st.wHour,
                        st.wMinute, st.wSecond, st.wMilliseconds, body, kCrLf);
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len * sizeof(wchar_t)), &written,
              nullptr);
    CloseHandle(h);
}

// ---------------------------------------------------------------------------
// Window sampling
// ---------------------------------------------------------------------------

struct WindowState {
    HWND hwnd;
    DWORD pid;
    std::wstring owner;  // "start" or "search"
    std::wstring cls;
    bool visible;
    int cloaked;
    RECT rect;
};

struct EnumCtx {
    DWORD startPid;
    std::vector<WindowState>* out;
};

BOOL CALLBACK EnumProc(HWND hwnd, LPARAM param) {
    auto* ctx = reinterpret_cast<EnumCtx*>(param);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);

    wchar_t cls[160] = {};
    GetClassNameW(hwnd, cls, ARRAYSIZE(cls));

    // Only the XAML-hosting window classes matter here.
    if (!wcsstr(cls, L"Windows.UI.Core") && !wcsstr(cls, L"Xaml")) {
        return TRUE;
    }

    // Our own process, plus whoever owns a window titled "Search" -- that is
    // how the search host is identified without enumerating processes.
    std::wstring owner;
    if (pid == ctx->startPid) {
        owner = L"start ";
    } else {
        wchar_t title[128] = {};
        GetWindowTextW(hwnd, title, ARRAYSIZE(title));
        if (wcscmp(title, L"Search") == 0) {
            owner = L"search";
        } else {
            return TRUE;
        }
    }

    WindowState s{};
    s.hwnd = hwnd;
    s.pid = pid;
    s.owner = owner;
    s.cls = cls;
    s.visible = IsWindowVisible(hwnd) != FALSE;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &s.cloaked, sizeof(s.cloaked));
    GetWindowRect(hwnd, &s.rect);
    ctx->out->push_back(std::move(s));
    return TRUE;
}

std::wstring Describe(const std::vector<WindowState>& windows) {
    if (windows.empty()) {
        return L"(no windows)";
    }
    std::wstring out;
    for (const WindowState& w : windows) {
        wchar_t line[512];
        wsprintfW(line, L"[%ls %ls vis=%d cloaked=%d %dx%d@%d,%d] ",
                  w.owner.c_str(), w.cls.c_str(), w.visible ? 1 : 0, w.cloaked,
                  w.rect.right - w.rect.left, w.rect.bottom - w.rect.top,
                  w.rect.left, w.rect.top);
        out += line;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

std::atomic<bool> g_quit{false};

// A std::thread global would call std::terminate at process exit if it were
// still joinable, taking the shell down with it.
[[clang::no_destroy]] std::thread g_worker;

void WorkerMain() {
    const DWORD selfPid = GetCurrentProcessId();
    Rec(L"=== recon attached to StartMenuExperienceHost pid=%lu ===", selfPid);
    Rec(L"watching for the Start window and the search window");

    std::wstring last = L"<unset>";
    while (!g_quit.load()) {
        std::vector<WindowState> windows;
        EnumCtx ctx{selfPid, &windows};
        EnumWindows(&EnumProc, reinterpret_cast<LPARAM>(&ctx));

        std::wstring now = Describe(windows);
        if (now != last) {
            Rec(L"%ls", now.c_str());
            last = now;
        }

        // Fine enough to catch the handoff, cheap enough to leave running.
        for (int i = 0; i < 10 && !g_quit.load(); i++) {
            Sleep(10);
        }
    }
    Rec(L"=== recon detaching ===");
}

}  // namespace

// ---------------------------------------------------------------------------
// Mod entry points
// ---------------------------------------------------------------------------

BOOL Wh_ModInit() {
    Wh_Log(L">");
    // Nothing here on purpose. Wh_ModInit runs before the host starts, and
    // work done here races the host's own startup.
    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");
    g_quit.store(false);
    g_worker = std::thread([] {
        // Nothing may escape a thread procedure: an unhandled exception there
        // is std::terminate, which aborts the host.
        try {
            WorkerMain();
        } catch (...) {
            Rec(L"worker threw; stopping");
        }
    });
}

void Wh_ModUninit() {
    Wh_Log(L">");
    g_quit.store(true);
    if (g_worker.joinable()) {
        g_worker.join();
    }
}
