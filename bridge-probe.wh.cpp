// ==WindhawkMod==
// @id              bridge-probe
// @name            Sandbox bridge probe (temporary)
// @description     Tests whether a normal-integrity process can send WM_COPYDATA into the sandboxed search host. Diagnostic only.
// @version         0.1
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         SearchHost.exe
// @architecture    x86-64
// @license         GPL-3.0
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Sandbox bridge probe

`SearchHost.exe` is a Low-integrity AppContainer, so code injected into it
cannot reach out to Everything. The proposed design pushes results *in*
instead: a normal-integrity broker sends them down via `WM_COPYDATA`, which
UIPI permits in that direction.

This mod tests only that one assumption. It creates a message-only window and
records anything that arrives. It renders nothing.

Output: the AppContainer's redirected temp, i.e.
`%LOCALAPPDATA%\Packages\MicrosoftWindows.Client.CBS_cw5n1h2txyewy\AC\Temp\bridge-probe.log`
*/
// ==/WindhawkModReadme==

#include <windows.h>

#include <atomic>
#include <thread>

namespace {

constexpr wchar_t kWindowClass[] = L"WindhawkEverythingBridgeProbe";
constexpr wchar_t kWindowTitle[] = L"WindhawkEverythingBridgeProbe";

void Rec(const wchar_t* fmt, ...) {
    wchar_t body[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, fmt, args);
    va_end(args);

    // Redirected into the package's private AC\Temp -- that redirection is
    // itself the proof this process is sandboxed.
    wchar_t path[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, path);
    if (!n || n > MAX_PATH - 32) {
        return;
    }
    wcscat_s(path, MAX_PATH, L"bridge-probe.log");

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

std::atomic<bool> g_quit{false};
std::atomic<HWND> g_hwnd{nullptr};

// Globals holding a thread must not be destroyed at process exit: a joinable
// std::thread destructor calls std::terminate and takes the host with it.
[[clang::no_destroy]] std::thread g_worker;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_COPYDATA) {
        auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (cds && cds->lpData && cds->cbData) {
            // Treat it as a wide string, bounded by what was actually sent.
            size_t chars = cds->cbData / sizeof(wchar_t);
            std::wstring text(static_cast<const wchar_t*>(cds->lpData),
                              chars);
            if (!text.empty() && text.back() == L'\0') {
                text.pop_back();
            }
            Rec(L"RECEIVED WM_COPYDATA  dwData=%llu  %u bytes  text='%ls'",
                static_cast<unsigned long long>(cds->dwData), cds->cbData,
                text.c_str());
            Rec(L"RESULT: a normal-integrity process CAN reach this "
                L"AppContainer window.");
        } else {
            Rec(L"RECEIVED WM_COPYDATA with no payload");
        }
        return TRUE;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void WorkerMain() {
    Rec(L"=== bridge probe attached, pid=%lu ===", GetCurrentProcessId());

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    if (!RegisterClassExW(&wc)) {
        Rec(L"RegisterClassEx failed: %lu", GetLastError());
        return;
    }

    // A real top-level window, not message-only: HWND_MESSAGE windows are not
    // discoverable by FindWindow from another process, and the broker has to
    // be able to find this one.
    HWND hwnd = CreateWindowExW(0, kWindowClass, kWindowTitle, WS_POPUP, 0, 0,
                                0, 0, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        Rec(L"CreateWindowEx failed: %lu", GetLastError());
        return;
    }
    g_hwnd.store(hwnd);
    Rec(L"listening on hwnd=%p class='%ls'", hwnd, kWindowClass);
    Rec(L"waiting for a WM_COPYDATA from the broker...");

    MSG msg;
    while (!g_quit.load()) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(15);
    }

    DestroyWindow(hwnd);
    g_hwnd.store(nullptr);
    UnregisterClassW(kWindowClass, wc.hInstance);
    Rec(L"=== bridge probe detaching ===");
}

}  // namespace

BOOL Wh_ModInit() {
    Wh_Log(L">");
    return TRUE;
}

void Wh_ModAfterInit() {
    Wh_Log(L">");
    g_quit.store(false);
    g_worker = std::thread([] {
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
