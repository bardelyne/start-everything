// ==WindhawkMod==
// @id              start-everything
// @name            Everything results in the Start menu
// @description     Native Everything search inside the Start menu, complete SearchHost disconnection, and seamless focus management.
// @version         0.2
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         StartMenuExperienceHost.exe
// @include         SearchHost.exe
// @include         explorer.exe
// @architecture    x86-64
// @license         GPL-3.0
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luuid -lshell32 -lshlwapi -lcomctl32 -ldwmapi -luser32 -liphlpapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Everything Results in the Start Menu

A high-performance, native replacement for Windows 11 Start Menu search powered directly by voidtools Everything. Completely severs SearchHost background telemetry, Bing web queries, and Edge WebView2 processes, replacing them with instantaneous sub-millisecond local file, application, and settings search directly inside the Start Menu.

## Key Features

- Instant Everything Search: Sub-millisecond file querying directly through the voidtools Everything Win32 IPC interface. Instant results across millions of files without background indexing lag or disk thrashing.
- Smart Apps and Settings Search: Fuzzy matching across Desktop applications, Microsoft Store / UWP packages, Control Panel applets, and Windows Settings URIs (ms-settings:) with high-resolution shell icons.
- On-Demand Animated Palette: The Start Menu stays completely clean and uncluttered when idle. The search palette smoothly reveals with a 140ms ease-out animation the moment you type or click the top search trigger, and collapses on empty or Escape.
- Complete SearchHost Disconnection: Intercepts SearchBoxViewModel::NotifyQueryTextChanged in SearchUx.UI.dll to completely stop background Bing queries, Edge WebView2 child processes, and indexing CPU spikes.
- Inline Calculator: Type /c <expression> (e.g. /c 100 * 5, /c sqrt(144), /c 15% of 200, /c 2^10) to evaluate math expressions instantly. Press Enter to copy the result.
- Configurable Unit Conversions: Type /c <number> [unit] to convert units using formulas configured in Mod Settings. Users can add, edit, or delete conversion items individually from the settings UI.
- Network Interface Inspector: Type /ip to display all active Wi-Fi, Ethernet, and VPN network interfaces with their IP addresses, subnet masks, gateways, and hardware descriptions. Press Enter to copy the IP.
- Full Right-Click Context Menu: Right-click any file or folder to Open, Run as Administrator, Cut (native shell file move), Copy (native shell file duplicate), Copy path, or Open file location.
- Explicit Web Search: Trigger web searches on demand using the '?' prefix (e.g. '?query'). Includes customizable keyword shortcuts such as '?yt' (YouTube), '?gh' (GitHub), '?w' (Wikipedia), and '?r' (Reddit).
- Start Menu Styler Compatibility: Automatically syncs background styles (Tinted Glass, Acrylic, custom theme colors) in real time without restarting the mod.
- Robust Win32 Key Listener: Combines a WH_GETMESSAGE UI thread hook, HWND subclassing, and XAML CoreWindow handling to ensure zero dropped keystrokes.
- Shell Focus Protection: Intercepts explorer.exe foreground redirection to prevent SearchHost from stealing focus away from the Start Menu.

## Requirements

1. Windows 11 (versions 22H2, 23H2, 24H2, x86-64).
2. voidtools Everything (version 1.4 or 1.5a) running in the background.

## Recommended Setup: Hide Taskbar Search

In Windows 11, clicking or typing into the taskbar search box opens the standalone SearchHost flyout rather than the Start Menu. Because this mod replaces Start Menu search and disconnects SearchHost background queries, it is strongly recommended to hide the search icon/box from your taskbar:
1. Right-click the Taskbar and select Taskbar settings (or Settings > Personalization > Taskbar).
2. Under Taskbar items, set Search to Hide.
All searches will now seamlessly route through the native Start Menu (Windows Key or Start button).

## Keyboard Shortcuts

- Type any key: Automatically reveals the search palette, focuses the search box, and queries apps and files.
- Up / Down: Navigate through application, calculation, conversion, and file results.
- Enter: Launch the selected application, copy calculation/conversion/IP result, or open item.
- Ctrl + Enter: Run the selected application or file as Administrator (triggers UAC).
- Escape: Clear the current query and smoothly collapse the search palette back to pinned apps.
- Right-Click: Context menu with Open, Run as Administrator, Cut, Copy, Copy path, and Open file location.

## Command Reference

- /c <expression>: Calculate math expression (e.g. /c 100 * 5, /c sqrt(144), /c 15% of 200).
- /c <number>: Display all configured unit conversions and programmer radix (Hex, Bin, Oct).
- /c <number> <unit>: Targeted unit conversion (e.g. /c 100 km, /c 32 c, /c 50 lbs).
- /ip: List all active network interfaces and IP addresses.
- ?<term>: Web search using default search engine.
- ?<shortcut> <term>: Targeted web search (e.g. ?yt lo-fi, ?gh windhawk, ?w physics, ?r windows).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- maxAppResults: 6
  $name: Max App Results
  $description: Number of application matches to display in the Apps column (default 6).
- maxFileResults: 12
  $name: Max File Results
  $description: Number of file matches to display in the Files column (default 12).
- showKeyHints: true
  $name: Show Keyboard Shortcuts Bar
  $description: Display the keyboard shortcut hints ([↑↓] Select, [↵] Open, [Ctrl+↵] Admin, [Esc] Close) in the bottom bar.
- defaultSearchUrl: "https://duckduckgo.com/?q={q}"
  $name: Default Search Engine URL
  $description: >-
    The URL template for standard web searches. Use {q} for the query placeholder.
    Defaults to DuckDuckGo.
- webShortcuts:
    - - prefix: "yt"
        $name: Shortcut Keyword
        $description: Keyword to trigger this search (e.g. "?yt music")
      - name: "YouTube"
        $name: Service Name
      - url: "https://www.youtube.com/results?search_query={q}"
        $name: Search URL
        $description: URL template with {q} placeholder
    - - prefix: "gh"
        $name: Shortcut Keyword
      - name: "GitHub"
        $name: Service Name
      - url: "https://github.com/search?q={q}"
        $name: Search URL
    - - prefix: "w"
        $name: Shortcut Keyword
      - name: "Wikipedia"
        $name: Service Name
      - url: "https://en.wikipedia.org/wiki/Special:Search?search={q}"
        $name: Search URL
    - - prefix: "r"
        $name: Shortcut Keyword
      - name: "Reddit"
        $name: Service Name
      - url: "https://www.reddit.com/search/?q={q}"
        $name: Search URL
- unitConversions:
    - - fromUnit: "km"
        $name: Source Unit
        $description: Trigger unit (e.g. km)
      - toUnit: "miles"
        $name: Target Unit
      - formula: "x * 0.621371"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Distance"
        $name: Category
    - - fromUnit: "c"
        $name: Source Unit
        $description: Trigger unit (e.g. c)
      - toUnit: "°F"
        $name: Target Unit
      - formula: "x * 9 / 5 + 32"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Temperature"
        $name: Category
    - - fromUnit: "kg"
        $name: Source Unit
        $description: Trigger unit (e.g. kg)
      - toUnit: "lbs"
        $name: Target Unit
      - formula: "x * 2.20462"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Weight"
        $name: Category
    - - fromUnit: "m"
        $name: Source Unit
        $description: Trigger unit (e.g. m)
      - toUnit: "feet"
        $name: Target Unit
      - formula: "x * 3.28084"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Length"
        $name: Category
    - - fromUnit: "cm"
        $name: Source Unit
        $description: Trigger unit (e.g. cm)
      - toUnit: "in"
        $name: Target Unit
      - formula: "x / 2.54"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Length"
        $name: Category
    - - fromUnit: "mb"
        $name: Source Unit
        $description: Trigger unit (e.g. mb)
      - toUnit: "GB"
        $name: Target Unit
      - formula: "x / 1024"
        $name: Formula
        $description: Formula using 'x' as input number
      - category: "Storage"
        $name: Category
  $name: Custom Unit Conversions
  $description: >-
    Configurable unit conversions for /c <number> [unit].
    Formulas evaluate using 'x' as input. You can add, edit, or remove items individually at any time.
*/
// ==/WindhawkModSettings==

#include <initguid.h>  // must precede xamlom.h

#include <inspectable.h>
#include <xamlom.h>

// winbase.h defines GetCurrentTime as a macro, which collides with
// Windows.UI.Xaml.Media.Animation's method of the same name.
#pragma push_macro("GetCurrentTime")
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
// Not just the .0.h forward declarations: Append/Size have deduced return
// types and must be defined before use.
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <shlguid.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shlwapi.h>

#include "broker/apps_index.h"
#include "broker/calc_and_tools.h"
#include "broker/everything_ipc.h"
#include "broker/file_ranker.h"
#include "broker/icon_util.h"

#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>

#pragma pop_macro("GetCurrentTime")

#include <robuffer.h>
#ifndef WH_MOD_ID
#define WH_MOD_ID L"start-everything"
#endif
#ifndef WH_MOD_VERSION
#define WH_MOD_VERSION L"0.1"
#endif

#include <windhawk_utils.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <memory>
#include <string>
#include <string_view>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace wf = winrt::Windows::Foundation;
namespace wut = winrt::Windows::UI::Text;
namespace wuc = winrt::Windows::UI::Core;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxcp = winrt::Windows::UI::Xaml::Controls::Primitives;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuxmi = winrt::Windows::UI::Xaml::Media::Imaging;
namespace wuxma = winrt::Windows::UI::Xaml::Media::Animation;

namespace {

void Rec(const wchar_t* fmt, ...) {
    wchar_t body[2048] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, fmt, args);
    va_end(args);

    wchar_t path[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, path);
    if (!n || n > MAX_PATH - 40) {
        return;
    }
    wcscat_s(path, MAX_PATH, L"start-everything.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    static const wchar_t kCrLf[] = {13, 10, 0};
    wchar_t line[2200];
    int len = wsprintfW(line, L"%02d:%02d:%02d.%03d  %ls%ls", st.wHour,
                        st.wMinute, st.wSecond, st.wMilliseconds, body, kCrLf);
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len * sizeof(wchar_t)), &written,
              nullptr);
    CloseHandle(h);
}

}  // namespace

static std::atomic<bool> g_quit{false};

// ===========================================================================
// Domain: Process Identification
// ===========================================================================

enum class TargetProcess {
    Unknown,
    StartMenu,
    SearchHost,
    Explorer
};

static TargetProcess g_targetProcess = TargetProcess::Unknown;

TargetProcess IdentifyCurrentProcess() {
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(NULL, path, MAX_PATH);
    PCWSTR name = wcsrchr(path, L'\\');
    name = name ? (name + 1) : path;
    if (_wcsicmp(name, L"StartMenuExperienceHost.exe") == 0) return TargetProcess::StartMenu;
    if (_wcsicmp(name, L"SearchHost.exe") == 0) return TargetProcess::SearchHost;
    if (_wcsicmp(name, L"explorer.exe") == 0) return TargetProcess::Explorer;
    return TargetProcess::Unknown;
}

// ===========================================================================
// Domain: explorer.exe (Shell Focus Redirection)
// ===========================================================================

using Explorer_SetForegroundWindow_t = BOOL(WINAPI*)(HWND);
static Explorer_SetForegroundWindow_t pOriginalExplorerSetForegroundWindow = nullptr;

static bool IsProcessNamed(DWORD pid, const wchar_t* name) {
    if (!pid || pid == GetCurrentProcessId()) return false;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    bool match = false;
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        match = (StrStrIW(path, name) != nullptr);
    }
    CloseHandle(hProcess);
    return match;
}

static HWND FindStartMenuCoreWindow() {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        HWND h = reinterpret_cast<HWND>(GetPropW(tray, L"WindhawkStartMenuHwnd"));
        if (h && IsWindow(h)) return h;
    }

    HWND found = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        if (GetPropW(hwnd, L"WindhawkStartMenuWindow")) {
            *reinterpret_cast<HWND*>(lParam) = hwnd;
            return FALSE;
        }
        wchar_t cls[64] = {};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (IsProcessNamed(pid, L"StartMenuExperienceHost.exe")) {
                *reinterpret_cast<HWND*>(lParam) = hwnd;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&found));

    if (found && IsWindow(found)) {
        if (tray) {
            SetPropW(tray, L"WindhawkStartMenuHwnd", found);
        }
        return found;
    }

    HWND hStart = FindWindowW(L"Windows.UI.Core.CoreWindow", L"Start");
    if (hStart && IsWindow(hStart)) {
        if (tray) {
            SetPropW(tray, L"WindhawkStartMenuHwnd", hStart);
        }
        return hStart;
    }

    return nullptr;
}

static BOOL WINAPI Hook_Explorer_SetForegroundWindow(HWND hWnd) {
    if (!hWnd) {
        return pOriginalExplorerSetForegroundWindow(hWnd);
    }

    DWORD targetPid = 0;
    GetWindowThreadProcessId(hWnd, &targetPid);

    if (IsProcessNamed(targetPid, L"SearchHost.exe")) {
        Rec(L"[Explorer] Blocked SetForegroundWindow to SearchHost.exe (0x%p)", hWnd);

        // Redirect shell foreground activation directly to Start Menu CoreWindow
        HWND hStart = FindStartMenuCoreWindow();
        if (hStart && IsWindow(hStart)) {
            Rec(L"[Explorer] Preserving foreground on StartMenu window 0x%p", hStart);
            pOriginalExplorerSetForegroundWindow(hStart);
        }
        return TRUE; // Pretend success
    }

    return pOriginalExplorerSetForegroundWindow(hWnd);
}

using Explorer_BringWindowToTop_t = BOOL(WINAPI*)(HWND);
static Explorer_BringWindowToTop_t pOriginalExplorerBringWindowToTop = nullptr;

static BOOL WINAPI Hook_Explorer_BringWindowToTop(HWND hWnd) {
    if (!hWnd) return pOriginalExplorerBringWindowToTop(hWnd);
    DWORD targetPid = 0;
    GetWindowThreadProcessId(hWnd, &targetPid);
    if (IsProcessNamed(targetPid, L"SearchHost.exe")) {
        Rec(L"[Explorer] Blocked BringWindowToTop to SearchHost.exe (0x%p)", hWnd);
        HWND hStart = FindStartMenuCoreWindow();
        if (hStart && IsWindow(hStart)) {
            pOriginalExplorerBringWindowToTop(hStart);
        }
        return TRUE;
    }
    return pOriginalExplorerBringWindowToTop(hWnd);
}

using Explorer_SwitchToThisWindow_t = void(WINAPI*)(HWND, BOOL);
static Explorer_SwitchToThisWindow_t pOriginalExplorerSwitchToThisWindow = nullptr;

static void WINAPI Hook_Explorer_SwitchToThisWindow(HWND hWnd, BOOL fAltTab) {
    if (!hWnd) return;
    DWORD targetPid = 0;
    GetWindowThreadProcessId(hWnd, &targetPid);
    if (IsProcessNamed(targetPid, L"SearchHost.exe")) {
        Rec(L"[Explorer] Blocked SwitchToThisWindow to SearchHost.exe (0x%p)", hWnd);
        HWND hStart = FindStartMenuCoreWindow();
        if (hStart && IsWindow(hStart)) {
            if (pOriginalExplorerSwitchToThisWindow) {
                pOriginalExplorerSwitchToThisWindow(hStart, fAltTab);
            } else {
                SetForegroundWindow(hStart);
            }
        }
        return;
    }
    if (pOriginalExplorerSwitchToThisWindow) {
        pOriginalExplorerSwitchToThisWindow(hWnd, fAltTab);
    }
}

void InitExplorer() {
    Rec(L"=== start-everything: initializing explorer.exe shell hooks ===");
    Wh_SetFunctionHook((void*)SetForegroundWindow, (void*)Hook_Explorer_SetForegroundWindow,
                       (void**)&pOriginalExplorerSetForegroundWindow);
    Wh_SetFunctionHook((void*)BringWindowToTop, (void*)Hook_Explorer_BringWindowToTop,
                       (void**)&pOriginalExplorerBringWindowToTop);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        void* pSwitch = (void*)GetProcAddress(hUser32, "SwitchToThisWindow");
        if (pSwitch) {
            Wh_SetFunctionHook(pSwitch, (void*)Hook_Explorer_SwitchToThisWindow,
                               (void**)&pOriginalExplorerSwitchToThisWindow);
        }
    }
}

// ===========================================================================
// Domain: SearchHost.exe (SearchHost Disconnect & Suppression)
// ===========================================================================

struct DisconnectRecursionGuard {
    bool& flag;
    explicit DisconnectRecursionGuard(bool& f) : flag(f) { flag = true; }
    ~DisconnectRecursionGuard() { flag = false; }
};

using CreateProcessW_t = decltype(&CreateProcessW);
static CreateProcessW_t pOriginalCreateProcessW = nullptr;

static bool IsWebViewProcess(LPCWSTR text) {
    if (!text) return false;
    std::wstring lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return (lower.find(L"msedgewebview2.exe") != std::wstring::npos) ||
           (lower.find(L"embeddedbrowserwebview") != std::wstring::npos) ||
           (lower.find(L"searchindexer.exe") != std::wstring::npos);
}

static BOOL WINAPI Hook_SearchHost_CreateProcessW(
    LPCWSTR applicationName, LPWSTR commandLine,
    LPSECURITY_ATTRIBUTES processAttributes,
    LPSECURITY_ATTRIBUTES threadAttributes,
    BOOL inheritHandles, DWORD creationFlags,
    LPVOID environment, LPCWSTR currentDirectory,
    LPSTARTUPINFOW startupInfo,
    LPPROCESS_INFORMATION processInformation) {
    thread_local bool inHook = false;
    if (inHook) {
        return pOriginalCreateProcessW(applicationName, commandLine,
                                      processAttributes, threadAttributes,
                                      inheritHandles, creationFlags, environment,
                                      currentDirectory, startupInfo, processInformation);
    }
    DisconnectRecursionGuard guard(inHook);

    if (IsWebViewProcess(applicationName) || IsWebViewProcess(commandLine)) {
        Rec(L"[SearchHost] Blocked WebView2 launch: app=%ls, cmd=%ls",
            applicationName ? applicationName : L"(null)",
            commandLine ? commandLine : L"(null)");
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }

    return pOriginalCreateProcessW(applicationName, commandLine,
                                  processAttributes, threadAttributes,
                                  inheritHandles, creationFlags, environment,
                                  currentDirectory, startupInfo, processInformation);
}

using CreateFileW_t = decltype(&CreateFileW);
static CreateFileW_t pOriginalCreateFileW = nullptr;

static bool IsRestrictedDatabaseFile(LPCWSTR lpFileName) {
    if (!lpFileName) return false;
    std::wstring lower(lpFileName);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });

    return (lower.find(L"appsindex.db") != std::wstring::npos ||
            lower.find(L"settings.db") != std::wstring::npos ||
            lower.find(L"windows.edb") != std::wstring::npos);
}

static HANDLE WINAPI Hook_SearchHost_CreateFileW(
    LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    thread_local bool inHook = false;
    if (inHook) {
        return pOriginalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                    lpSecurityAttributes, dwCreationDisposition,
                                    dwFlagsAndAttributes, hTemplateFile);
    }
    DisconnectRecursionGuard guard(inHook);

    if (IsRestrictedDatabaseFile(lpFileName)) {
        Rec(L"[SearchHost] Blocked access to search database: %ls", lpFileName);
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }

    return pOriginalCreateFileW(lpFileName, dwDesiredAccess, dwShareMode,
                                lpSecurityAttributes, dwCreationDisposition,
                                dwFlagsAndAttributes, hTemplateFile);
}

using CoCreateInstance_t = decltype(&CoCreateInstance);
static CoCreateInstance_t pOriginalCoCreateInstance = nullptr;

static const GUID kSearchManager = {0x7D096C5F, 0xAC08, 0x4F1F, {0xBE, 0xB7, 0x5C, 0x22, 0xC5, 0x17, 0xCE, 0x39}};
static const GUID kCollatorUtilities = {0x9E175B8B, 0xF52A, 0x11D8, {0xB9, 0xA5, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30}};
static const GUID kSearchQuery = {0x0B63E349, 0x9CCC, 0x11D0, {0xBC, 0xDB, 0x00, 0x80, 0x5F, 0xCC, 0xCE, 0x04}};
static const GUID kSearchFolder = {0x323CA680, 0xC24D, 0x4099, {0xB9, 0xD4, 0x44, 0x6D, 0xD2, 0xD7, 0x24, 0x9E}};

static HRESULT WINAPI Hook_SearchHost_CoCreateInstance(
    REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext,
    REFIID riid, LPVOID* ppv) {
    thread_local bool inHook = false;
    if (inHook) {
        return pOriginalCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
    }
    DisconnectRecursionGuard guard(inHook);

    if (IsEqualGUID(rclsid, kSearchManager) ||
        IsEqualGUID(rclsid, kCollatorUtilities) ||
        IsEqualGUID(rclsid, kSearchQuery) ||
        IsEqualGUID(rclsid, kSearchFolder)) {
        Rec(L"[SearchHost] Blocked Windows Search COM activation");
        if (ppv) *ppv = nullptr;
        return REGDB_E_CLASSNOTREG;
    }

    return pOriginalCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}

static LRESULT CALLBACK SearchHostSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    if (uMsg == WM_ACTIVATE) {
        if (LOWORD(wParam) != WA_INACTIVE) {
            Rec(L"[SearchHost] WM_ACTIVATE (active) on 0x%p -> redirecting foreground to StartMenu", hWnd);
            HWND hStart = FindStartMenuCoreWindow();
            if (hStart && IsWindow(hStart)) {
                DWORD startTid = GetWindowThreadProcessId(hStart, nullptr);
                DWORD curTid = GetCurrentThreadId();
                if (startTid && startTid != curTid) {
                    AttachThreadInput(curTid, startTid, TRUE);
                    SetForegroundWindow(hStart);
                    BringWindowToTop(hStart);
                    AttachThreadInput(curTid, startTid, FALSE);
                } else {
                    SetForegroundWindow(hStart);
                    BringWindowToTop(hStart);
                }
            }
            return 0;
        }
    } else if (uMsg == WM_SETFOCUS) {
        Rec(L"[SearchHost] WM_SETFOCUS on 0x%p -> redirecting foreground to StartMenu", hWnd);
        HWND hStart = FindStartMenuCoreWindow();
        if (hStart && IsWindow(hStart)) {
            SetForegroundWindow(hStart);
            BringWindowToTop(hStart);
        }
        return 0;
    } else if (uMsg == WM_WINDOWPOSCHANGING) {
        WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (wp) {
            wp->flags |= SWP_HIDEWINDOW;
            wp->flags &= ~SWP_SHOWWINDOW;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

using SetForegroundWindow_t = BOOL(WINAPI*)(HWND);
static SetForegroundWindow_t pOrigSearchHostSetForegroundWindow = nullptr;
static BOOL WINAPI Hook_SearchHost_SetForegroundWindow(HWND hWnd) {
    Rec(L"[SearchHost] SetForegroundWindow called for 0x%p", hWnd);
    HWND hStart = FindStartMenuCoreWindow();
    if (hStart && IsWindow(hStart)) {
        Rec(L"[SearchHost] Redirecting SetForegroundWindow to StartMenu 0x%p", hStart);
        DWORD startTid = GetWindowThreadProcessId(hStart, nullptr);
        DWORD curTid = GetCurrentThreadId();
        if (startTid && startTid != curTid) {
            AttachThreadInput(curTid, startTid, TRUE);
            pOrigSearchHostSetForegroundWindow(hStart);
            BringWindowToTop(hStart);
            AttachThreadInput(curTid, startTid, FALSE);
        } else {
            pOrigSearchHostSetForegroundWindow(hStart);
            BringWindowToTop(hStart);
        }
    }
    return TRUE;
}

using BringWindowToTop_t = BOOL(WINAPI*)(HWND);
static BringWindowToTop_t pOrigSearchHostBringWindowToTop = nullptr;
static BOOL WINAPI Hook_SearchHost_BringWindowToTop(HWND hWnd) {
    Rec(L"[SearchHost] BringWindowToTop called for 0x%p", hWnd);
    HWND hStart = FindStartMenuCoreWindow();
    if (hStart && IsWindow(hStart)) {
        pOrigSearchHostBringWindowToTop(hStart);
    }
    return TRUE;
}

using ShowWindow_t = BOOL(WINAPI*)(HWND, int);
static ShowWindow_t pOrigSearchHostShowWindow = nullptr;
static BOOL WINAPI Hook_SearchHost_ShowWindow(HWND hWnd, int nCmdShow) {
    if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL || nCmdShow == SW_RESTORE || nCmdShow == SW_SHOWDEFAULT) {
        Rec(L"[SearchHost] Redirected SearchHost ShowWindow to SW_HIDE");
        nCmdShow = SW_HIDE;
    }
    return pOrigSearchHostShowWindow(hWnd, nCmdShow);
}

using SetWindowPos_t = BOOL(WINAPI*)(HWND, HWND, int, int, int, int, UINT);
static SetWindowPos_t pOrigSearchHostSetWindowPos = nullptr;
static BOOL WINAPI Hook_SearchHost_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    if (uFlags & SWP_SHOWWINDOW) {
        uFlags &= ~SWP_SHOWWINDOW;
        uFlags |= SWP_HIDEWINDOW;
    }
    return pOrigSearchHostSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
}

using SwitchToThisWindow_t = void(WINAPI*)(HWND, BOOL);
static SwitchToThisWindow_t pOrigSearchHostSwitchToThisWindow = nullptr;
static void WINAPI Hook_SearchHost_SwitchToThisWindow(HWND hWnd, BOOL fAltTab) {
    Rec(L"[SearchHost] SwitchToThisWindow called for 0x%p", hWnd);
    HWND hStart = FindStartMenuCoreWindow();
    if (hStart && IsWindow(hStart)) {
        SetForegroundWindow(hStart);
    }
}

using NotifyQueryTextChanged_t = void (*)(void* thisPtr, void* queryText, int selStart, int selLen);
static NotifyQueryTextChanged_t pOrigNotifyQueryTextChanged = nullptr;
static void Hook_NotifyQueryTextChanged(void* thisPtr, void* queryText, int selStart, int selLen) {
    Rec(L"[SearchHost] Suppressed SearchBoxViewModel::NotifyQueryTextChanged (0 searches, 0 CPU)");
    return;
}

using CallHandler_t = HRESULT (*)(void* handler, void* str);
static CallHandler_t pOrigCallHandler = nullptr;
static HRESULT Hook_CallHandler(void* handler, void* str) {
    thread_local bool inHook = false;
    if (inHook) {
        return pOrigCallHandler(handler, nullptr);
    }
    DisconnectRecursionGuard guard(inHook);
    return pOrigCallHandler(handler, nullptr);
}

static bool GetTextSection(HMODULE hMod, BYTE** outStart, size_t* outSize) {
    if (!hMod) return false;
    auto dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(hMod);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>((BYTE*)hMod + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return false;

    auto section = IMAGE_FIRST_SECTION(ntHeaders);
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; ++i, ++section) {
        if (strncmp((char*)section->Name, ".text", 5) == 0) {
            *outStart = (BYTE*)hMod + section->VirtualAddress;
            *outSize = section->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

static void* FindPattern(BYTE* base, size_t size, const BYTE* pattern, const char* mask) {
    size_t patternLen = strlen(mask);
    if (size < patternLen) return nullptr;
    for (size_t i = 0; i <= size - patternLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < patternLen; ++j) {
            if (mask[j] == 'x' && base[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return base + i;
        }
    }
    return nullptr;
}

static bool g_searchUxHooked = false;
static void HookSearchUx() {
    if (g_searchUxHooked) return;
    HMODULE hSearchUx = GetModuleHandleW(L"SearchUx.UI.dll");
    if (!hSearchUx) {
        hSearchUx = LoadLibraryW(L"SearchUx.UI.dll");
    }
    if (!hSearchUx) return;

    BYTE* textStart = nullptr;
    size_t textSize = 0;
    if (!GetTextSection(hSearchUx, &textStart, &textSize)) {
        Rec(L"[SearchHost] Failed to query .text section of SearchUx.UI.dll");
        return;
    }

    const BYTE sigNotify[] = {
        0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
        0x48, 0x8D, 0x6C, 0x24, 0xF1, 0x48, 0x81, 0xEC, 0xB8, 0x00, 0x00, 0x00
    };
    const char maskNotify[] = "xxxxxxxxxxxxxxxxxxxxxxxxx";

    void* pNotify = nullptr;
    void* pKnownNotify = (void*)((BYTE*)hSearchUx + 0x191F40);
    if (memcmp(pKnownNotify, sigNotify, sizeof(sigNotify)) == 0) {
        pNotify = pKnownNotify;
    } else {
        pNotify = FindPattern(textStart, textSize, sigNotify, maskNotify);
    }

    if (pNotify) {
        Wh_SetFunctionHook(pNotify, (void*)Hook_NotifyQueryTextChanged, (void**)&pOrigNotifyQueryTextChanged);
        Rec(L"[SearchHost] Hooked SearchUx.UI.dll!NotifyQueryTextChanged (CPU Spike Killer)");
    }

    const BYTE sigAuto[] = {
        0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x01, 0x48, 0x8B, 0x40, 0x48,
        0xFF, 0x15, 0x00, 0x00, 0x00, 0x00, 0x85, 0xC0, 0x79, 0x08
    };
    const char maskAuto[] = "xxxxxxxxxxxxx????xxxx";

    void* pAuto = nullptr;
    void* pKnownAuto = (void*)((BYTE*)hSearchUx + 0x2819F0);
    BYTE* bKnownAuto = (BYTE*)pKnownAuto;
    if (bKnownAuto[0] == 0x48 && bKnownAuto[1] == 0x83 && bKnownAuto[2] == 0xEC && bKnownAuto[3] == 0x28) {
        pAuto = pKnownAuto;
    } else {
        pAuto = FindPattern(textStart, textSize, sigAuto, maskAuto);
    }

    if (pAuto) {
        Wh_SetFunctionHook(pAuto, (void*)Hook_CallHandler, (void**)&pOrigCallHandler);
        Rec(L"[SearchHost] Hooked SearchUx.UI.dll!SetAutoCompleteQueryText");
    }

    g_searchUxHooked = true;
}

void InitSearchHost() {
    Rec(L"=== start-everything: initializing SearchHost disconnect & suppression hooks ===");
    Wh_SetFunctionHook((void*)CreateProcessW, (void*)Hook_SearchHost_CreateProcessW, (void**)&pOriginalCreateProcessW);
    Wh_SetFunctionHook((void*)CreateFileW, (void*)Hook_SearchHost_CreateFileW, (void**)&pOriginalCreateFileW);
    Wh_SetFunctionHook((void*)CoCreateInstance, (void*)Hook_SearchHost_CoCreateInstance, (void**)&pOriginalCoCreateInstance);
    Wh_SetFunctionHook((void*)SetForegroundWindow, (void*)Hook_SearchHost_SetForegroundWindow, (void**)&pOrigSearchHostSetForegroundWindow);
    Wh_SetFunctionHook((void*)BringWindowToTop, (void*)Hook_SearchHost_BringWindowToTop, (void**)&pOrigSearchHostBringWindowToTop);
    Wh_SetFunctionHook((void*)ShowWindow, (void*)Hook_SearchHost_ShowWindow, (void**)&pOrigSearchHostShowWindow);
    Wh_SetFunctionHook((void*)SetWindowPos, (void*)Hook_SearchHost_SetWindowPos, (void**)&pOrigSearchHostSetWindowPos);

    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        void* pSwitch = (void*)GetProcAddress(hUser32, "SwitchToThisWindow");
        if (pSwitch) {
            Wh_SetFunctionHook(pSwitch, (void*)Hook_SearchHost_SwitchToThisWindow, (void**)&pOrigSearchHostSwitchToThisWindow);
        }
    }

    HookSearchUx();
}

[[clang::no_destroy]] static std::thread g_searchHostWatchdog;
static void StartSearchHostWatchdog() {
    g_searchHostWatchdog = std::thread([] {
        for (int i = 0; i < 120 && !g_quit.load(); ++i) {
            EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                if (pid == GetCurrentProcessId()) {
                    SetWindowSubclass(hwnd, SearchHostSubclassProc, 201, 0);
                }
                return TRUE;
            }, 0);
            Sleep(500);
        }
    });
}

// ===========================================================================
// Domain: StartMenuExperienceHost.exe
// ===========================================================================

// Defined much further down, next to the TAP that used to contain them.
// Declared at global scope, which is where they are defined -- declaring them
// inside the anonymous namespace below would create a second, undefined pair
// and make every call that can see both ambiguous.
void TeardownStartMenuUi();
UINT GetTeardownMessage();

namespace {

struct WebShortcut {
    std::wstring prefix;
    std::wstring name;
    std::wstring url;
};

struct CustomConversion {
    std::wstring fromUnit;
    std::wstring toUnit;
    std::wstring formula;
    std::wstring category;
};

struct Settings {
    std::wstring defaultSearchUrl = L"https://duckduckgo.com/?q={q}";
    std::vector<WebShortcut> webShortcuts;
    std::vector<CustomConversion> unitConversions;
    int maxAppResults = 6;
    int maxFileResults = 12;
    bool showKeyHints = true;
};

[[clang::no_destroy]] Settings g_settings;
[[clang::no_destroy]] std::mutex g_settingsMutex;
std::atomic<DWORD> g_xamlThreadId{0};
std::atomic<int> g_seen{0};

void LoadSettings() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);

    auto defUrl = WindhawkUtils::StringSetting::make(L"defaultSearchUrl");
    if (defUrl.get() && *defUrl.get()) {
        g_settings.defaultSearchUrl = defUrl.get();
    } else {
        g_settings.defaultSearchUrl = L"https://duckduckgo.com/?q={q}";
    }

    g_settings.webShortcuts.clear();
    for (int i = 0;; ++i) {
        auto pfx = WindhawkUtils::StringSetting::make(L"webShortcuts[%d].prefix", i);
        if (!pfx.get() || !*pfx.get()) break;
        auto name = WindhawkUtils::StringSetting::make(L"webShortcuts[%d].name", i);
        auto url = WindhawkUtils::StringSetting::make(L"webShortcuts[%d].url", i);

        WebShortcut s;
        s.prefix = pfx.get();
        for (auto& c : s.prefix) c = static_cast<wchar_t>(towlower(c));
        s.name = (name.get() && *name.get()) ? name.get() : L"Web";
        s.url = (url.get() && *url.get()) ? url.get() : L"";
        if (!s.prefix.empty() && !s.url.empty()) {
            g_settings.webShortcuts.push_back(std::move(s));
        }
    }
    if (g_settings.webShortcuts.empty()) {
        g_settings.webShortcuts.push_back({L"yt", L"YouTube", L"https://www.youtube.com/results?search_query={q}"});
        g_settings.webShortcuts.push_back({L"gh", L"GitHub", L"https://github.com/search?q={q}"});
        g_settings.webShortcuts.push_back({L"w", L"Wikipedia", L"https://en.wikipedia.org/wiki/Special:Search?search={q}"});
        g_settings.webShortcuts.push_back({L"r", L"Reddit", L"https://www.reddit.com/search/?q={q}"});
    }

    g_settings.unitConversions.clear();
    for (int i = 0;; ++i) {
        auto fromU = WindhawkUtils::StringSetting::make(L"unitConversions[%d].fromUnit", i);
        if (!fromU.get() || !*fromU.get()) break;
        auto toU = WindhawkUtils::StringSetting::make(L"unitConversions[%d].toUnit", i);
        auto formula = WindhawkUtils::StringSetting::make(L"unitConversions[%d].formula", i);
        auto cat = WindhawkUtils::StringSetting::make(L"unitConversions[%d].category", i);

        CustomConversion c;
        c.fromUnit = tools::ToLower(tools::Trim(fromU.get()));
        c.toUnit = (toU.get() && *toU.get()) ? tools::Trim(toU.get()) : L"";
        c.formula = (formula.get() && *formula.get()) ? tools::Trim(formula.get()) : L"";
        c.category = (cat.get() && *cat.get()) ? tools::Trim(cat.get()) : L"Conversion";

        if (!c.fromUnit.empty() && !c.toUnit.empty() && !c.formula.empty()) {
            g_settings.unitConversions.push_back(std::move(c));
        }
    }
    if (g_settings.unitConversions.empty()) {
        g_settings.unitConversions.push_back({L"km", L"miles", L"x * 0.621371", L"Distance"});
        g_settings.unitConversions.push_back({L"c", L"\u00B0F", L"x * 9 / 5 + 32", L"Temperature"});
        g_settings.unitConversions.push_back({L"kg", L"lbs", L"x * 2.20462", L"Weight"});
        g_settings.unitConversions.push_back({L"m", L"feet", L"x * 3.28084", L"Length"});
        g_settings.unitConversions.push_back({L"cm", L"in", L"x / 2.54", L"Length"});
        g_settings.unitConversions.push_back({L"mb", L"GB", L"x / 1024", L"Storage"});
    }

    int maxApps = Wh_GetIntSetting(L"maxAppResults");
    g_settings.maxAppResults = (maxApps > 0) ? std::clamp(maxApps, 1, 30) : 6;

    int maxFiles = Wh_GetIntSetting(L"maxFileResults");
    g_settings.maxFileResults = (maxFiles > 0) ? std::clamp(maxFiles, 1, 50) : 12;

    auto hintsStr = WindhawkUtils::StringSetting::make(L"showKeyHints");
    if (!hintsStr.get() || !*hintsStr.get()) {
        g_settings.showKeyHints = true;
    } else {
        g_settings.showKeyHints = (wcscmp(hintsStr.get(), L"0") != 0 && _wcsicmp(hintsStr.get(), L"false") != 0);
    }

    Rec(L"=== settings: defSearch=%ls shortcuts=%zu maxApps=%d maxFiles=%d hints=%d ===",
        g_settings.defaultSearchUrl.c_str(), g_settings.webShortcuts.size(),
        g_settings.maxAppResults, g_settings.maxFileResults,
        g_settings.showKeyHints ? 1 : 0);
}

HMODULE GetCurrentModuleHandle() {
    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           L"", &module)) {
        return nullptr;
    }
    return module;
}

wux::DependencyObject FindDescendantByName(wux::DependencyObject const& root,
                                           std::wstring_view name,
                                           int maxDepth) {
    if (maxDepth < 0) {
        return nullptr;
    }
    if (auto fe = root.try_as<wux::FrameworkElement>()) {
        if (std::wstring_view{fe.Name()} == name) {
            return root;
        }
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        auto child = wuxm::VisualTreeHelper::GetChild(root, i);
        if (auto found = FindDescendantByName(child, name, maxDepth - 1)) {
            return found;
        }
    }
    return nullptr;
}

// Type plus x:Name, for log lines that have to be matched against what a
// tree inspector shows.

std::wstring ElementLabel(wux::DependencyObject const& obj) {
    std::wstring label;
    try {
        label = winrt::get_class_name(obj);
    } catch (...) {
        label = L"<unknown>";
    }
    if (auto fe = obj.try_as<wux::FrameworkElement>()) {
        std::wstring name{fe.Name()};
        if (!name.empty()) {
            label += L"#" + name;
        }
    }
    return label;
}


// ---------------------------------------------------------------------------
// A search box that is actually a search box
//
// The stock one is a Button: a placeholder TextBlock, two icons and a
// Rectangle called TextCaret drawn to look like a cursor. Typing into it is
// not possible because there is nothing there to type into -- activating it
// hands off to SearchHost, which owns the only real text box in the whole
// arrangement.
//
// So this collapses the decoy and puts a TextBox in the same grid cell, at the
// same size, to see whether the Start menu will host one at all.
// ---------------------------------------------------------------------------

// What currently has keyboard focus, for the log.
std::wstring FocusedElementLabel() {
    try {
        auto focused = wux::Input::FocusManager::GetFocusedElement();
        if (!focused) {
            return L"(nothing)";
        }
        if (auto dobj = focused.try_as<wux::DependencyObject>()) {
            return ElementLabel(dobj);
        }
        return std::wstring{winrt::get_class_name(focused)};
    } catch (...) {
        return L"(threw)";
    }
}

[[clang::no_destroy]] wuxc::TextBox g_ourBox{nullptr};
[[clang::no_destroy]] wux::FrameworkElement g_stockButton{nullptr};
[[clang::no_destroy]] wuxc::Grid g_resultsHost{nullptr};
[[clang::no_destroy]] wuxc::StackPanel g_resultsList{nullptr};
[[clang::no_destroy]] wuxc::StackPanel g_appsList{nullptr};
[[clang::no_destroy]] std::vector<wuxc::Button> g_appButtons;

struct AppCardUI {
    int appIndex = -1;
    std::wstring title;
    std::wstring openPath;
    wuxc::Button button{nullptr};
    bool canRunAsAdmin = true;
};
[[clang::no_destroy]] std::vector<AppCardUI> g_activeApps;
[[clang::no_destroy]] wuxc::Border g_appsHeaderHolder{nullptr};
[[clang::no_destroy]] wuxc::Border g_filesHeaderHolder{nullptr};
[[clang::no_destroy]] wuxm::TranslateTransform g_resultsTranslate{nullptr};
[[clang::no_destroy]] wuxma::Storyboard g_revealAnim{nullptr};
[[clang::no_destroy]] wuxma::Storyboard g_hideAnim{nullptr};
std::atomic<bool> g_isOverlayVisible{false};
std::atomic<bool> g_isHiding{false};

void HideStockPlaceholder(wux::FrameworkElement const& stock);
void RevealOverlayAnimated();
void HideOverlayAnimated();
void HideAllOtherSearchBoxes(wux::DependencyObject const& root, int depth = 15);
void SyncOverlayBackground();

[[clang::no_destroy]] wuxc::TextBox::TextChanged_revoker g_ourBoxChanged;
[[clang::no_destroy]] wux::UIElement::LostFocus_revoker g_ourBoxLost;
[[clang::no_destroy]] wux::DispatcherTimer g_focusProbe{nullptr};
[[clang::no_destroy]] wux::DispatcherTimer g_openFocus{nullptr};
[[clang::no_destroy]] wux::DispatcherTimer g_refocus{nullptr};

wuxc::Border FindMenuAcrylicBorder() {
    try {
        wux::DependencyObject start = g_resultsHost ? g_resultsHost : g_stockButton;
        if (!start) return nullptr;

        wux::DependencyObject root = start;
        wux::DependencyObject node = start;
        for (int up = 0; up < 12; ++up) {
            auto p = wuxm::VisualTreeHelper::GetParent(node);
            if (!p) break;
            root = p;
            node = p;
        }

        if (auto found = FindDescendantByName(root, L"AcrylicBorder", 10)) {
            if (auto b = found.try_as<wuxc::Border>()) {
                return b;
            }
        }
    } catch (...) {}
    return nullptr;
}

void SyncOverlayBackground() {
    if (!g_resultsHost) return;
    try {
        if (auto border = FindMenuAcrylicBorder()) {
            if (auto brush = border.Background()) {
                if (g_resultsHost.Background() != brush) {
                    g_resultsHost.Background(brush);
                    Rec(L"SyncOverlayBackground: synchronized background from AcrylicBorder (%ls)",
                        winrt::get_class_name(brush).c_str());
                }
                try {
                    auto cr = border.CornerRadius();
                    g_resultsHost.CornerRadius(cr);
                } catch (...) {}
                return;
            }
        }
    } catch (...) {}
}

void RevealOverlayAnimated() {
    if (!g_resultsHost) return;
    try {
        // If already visible and not in the process of hiding, nothing to animate!
        if (g_isOverlayVisible.load() && !g_isHiding.load()) {
            return;
        }

        g_isOverlayVisible.store(true);
        g_isHiding.store(false);

        if (g_hideAnim) {
            g_hideAnim.Stop();
            g_hideAnim = nullptr;
        }

        SyncOverlayBackground();
        g_resultsHost.Visibility(wux::Visibility::Visible);
        g_resultsHost.IsHitTestVisible(true);

        if (g_resultsHost.Opacity() >= 0.98 && g_resultsTranslate && std::abs(g_resultsTranslate.Y()) < 0.1) {
            g_resultsHost.Opacity(1.0);
            g_resultsHost.IsHitTestVisible(true);
            if (g_resultsTranslate) g_resultsTranslate.Y(0.0);
            return;
        }

        if (g_revealAnim) {
            g_revealAnim.Stop();
            g_revealAnim = nullptr;
        }

        wuxma::Storyboard sb;
        wuxma::CubicEase ease;
        ease.EasingMode(wuxma::EasingMode::EaseOut);

        wuxma::DoubleAnimation animOpacity;
        double currentOpacity = g_resultsHost.Opacity();
        animOpacity.From(currentOpacity < 0.95 ? currentOpacity : 0.0);
        animOpacity.To(1.0);
        animOpacity.Duration(wux::DurationHelper::FromTimeSpan(std::chrono::milliseconds(120)));
        animOpacity.EasingFunction(ease);
        wuxma::Storyboard::SetTarget(animOpacity, g_resultsHost);
        wuxma::Storyboard::SetTargetProperty(animOpacity, L"Opacity");
        sb.Children().Append(animOpacity);

        if (g_resultsTranslate) {
            wuxma::DoubleAnimation animY;
            double currentY = g_resultsTranslate.Y();
            animY.From(currentY < -0.5 ? currentY : -8.0);
            animY.To(0.0);
            animY.Duration(wux::DurationHelper::FromTimeSpan(std::chrono::milliseconds(120)));
            animY.EasingFunction(ease);
            wuxma::Storyboard::SetTarget(animY, g_resultsTranslate);
            wuxma::Storyboard::SetTargetProperty(animY, L"Y");
            sb.Children().Append(animY);
        }

        sb.Completed([](wf::IInspectable const&, wf::IInspectable const&) {
            if (g_isOverlayVisible.load() && g_resultsHost) {
                g_resultsHost.Opacity(1.0);
                g_resultsHost.IsHitTestVisible(true);
                if (g_resultsTranslate) g_resultsTranslate.Y(0.0);
            }
        });

        g_revealAnim = sb;
        sb.Begin();
    } catch (...) {}
}

void HideOverlayAnimated() {
    if (!g_resultsHost) return;
    try {
        if (g_isHiding.load()) {
            return;
        }
        if (!g_isOverlayVisible.load()) {
            return;
        }
        g_isOverlayVisible.store(false);
        g_isHiding.store(true);

        if (g_revealAnim) {
            g_revealAnim.Stop();
            g_revealAnim = nullptr;
        }

        wuxma::Storyboard sb;
        wuxma::DoubleAnimation animOpacity;
        double currentOpacity = g_resultsHost.Opacity();
        animOpacity.From(currentOpacity > 0.05 ? currentOpacity : 1.0);
        animOpacity.To(0.0);
        animOpacity.Duration(wux::DurationHelper::FromTimeSpan(std::chrono::milliseconds(180)));
        wuxma::Storyboard::SetTarget(animOpacity, g_resultsHost);
        wuxma::Storyboard::SetTargetProperty(animOpacity, L"Opacity");

        wuxma::CubicEase ease;
        ease.EasingMode(wuxma::EasingMode::EaseOut);
        animOpacity.EasingFunction(ease);
        sb.Children().Append(animOpacity);

        sb.Completed([](wf::IInspectable const&, wf::IInspectable const&) {
            g_isHiding.store(false);
            if (!g_isOverlayVisible.load() && g_resultsHost) {
                g_resultsHost.Opacity(0.0);
                g_resultsHost.IsHitTestVisible(false);
                if (g_resultsTranslate) {
                    g_resultsTranslate.Y(-8.0);
                }
                if (g_resultsList) g_resultsList.Children().Clear();
                if (g_appsList) g_appsList.Children().Clear();
                g_activeApps.clear();
                g_appButtons.clear();
            }
        });

        g_hideAnim = sb;
        sb.Begin();
    } catch (...) {
        g_isHiding.store(false);
        if (g_resultsHost) {
            g_resultsHost.Opacity(0.0);
            g_resultsHost.IsHitTestVisible(false);
        }
    }
}

void HideAllOtherSearchBoxes(wux::DependencyObject const& root, int depth) {
    if (!root || depth < 0) return;
    try {
        if (auto fe = root.try_as<wux::FrameworkElement>()) {
            std::wstring name{fe.Name()};
            std::wstring cls;
            try { cls = winrt::get_class_name(root); } catch (...) {}

            if (name.find(L"Windhawk") == std::wstring::npos) {
                if (cls.find(L"SearchBox") != std::wstring::npos ||
                    cls.find(L"RichSearch") != std::wstring::npos ||
                    cls.find(L"SearchControl") != std::wstring::npos ||
                    name.find(L"SearchBox") != std::wstring::npos ||
                    name.find(L"SearchBlock") != std::wstring::npos) {
                    fe.Visibility(wux::Visibility::Collapsed);
                    fe.Opacity(0.0);
                    fe.IsHitTestVisible(false);
                    if (auto ctl = fe.try_as<wuxc::Control>()) {
                        ctl.IsTabStop(false);
                    }
                    Rec(L"HideAllOtherSearchBoxes: suppressed %ls#%ls", cls.c_str(), name.c_str());
                }
            }
        }
        int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
        for (int i = 0; i < count; ++i) {
            HideAllOtherSearchBoxes(wuxm::VisualTreeHelper::GetChild(root, i), depth - 1);
        }
    } catch (...) {}
}

static HWND g_hCoreWindow = nullptr;

HWND GetOurCoreWindow() {
    if (g_hCoreWindow && IsWindow(g_hCoreWindow)) {
        return g_hCoreWindow;
    }
    HWND ours = nullptr;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != GetCurrentProcessId()) {
                return TRUE;
            }
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
            if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
                *reinterpret_cast<HWND*>(param) = hwnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ours));
    if (ours) {
        g_hCoreWindow = ours;
    }
    return ours;
}

bool IsOurWindowCloaked() {
    HWND ours = GetOurCoreWindow();
    if (!ours) return true;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(ours, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return cloaked != 0;
    }
    return false;
}

std::atomic<bool> g_suppressRefocus{false};

void TakeForeground(bool force = false) {
    try {
        if (!force && g_suppressRefocus.load()) {
            return;
        }

        HWND ours = GetOurCoreWindow();
        if (!ours || !IsWindow(ours)) {
            return;
        }

        HWND current = GetForegroundWindow();
        if (current == ours) {
            return;
        }

        DWORD ourWindowTid = GetWindowThreadProcessId(ours, nullptr);
        DWORD currentTid = current ? GetWindowThreadProcessId(current, nullptr) : 0;
        DWORD callerTid = GetCurrentThreadId();

        // Simulate Alt press/release to bypass Windows foreground restriction
        keybd_event(VK_MENU, 0, 0, 0);
        keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);

        if (currentTid && currentTid != callerTid) {
            AttachThreadInput(callerTid, currentTid, TRUE);
            if (ourWindowTid && ourWindowTid != callerTid && ourWindowTid != currentTid) {
                AttachThreadInput(ourWindowTid, currentTid, TRUE);
            }
            SetForegroundWindow(ours);
            BringWindowToTop(ours);
            if (ourWindowTid && ourWindowTid != callerTid && ourWindowTid != currentTid) {
                AttachThreadInput(ourWindowTid, currentTid, FALSE);
            }
            AttachThreadInput(callerTid, currentTid, FALSE);
        } else {
            SetForegroundWindow(ours);
            BringWindowToTop(ours);
        }
    } catch (...) {
    }
}

void FocusOurBoxNow() {
    if (g_suppressRefocus.load()) return;
    if (!g_ourBox) return;

    try {
        auto now = wux::Input::FocusManager::GetFocusedElement();
        if (now && now == g_ourBox) {
            return; // Already focused! Do not re-focus or interrupt typing!
        }
        if (g_isOverlayVisible.load() && now && (now.try_as<wuxc::Button>() || 
                                                now.try_as<wuxc::GridViewItem>() || 
                                                now.try_as<wuxc::ListViewItem>())) {
            return;
        }
    } catch (...) {}

    TakeForeground();
    try {
        bool ok = g_ourBox.Focus(wux::FocusState::Programmatic);
        g_ourBox.SelectionStart(static_cast<int32_t>(g_ourBox.Text().size()));
        g_ourBox.SelectionLength(0);
        Rec(L"focus: FocusOurBoxNow -> Focus result=%d, focused=%ls",
            ok ? 1 : 0, FocusedElementLabel().c_str());
    } catch (...) {}
}

void TriggerMenuOpenFocus() {
    try {
        g_suppressRefocus.store(false);
        FocusOurBoxNow();
        if (g_openFocus) {
            g_openFocus.Stop();
            g_openFocus = nullptr;
        }
        auto t = wux::DispatcherTimer();
        t.Interval(std::chrono::milliseconds(50));
        auto ticks = std::make_shared<int>(0);
        t.Tick([t, ticks](wf::IInspectable const&, wf::IInspectable const&) {
            if (g_suppressRefocus.load()) {
                t.Stop();
                return;
            }
            FocusOurBoxNow();
            if (++(*ticks) >= 4) {
                t.Stop();
            }
        });
        t.Start();
        g_openFocus = t;
    } catch (...) {
    }
}

void DismissStartMenu() {
    try {
        g_suppressRefocus.store(true);
        if (g_refocus) {
            g_refocus.Stop();
            g_refocus = nullptr;
        }
        if (g_openFocus) {
            g_openFocus.Stop();
            g_openFocus = nullptr;
        }
        if (g_ourBox) {
            g_ourBox.Text(L"");
        }
        if (g_resultsHost) {
            g_resultsHost.Visibility(wux::Visibility::Visible);
            g_resultsHost.Opacity(0.0);
            g_resultsHost.IsHitTestVisible(false);
            if (g_resultsTranslate) g_resultsTranslate.Y(-8.0);
        }
        g_isOverlayVisible.store(false);
        HWND ours = GetOurCoreWindow();
        if (ours) {
            PostMessageW(ours, WM_KEYDOWN, VK_ESCAPE, 0x00010001);
            PostMessageW(ours, WM_KEYUP, VK_ESCAPE, 0xC0010001);
        }
        keybd_event(VK_ESCAPE, 0, 0, 0);
        keybd_event(VK_ESCAPE, 0, KEYEVENTF_KEYUP, 0);
        Rec(L"DismissStartMenu: sent dismiss signal");
    } catch (...) {
    }
}

void RestoreWindowSoon() {
    try {
        if (g_suppressRefocus.load()) return;
        if (!g_isOverlayVisible.load()) return;
        if (g_refocus) {
            g_refocus.Stop();
            g_refocus = nullptr;
        }
        auto back = wux::DispatcherTimer();
        back.Interval(std::chrono::milliseconds(40));
        back.Tick([back](wf::IInspectable const&, wf::IInspectable const&) {
            back.Stop();
            if (g_suppressRefocus.load()) return;
            if (!g_isOverlayVisible.load()) return;
            if (g_ourBox) {
                auto now = wux::Input::FocusManager::GetFocusedElement();
                if (now && now == g_ourBox) {
                    return; // ALREADY focused! Leave it alone!
                }
                // If user clicked/focused an app or result item, keep it!
                if (now && (now.try_as<wuxc::Button>() || 
                            now.try_as<wuxc::GridViewItem>() || 
                            now.try_as<wuxc::ListViewItem>())) {
                    return;
                }
                TakeForeground();
                g_ourBox.Focus(wux::FocusState::Programmatic);
                g_ourBox.SelectionStart(static_cast<int32_t>(g_ourBox.Text().size()));
                g_ourBox.SelectionLength(0);
            }
        });
        back.Start();
        g_refocus = back;
    } catch (...) {
    }
}

void DisarmScrollTabStops(wux::DependencyObject const& root, int depth = 15) {
    if (!root || depth < 0) return;
    if (auto scroller = root.try_as<wuxc::ScrollViewer>()) {
        if (scroller.IsTabStop()) {
            scroller.IsTabStop(false);
            Rec(L"focus: proactively disabled IsTabStop on %ls", ElementLabel(root).c_str());
        }
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        DisarmScrollTabStops(wuxm::VisualTreeHelper::GetChild(root, i), depth - 1);
    }
}

void HandleNavigationKey(winrt::Windows::System::VirtualKey key, bool ctrl);

static HHOOK g_hGetMsgHook = nullptr;
static winrt::event_token g_charReceivedToken{};
static winrt::event_token g_keyDownToken{};
static winrt::event_token g_activatedToken{};
static bool g_coreEventsHooked = false;

static DWORD s_lastCharTick = 0;
static wchar_t s_lastChar = 0;

bool ProcessKeyChar(wchar_t ch) {
    if (ch < 0x20 || ch == 0x7F) return false;
    if (!g_ourBox || !g_resultsHost) return false;

    // If our search box is already focused, let standard XAML TextBox handle typing natively.
    // Its TextChanged handler will automatically reveal the overlay and query results without double characters.
    try {
        auto focused = wux::Input::FocusManager::GetFocusedElement();
        if (focused && focused == g_ourBox) {
            if (!g_isOverlayVisible.load()) {
                RevealOverlayAnimated();
            }
            return false;
        }
    } catch (...) {}

    // Deduplicate rapid duplicate events (e.g. from WM_CHAR and CharacterReceived) within 60ms
    DWORD now = GetTickCount();
    if ((now - s_lastCharTick < 60) && s_lastChar == ch) {
        return true; // Consume duplicate event so XAML doesn't double-type it
    }
    s_lastCharTick = now;
    s_lastChar = ch;

    Rec(L"key listener: intercepted char '%c' (0x%X) [overlayVisible=%d]",
        ch, static_cast<unsigned>(ch), g_isOverlayVisible.load() ? 1 : 0);
    try {
        g_suppressRefocus.store(false);
        RevealOverlayAnimated();
        std::wstring text{g_ourBox.Text()};
        text.push_back(ch);
        g_ourBox.Text(text);
        g_ourBox.Focus(wux::FocusState::Programmatic);
        g_ourBox.SelectionStart(static_cast<int32_t>(text.size()));
        g_ourBox.SelectionLength(0);
        return true;
    } catch (...) {
        return false;
    }
}

bool ProcessKeyCommand(WPARAM vk) {
    if (!g_ourBox || !g_resultsHost) return false;

    if (vk == VK_ESCAPE) {
        if (g_isOverlayVisible.load() || g_isHiding.load()) {
            Rec(L"key listener: intercepted Escape -> hiding overlay");
            try {
                if (g_ourBox) g_ourBox.Text(L"");
                HideOverlayAnimated();
            } catch (...) {}
            return true;
        }
        return false;
    }

    if (vk == VK_DOWN || vk == VK_UP || vk == VK_RETURN) {
        if (g_isOverlayVisible.load()) {
            bool ctrl = (GetKeyState(VK_CONTROL) < 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
            try {
                HandleNavigationKey(static_cast<winrt::Windows::System::VirtualKey>(vk), ctrl);
            } catch (...) {}
            return true;
        }
        return false;
    }

    if (vk == VK_BACK) {
        bool alreadyFocused = false;
        try {
            auto focused = wux::Input::FocusManager::GetFocusedElement();
            if (focused && focused == g_ourBox) alreadyFocused = true;
        } catch (...) {}

        if (!alreadyFocused) {
            std::wstring text{g_ourBox.Text()};
            if (!text.empty()) {
                text.pop_back();
                try {
                    g_ourBox.Text(text);
                    if (text.empty()) {
                        HideOverlayAnimated();
                    } else {
                        RevealOverlayAnimated();
                        g_ourBox.Focus(wux::FocusState::Programmatic);
                        g_ourBox.SelectionStart(static_cast<int32_t>(text.size()));
                    }
                } catch (...) {}
                return true;
            }
        }
    }

    return false;
}

LRESULT CALLBACK StartMenuGetMsgProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0 && lParam) {
        MSG* msg = reinterpret_cast<MSG*>(lParam);
        if (msg) {
            if (msg->message == WM_CHAR) {
                if (ProcessKeyChar(static_cast<wchar_t>(msg->wParam))) {
                    msg->message = WM_NULL;
                }
            } else if (msg->message == WM_KEYDOWN) {
                if (ProcessKeyCommand(msg->wParam)) {
                    msg->message = WM_NULL;
                }
            }
        }
    }
    return CallNextHookEx(g_hGetMsgHook, code, wParam, lParam);
}

void InstallMessageHook() {
    if (g_hGetMsgHook) return;
    DWORD tid = g_xamlThreadId.load();
    if (!tid) tid = GetCurrentThreadId();
    g_hGetMsgHook = SetWindowsHookExW(WH_GETMESSAGE, StartMenuGetMsgProc, NULL, tid);
    if (g_hGetMsgHook) {
        Rec(L"key listener: installed WH_GETMESSAGE hook on thread %lu", tid);
    }
}

static bool g_subclassed = false;
static LRESULT CALLBACK StartMenuSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    // Unload, marshalled here from Windhawk's thread. This proc runs on the
    // thread that owns the window, which is the XAML thread, which is the one
    // thread allowed to touch any of it.
    if (uMsg && uMsg == GetTeardownMessage()) {
        Rec(L"subclass: teardown message received");
        TeardownStartMenuUi();
        return 0;
    }
    if (uMsg == WM_ACTIVATE) {
        HWND otherHwnd = reinterpret_cast<HWND>(lParam);
        DWORD otherPid = 0;
        if (otherHwnd) GetWindowThreadProcessId(otherHwnd, &otherPid);

        if (LOWORD(wParam) != WA_INACTIVE) {
            g_suppressRefocus.store(false);
            Rec(L"subclass: WM_ACTIVATE (active, prev=%p) -> ready for search", otherHwnd);
            if (g_ourBox && !g_isOverlayVisible.load()) {
                g_ourBox.Text(L"");
            }
            if (g_resultsHost && !g_isOverlayVisible.load()) {
                g_resultsHost.Visibility(wux::Visibility::Visible);
                g_resultsHost.Opacity(0.0);
                g_resultsHost.IsHitTestVisible(false);
                if (g_resultsTranslate) g_resultsTranslate.Y(-8.0);
                SyncOverlayBackground();
            }
            TriggerMenuOpenFocus();
        } else {
            // If the other window is in our own process (e.g. context menu, flyout, tooltip), don't suppress refocus!
            if (otherPid == GetCurrentProcessId()) {
                Rec(L"subclass: WM_ACTIVATE (inactive, internal other=%p) -> ignoring", otherHwnd);
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }

            g_suppressRefocus.store(true);
            Rec(L"subclass: WM_ACTIVATE (inactive, other=%p pid=%lu) -> suppressing refocus", otherHwnd, otherPid);
            if (g_resultsHost) {
                g_resultsHost.Visibility(wux::Visibility::Visible);
                g_resultsHost.Opacity(0.0);
                g_resultsHost.IsHitTestVisible(false);
                if (g_resultsTranslate) g_resultsTranslate.Y(-8.0);
            }
            g_isOverlayVisible.store(false);
            g_isHiding.store(false);
            if (g_hideAnim) g_hideAnim.Stop();
            if (g_revealAnim) g_revealAnim.Stop();
            if (g_openFocus) {
                g_openFocus.Stop();
                g_openFocus = nullptr;
            }
            if (g_refocus) {
                g_refocus.Stop();
                g_refocus = nullptr;
            }
        }
    } else if (uMsg == WM_WINDOWPOSCHANGED) {
        WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
        if (wp && !(wp->flags & SWP_HIDEWINDOW)) {
            if (!IsOurWindowCloaked()) {
                HWND fg = GetForegroundWindow();
                if (fg != hWnd) {
                    DWORD fgPid = 0;
                    if (fg) GetWindowThreadProcessId(fg, &fgPid);
                    if (fgPid != GetCurrentProcessId()) {
                        Rec(L"subclass: WM_WINDOWPOSCHANGED uncloaked, fg=%p (ours=%p) -> claiming foreground", fg, hWnd);
                        g_suppressRefocus.store(false);
                        TakeForeground(true);
                        TriggerMenuOpenFocus();
                    }
                }
            }
        }
    } else if (uMsg == WM_SETFOCUS) {
        g_suppressRefocus.store(false);
        TriggerMenuOpenFocus();
    } else if (uMsg == WM_CHAR) {
        if (ProcessKeyChar(static_cast<wchar_t>(wParam))) {
            return 0;
        }
    } else if (uMsg == WM_KEYDOWN) {
        if (ProcessKeyCommand(wParam)) {
            return 0;
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void PlaceOurSearchBox(wux::FrameworkElement const& stockButton);

// The search box, found by walking rather than by being told.
//
// Same set of types the diagnostics callback matched on: the stock toggle is
// what we replace, and anything else search-shaped gets suppressed so two
// boxes are never live at once.
wux::FrameworkElement FindStockSearchToggle(wux::DependencyObject const& root,
                                            int depth = 24) {
    if (!root || depth < 0) {
        return nullptr;
    }
    try {
        std::wstring_view type{winrt::get_class_name(root)};
        if (type == L"StartMenu.SearchBoxToggleButton") {
            if (auto fe = root.try_as<wux::FrameworkElement>()) {
                std::wstring name{fe.Name()};
                if (name.find(L"Windhawk") == std::wstring::npos) {
                    return fe;
                }
            }
        }
    } catch (...) {
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        if (auto found = FindStockSearchToggle(
                wuxm::VisualTreeHelper::GetChild(root, i), depth - 1)) {
            return found;
        }
    }
    return nullptr;
}

std::atomic<bool> g_attaching{false};

// Runs on the XAML thread. Idempotent: it stops as soon as our box is in the
// tree, so being called on every shown window costs one failed lookup.
void TryAttachFromWindowRoot() {
    bool expected = false;
    if (!g_attaching.compare_exchange_strong(expected, true)) {
        return;
    }
    struct Guard {
        ~Guard() { g_attaching.store(false); }
    } guard;

    try {
        if (g_ourBox) {
            return;  // already placed
        }
        auto window = wux::Window::Current();
        if (!window) {
            return;  // not a XAML thread; most shown windows are not
        }
        auto content = window.Content();
        if (!content) {
            return;
        }
        auto toggle = FindStockSearchToggle(content);
        if (!toggle) {
            return;  // tree not built yet; the next shown window retries
        }
        g_xamlThreadId.store(GetCurrentThreadId());
        Rec(L"attach: found SearchBoxToggleButton (%.0fx%.0f) without "
            L"diagnostics, on thread %lu",
            toggle.ActualWidth(), toggle.ActualHeight(), GetCurrentThreadId());
        PlaceOurSearchBox(toggle);
    } catch (...) {
        Rec(L"attach: threw %08X", static_cast<unsigned>(winrt::to_hresult()));
    }
}

HWINEVENTHOOK g_attachWatch = nullptr;

void CALLBACK AttachWatchProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                              LONG idObject, LONG idChild, DWORD, DWORD) {
    if ((event != EVENT_OBJECT_SHOW && event != EVENT_OBJECT_UNCLOAKED) ||
        !hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF) {
        return;
    }
    // In-context, so this is the thread that raised the event. The filter is
    // Window::Current() returning something rather than a class name, because
    // a name that changes across builds breaks quietly.
    TryAttachFromWindowRoot();
}

void StartAttachWatch() {
    if (g_attachWatch) {
        return;
    }
    g_attachWatch = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_UNCLOAKED,
                                    GetCurrentModuleHandle(), AttachWatchProc,
                                    GetCurrentProcessId(), 0,
                                    WINEVENT_INCONTEXT);
    Rec(L"attach watch %ls (tapless, no diagnostics slot taken)",
        g_attachWatch ? L"installed" : L"FAILED");
}

void StopAttachWatch() {
    if (g_attachWatch) {
        UnhookWinEvent(g_attachWatch);
        g_attachWatch = nullptr;
    }
}

void SubclassStartMenuWindow() {
    HWND ours = nullptr;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != GetCurrentProcessId()) return TRUE;
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
            if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
                *reinterpret_cast<HWND*>(param) = hwnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ours));
    if (ours) {
        g_hCoreWindow = ours;
        SetPropW(ours, L"WindhawkStartMenuWindow", (HANDLE)1);
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (tray) {
            SetPropW(tray, L"WindhawkStartMenuHwnd", ours);
        }
        if (!g_subclassed) {
            SetWindowSubclass(ours, StartMenuSubclassProc, 101, 0);
            g_subclassed = true;
            Rec(L"subclass: hooked CoreWindow HWND %p", ours);
        }
    }
    InstallMessageHook();
}

// Results, handed from the search thread to the XAML thread.
[[clang::no_destroy]] std::mutex g_resultsMutex;
[[clang::no_destroy]] std::vector<everything::Result> g_results;
std::atomic<DWORD> g_totalMatches{0};

[[clang::no_destroy]] std::thread g_searchThread;
[[clang::no_destroy]] std::mutex g_queryMutex;
[[clang::no_destroy]] std::condition_variable g_queryWake;
[[clang::no_destroy]] std::wstring g_pendingQuery;
std::atomic<bool> g_searchQuit{false};
std::atomic<bool> g_queryDirty{false};

// A row as the XAML thread needs it: text, an optional icon as raw BGRA, and
// what to do when it is clicked.
//
// Icons travel as pixels rather than as a path to fetch later, because the
// fetch is the slow part and it has already happened on the search thread.
struct Row {
    std::wstring title;
    std::wstring subtitle;
    std::wstring openPath;   // files: what ShellExecute opens
    int appIndex = -1;       // apps: which entry of the index to launch
    std::vector<BYTE> icon;  // BGRA, kIconSize square, or empty
    bool canRunAsAdmin = true;
    std::wstring copyText;   // text to copy to clipboard on activation
    std::wstring customGlyph; // Segoe Fluent glyph override (e.g. \uE1D0, \uE701, \uE88E)
};

// Fetched at 48, drawn at 24.
//
// These are two different things and conflating them is what made the first
// attempt look blurry. The draw size is in logical pixels, so on a display at
// 150% a 24-logical icon is 36 real pixels -- a 24px bitmap has to be
// stretched to fill it. Asking the shell for 48 and letting XAML scale down
// stays sharp to 200%, and costs nothing extra: the shell has these sizes
// already.
inline constexpr int kIconSize = 48;     // what we ask the shell for
inline constexpr int kIconDisplay = 24;  // what it occupies in the row

[[clang::no_destroy]] std::vector<Row> g_appRows;
[[clang::no_destroy]] std::vector<Row> g_fileRows;

// Launch requests, posted from the XAML thread back to the search thread,
// which owns the app index and therefore the PIDLs.
//
// A PIDL cannot simply be captured in a click handler: the index is rebuilt
// and the pointer would dangle. Sending an index back to the owning thread
// keeps the lifetime where the data lives.
[[clang::no_destroy]] std::mutex g_launchMutex;
std::atomic<int> g_launchRequest{-1};
std::atomic<bool> g_launchAsAdmin{false};

// ---------------------------------------------------------------------------
// The results list
//
// Sits over the pinned apps, in the same Grid the search row lives in, and is
// shown only while there is a query. Hiding it again restores the menu
// exactly, because nothing about the menu's own children is touched -- the
// list is simply collapsed.
// ---------------------------------------------------------------------------

[[clang::no_destroy]] wuxc::TextBlock g_resultsHeader{nullptr};

// Opens what was clicked.
struct LaunchTask {
    std::thread thread;
    std::atomic<bool> done{false};
};

static std::mutex g_launchTasksMutex;
static std::vector<std::shared_ptr<LaunchTask>> g_launchTasks;

template <typename F>
static void SpawnTrackedLaunch(F&& f) {
    std::lock_guard<std::mutex> lock(g_launchTasksMutex);
    g_launchTasks.erase(
        std::remove_if(g_launchTasks.begin(), g_launchTasks.end(),
            [](const auto& task) {
                if (task->done.load()) {
                    if (task->thread.joinable()) {
                        task->thread.join();
                    }
                    return true;
                }
                return false;
            }),
        g_launchTasks.end()
    );

    auto task = std::make_shared<LaunchTask>();
    task->thread = std::thread([task, fn = std::forward<F>(f)]() mutable {
        try {
            fn();
        } catch (...) {}
        task->done.store(true);
    });
    g_launchTasks.push_back(task);
}

static void WaitForTrackedLaunches() {
    std::vector<std::shared_ptr<LaunchTask>> tasksToJoin;
    {
        std::lock_guard<std::mutex> lock(g_launchTasksMutex);
        tasksToJoin = std::move(g_launchTasks);
    }
    for (auto& task : tasksToJoin) {
        if (task && task->thread.joinable()) {
            task->thread.join();
        }
    }
}

// Opens what was clicked.
//
// ShellExecute rather than CreateProcess: these are paths of any kind, and
// the shell decides what opening one means. This process runs at the user's
// own integrity, so what opens is what the user would have opened -- which is
// exactly what the broker could not promise when it ran elevated.
void OpenResult(std::wstring path, bool asAdmin = false) {
    SpawnTrackedLaunch([path = std::move(path), asAdmin] {
        wchar_t userProfile[MAX_PATH] = {};
        if (!GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) || !userProfile[0]) {
            PWSTR kf = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &kf)) && kf) {
                wcsncpy_s(userProfile, kf, MAX_PATH - 1);
                CoTaskMemFree(kf);
            }
        }

        std::wstring lowerPath = path;
        for (auto& c : lowerPath) c = static_cast<wchar_t>(towlower(c));
        bool isTerminalOrShell = (lowerPath.find(L"cmd.exe") != std::wstring::npos ||
                                  lowerPath.find(L"powershell") != std::wstring::npos ||
                                  lowerPath.find(L"pwsh") != std::wstring::npos ||
                                  lowerPath.find(L"windowsterminal") != std::wstring::npos ||
                                  lowerPath.ends_with(L"wt.exe"));

        size_t lastSlash = path.find_last_of(L"\\/");
        std::wstring parentDir = (lastSlash != std::wstring::npos) ? path.substr(0, lastSlash) : L"";
        LPCWSTR workDir = isTerminalOrShell ? userProfile : (!parentDir.empty() ? parentDir.c_str() : userProfile);

        std::wstring params;
        if (lowerPath.ends_with(L"cmd.exe")) {
            params = L"/k cd /d \"" + std::wstring(userProfile) + L"\"";
        } else if (lowerPath.find(L"powershell.exe") != std::wstring::npos || lowerPath.ends_with(L"pwsh.exe")) {
            params = L"-NoExit -Command \"Set-Location '" + std::wstring(userProfile) + L"'\"";
        }

        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOASYNC | (asAdmin ? 0 : SEE_MASK_FLAG_NO_UI);
        info.lpVerb = asAdmin ? L"runas" : L"open";
        info.lpFile = path.c_str();
        if (!params.empty()) {
            info.lpParameters = params.c_str();
        }
        info.lpDirectory = workDir;
        info.nShow = SW_SHOWNORMAL;
        if (!ShellExecuteExW(&info)) {
            Rec(L"open failed (%lu): %ls (admin=%d)", GetLastError(), path.c_str(), asAdmin ? 1 : 0);
        }
    });
}

void OpenFileLocation(std::wstring path) {
    SpawnTrackedLaunch([path = std::move(path)] {
        std::wstring args = L"/select,\"" + path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    });
}

inline bool CopyTextToClipboard(const std::wstring& text) {
    if (text.empty()) return false;
    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hGlobal) {
        void* ptr = GlobalLock(hGlobal);
        if (ptr) {
            memcpy(ptr, text.c_str(), bytes);
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_UNICODETEXT, hGlobal);
        }
    }
    CloseClipboard();
    return true;
}

inline std::wstring UrlEncode(const std::wstring& str) {
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Len <= 0) return L"";
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), -1, &utf8[0], utf8Len, nullptr, nullptr);

    std::string encoded;
    const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < utf8.size() - 1; ++i) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += static_cast<char>(c);
        } else if (c == ' ') {
            encoded += '+';
        } else {
            encoded += '%';
            encoded += hex[(c >> 4) & 0x0F];
            encoded += hex[c & 0x0F];
        }
    }
    return std::wstring(encoded.begin(), encoded.end());
}

struct ResolvedWebQuery {
    std::wstring serviceName;
    std::wstring queryTerm;
    std::wstring searchUrl;
    std::wstring homeUrl;
    bool isShortcut = false;
};

inline std::wstring DeriveHomeUrl(const std::wstring& urlTemplate) {
    size_t qMark = urlTemplate.find(L"?");
    if (qMark != std::wstring::npos) {
        size_t slash = urlTemplate.find(L"/", 8); // after https://
        if (slash != std::wstring::npos && slash < qMark) {
            return urlTemplate.substr(0, slash + 1);
        }
        return urlTemplate.substr(0, qMark);
    }
    return urlTemplate;
}

inline std::wstring SubstituteQuery(const std::wstring& urlTemplate, const std::wstring& query) {
    if (query.empty()) return DeriveHomeUrl(urlTemplate);
    std::wstring encoded = UrlEncode(query);
    std::wstring res = urlTemplate;
    size_t pos = res.find(L"{q}");
    if (pos != std::wstring::npos) {
        res.replace(pos, 3, encoded);
        return res;
    }
    pos = res.find(L"{searchTerms}");
    if (pos != std::wstring::npos) {
        res.replace(pos, 13, encoded);
        return res;
    }
    if (res.find(L"?") == std::wstring::npos) {
        res += L"?q=" + encoded;
    } else {
        res += L"&q=" + encoded;
    }
    return res;
}

inline std::wstring DeriveEngineName(const std::wstring& url) {
    std::wstring lower = url;
    for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
    if (lower.find(L"duckduckgo") != std::wstring::npos) return L"DuckDuckGo";
    if (lower.find(L"google") != std::wstring::npos) return L"Google";
    if (lower.find(L"bing") != std::wstring::npos) return L"Bing";
    if (lower.find(L"brave") != std::wstring::npos) return L"Brave";
    if (lower.find(L"yahoo") != std::wstring::npos) return L"Yahoo";
    if (lower.find(L"ecosia") != std::wstring::npos) return L"Ecosia";
    if (lower.find(L"kagi") != std::wstring::npos) return L"Kagi";
    if (lower.find(L"startpage") != std::wstring::npos) return L"Startpage";
    return L"Web";
}

inline ResolvedWebQuery ResolveWebSearch(const std::wstring& input) {
    ResolvedWebQuery res;
    std::wstring text = input;
    while (!text.empty() && text.front() == L' ') text.erase(0, 1);

    std::vector<WebShortcut> shortcuts;
    std::wstring defUrl;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        shortcuts = g_settings.webShortcuts;
        defUrl = g_settings.defaultSearchUrl;
    }
    if (defUrl.empty()) defUrl = L"https://duckduckgo.com/?q={q}";

    // Check if first token matches any shortcut
    size_t delim = text.find_first_of(L" :");
    std::wstring firstWord = (delim != std::wstring::npos) ? text.substr(0, delim) : text;
    std::wstring firstWordLower = firstWord;
    for (auto& c : firstWordLower) c = static_cast<wchar_t>(towlower(c));

    for (const auto& sc : shortcuts) {
        if (!firstWordLower.empty() && firstWordLower == sc.prefix) {
            res.isShortcut = true;
            res.serviceName = sc.name;
            std::wstring remainder = (delim != std::wstring::npos) ? text.substr(delim + 1) : L"";
            while (!remainder.empty() && remainder.front() == L' ') remainder.erase(0, 1);
            res.queryTerm = remainder;
            res.homeUrl = DeriveHomeUrl(sc.url);
            res.searchUrl = SubstituteQuery(sc.url, res.queryTerm);
            return res;
        }
    }

    // Default search engine (DuckDuckGo or user configured in settings)
    res.isShortcut = false;
    res.serviceName = DeriveEngineName(defUrl);
    res.queryTerm = text;
    res.homeUrl = DeriveHomeUrl(defUrl);
    res.searchUrl = SubstituteQuery(defUrl, text);
    return res;
}

inline wuxm::SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return wuxm::SolidColorBrush{winrt::Windows::UI::ColorHelper::FromArgb(a, r, g, b)};
}

[[clang::no_destroy]] wuxc::TextBlock g_footerStatus{nullptr};
[[clang::no_destroy]] wuxc::StackPanel g_footerHints{nullptr};

void QueueQuery(std::wstring text);

[[clang::no_destroy]] std::vector<Row> g_currentAppRows;
[[clang::no_destroy]] std::vector<Row> g_currentFileRows;
static int g_selectedApp = -1;
static uint64_t g_lastNavTick = 0;

void SetAppSelection(int index) {
    if (g_appButtons.empty()) {
        g_selectedApp = -1;
        return;
    }
    if (index < 0) index = 0;
    if (index >= static_cast<int>(g_appButtons.size())) {
        index = static_cast<int>(g_appButtons.size()) - 1;
    }
    g_selectedApp = index;

    for (size_t i = 0; i < g_appButtons.size(); ++i) {
        auto& btn = g_appButtons[i];
        if (!btn) continue;

        if (static_cast<int>(i) == index) {
            btn.Background(MakeBrush(0x28, 0xFF, 0xFF, 0xFF));
            btn.BorderBrush(MakeBrush(0x55, 0x60, 0xCD, 0xFF));
            try {
                btn.StartBringIntoView();
            } catch (...) {}
        } else {
            btn.Background(MakeBrush(0, 0, 0, 0));
            btn.BorderBrush(MakeBrush(0, 0, 0, 0));
        }
    }
}

void LaunchSelectedApp(int index, bool asAdmin) {
    if (index < 0 || index >= static_cast<int>(g_currentAppRows.size())) {
        return;
    }
    const auto& row = g_currentAppRows[index];
    if (!row.copyText.empty()) {
        DismissStartMenu();
        tools::CopyTextToClipboard(row.copyText);
        Rec(L"copied to clipboard: '%ls'", row.copyText.c_str());
        return;
    }
    if (row.openPath.starts_with(L"http:") || row.openPath.starts_with(L"https:")) {
        DismissStartMenu();
        OpenResult(row.openPath, false);
        Rec(L"launch web: '%ls'", row.openPath.c_str());
        return;
    }
    int which = row.appIndex;
    if (which >= 0) {
        DismissStartMenu();
        if (asAdmin && !row.canRunAsAdmin) {
            asAdmin = false;
        }
        g_launchAsAdmin.store(asAdmin);
        g_launchRequest.store(which);
        g_queryWake.notify_all();
        Rec(L"launch app: index %d ('%ls'), asAdmin=%d", index, row.title.c_str(), asAdmin ? 1 : 0);
    }
}

void HandleNavigationKey(winrt::Windows::System::VirtualKey key, bool ctrl) {
    uint64_t now = GetTickCount64();
    if (now - g_lastNavTick < 60) return;
    g_lastNavTick = now;

    if (key == winrt::Windows::System::VirtualKey::Down) {
        if (!g_appButtons.empty()) {
            int next = (g_selectedApp < 0) ? 0 : g_selectedApp + 1;
            if (next >= static_cast<int>(g_appButtons.size())) {
                next = static_cast<int>(g_appButtons.size()) - 1;
            }
            SetAppSelection(next);
        }
    } else if (key == winrt::Windows::System::VirtualKey::Up) {
        if (!g_appButtons.empty()) {
            int prev = (g_selectedApp <= 0) ? 0 : g_selectedApp - 1;
            SetAppSelection(prev);
        }
    } else if (key == winrt::Windows::System::VirtualKey::Enter) {
        if (!g_currentAppRows.empty()) {
            int target = (g_selectedApp >= 0) ? g_selectedApp : 0;
            LaunchSelectedApp(target, ctrl);
        }
    }
}

// Results palette:
// Search box at top (Row 0),
// Dual-column apps and files in middle (Row 1),
// and keyboard navigation footer at bottom (Row 2).
void BuildResultsList(wuxc::Panel const& ownerPanel) try {
    if (g_resultsHost) {
        return;
    }

    wuxc::Grid root;
    root.Name(L"WindhawkEverythingResults");
    root.Margin(wux::ThicknessHelper::FromLengths(14, 14, 14, 10));
    root.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
    root.VerticalAlignment(wux::VerticalAlignment::Stretch);
    root.Visibility(wux::Visibility::Visible);
    root.Opacity(0.0);
    root.IsHitTestVisible(false);
    root.Padding(wux::ThicknessHelper::FromLengths(16, 12, 16, 8));
    root.CornerRadius(wux::CornerRadius{8, 8, 8, 8});

    wuxm::TranslateTransform tt;
    tt.Y(-8.0);
    root.RenderTransform(tt);
    g_resultsTranslate = tt;

    // Spanning every row and column of MainContent
    wuxc::Grid::SetRow(root, 0);
    wuxc::Grid::SetRowSpan(root, 12);
    wuxc::Grid::SetColumn(root, 0);
    wuxc::Grid::SetColumnSpan(root, 12);
    wuxc::Canvas::SetZIndex(root, 999);

    // Root layout:
    // Row 0 (Auto): Search bar inside the overlay
    // Row 1 (1*): Dual-column Results (Apps 40* / hairline divider / Files 60*)
    // Row 2 (Auto): Status Footer
    wuxc::RowDefinition searchRowDef, resultsRowDef, footerRowDef;
    searchRowDef.Height(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    resultsRowDef.Height(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
    footerRowDef.Height(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    root.RowDefinitions().Append(searchRowDef);
    root.RowDefinitions().Append(resultsRowDef);
    root.RowDefinitions().Append(footerRowDef);

    // Search bar container at Row 0
    wuxc::Border searchBarBorder;
    searchBarBorder.Margin(wux::ThicknessHelper::FromLengths(0, 0, 0, 8));
    searchBarBorder.Padding(wux::ThicknessHelper::FromLengths(12, 0, 10, 0));
    searchBarBorder.CornerRadius(wux::CornerRadius{6, 6, 6, 6});
    searchBarBorder.Background(MakeBrush(0x18, 0xFF, 0xFF, 0xFF));
    searchBarBorder.BorderBrush(MakeBrush(0x28, 0xFF, 0xFF, 0xFF));
    searchBarBorder.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
    searchBarBorder.Height(40);
    wuxc::Grid::SetRow(searchBarBorder, 0);

    wuxc::Grid searchBarGrid;
    wuxc::ColumnDefinition sbIconCol, sbBoxCol;
    sbIconCol.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    sbBoxCol.Width(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
    searchBarGrid.ColumnDefinitions().Append(sbIconCol);
    searchBarGrid.ColumnDefinitions().Append(sbBoxCol);

    wuxc::FontIcon searchIcon;
    searchIcon.Glyph(L"\uE721");
    searchIcon.FontSize(14);
    searchIcon.Opacity(0.65);
    searchIcon.Margin(wux::ThicknessHelper::FromLengths(0, 0, 10, 0));
    searchIcon.VerticalAlignment(wux::VerticalAlignment::Center);
    wuxc::Grid::SetColumn(searchIcon, 0);
    searchBarGrid.Children().Append(searchIcon);

    wuxc::TextBox box;
    box.Name(L"WindhawkStartSearchBox");
    box.PlaceholderText(L"Search apps, settings, and files...");
    box.VerticalAlignment(wux::VerticalAlignment::Center);
    box.VerticalContentAlignment(wux::VerticalAlignment::Center);
    box.FontSize(14);
    box.Background(MakeBrush(0, 0, 0, 0));
    box.BorderThickness(wux::ThicknessHelper::FromUniformLength(0));
    box.IsTabStop(true);
    box.TabIndex(0);

    static const wchar_t* kClearKeys[] = {
        L"TextControlBackground",
        L"TextControlBackgroundPointerOver",
        L"TextControlBackgroundFocused",
        L"TextControlBackgroundDisabled",
        L"TextControlBorderBrush",
        L"TextControlBorderBrushPointerOver",
        L"TextControlBorderBrushFocused",
        L"TextControlBorderBrushDisabled",
        L"TextControlButtonBackground",
        L"TextControlButtonBackgroundPointerOver",
        L"TextControlButtonBackgroundPressed",
    };
    for (const wchar_t* key : kClearKeys) {
        box.Resources().Insert(winrt::box_value(winrt::hstring{key}),
                               wuxm::SolidColorBrush{winrt::Windows::UI::Colors::Transparent()});
    }

    g_ourBoxChanged = box.TextChanged(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxc::TextChangedEventArgs const&) {
            try {
                auto b = sender.as<wuxc::TextBox>();
                std::wstring text{b.Text()};
                Rec(L"own box: '%ls'", text.c_str());
                if (text.empty()) {
                    HideOverlayAnimated();
                } else {
                    RevealOverlayAnimated();
                }
                QueueQuery(std::move(text));
            } catch (...) {}
        });

    box.PreviewKeyDown([](wf::IInspectable const&, wux::Input::KeyRoutedEventArgs const& args) {
        try {
            auto key = args.Key();
            if (key == winrt::Windows::System::VirtualKey::Escape) {
                if (g_isOverlayVisible.load() || g_isHiding.load()) {
                    if (g_ourBox) g_ourBox.Text(L"");
                    HideOverlayAnimated();
                    args.Handled(true);
                    return;
                }
            }
            if (key == winrt::Windows::System::VirtualKey::Down ||
                key == winrt::Windows::System::VirtualKey::Up ||
                key == winrt::Windows::System::VirtualKey::Enter) {
                if (g_isOverlayVisible.load()) {
                    bool ctrl = (GetKeyState(VK_CONTROL) < 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
                    HandleNavigationKey(key, ctrl);
                    args.Handled(true);
                }
            }
        } catch (...) {}
    });

    g_ourBoxLost = box.LostFocus(
        winrt::auto_revoke,
        [](wf::IInspectable const&, wux::RoutedEventArgs const&) {
            try {
                if (g_ourBox && g_ourBox.Text().empty() && (g_isOverlayVisible.load() || g_isHiding.load())) {
                    HideOverlayAnimated();
                }
            } catch (...) {}
        });

    wuxc::Grid::SetColumn(box, 1);
    searchBarGrid.Children().Append(box);
    g_ourBox = box;

    searchBarBorder.Child(searchBarGrid);
    root.Children().Append(searchBarBorder);

    // Results container grid: Apps column (40*), 1px hairline divider, Files column (60*)
    wuxc::Grid resultsGrid;
    wuxc::Grid::SetRow(resultsGrid, 1);

    wuxc::ColumnDefinition appsCol, divCol, filesCol;
    appsCol.Width(wux::GridLengthHelper::FromValueAndType(40, wux::GridUnitType::Star));
    divCol.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    filesCol.Width(wux::GridLengthHelper::FromValueAndType(60, wux::GridUnitType::Star));
    resultsGrid.ColumnDefinitions().Append(appsCol);
    resultsGrid.ColumnDefinitions().Append(divCol);
    resultsGrid.ColumnDefinitions().Append(filesCol);

    wuxc::StackPanel apps;

    wuxc::Grid appsColGrid;
    wuxc::RowDefinition aHeadRow, aListRow;
    aHeadRow.Height(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    aListRow.Height(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
    appsColGrid.RowDefinitions().Append(aHeadRow);
    appsColGrid.RowDefinitions().Append(aListRow);
    wuxc::Grid::SetColumn(appsColGrid, 0);

    wuxc::Border appsHeaderHolder;
    wuxc::Grid::SetRow(appsHeaderHolder, 0);
    appsColGrid.Children().Append(appsHeaderHolder);
    g_appsHeaderHolder = appsHeaderHolder;

    wuxc::ScrollViewer appsScroll;
    appsScroll.Content(apps);
    appsScroll.IsTabStop(false);
    appsScroll.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);
    appsScroll.HorizontalScrollBarVisibility(wuxc::ScrollBarVisibility::Disabled);
    wuxc::Grid::SetRow(appsScroll, 1);
    appsColGrid.Children().Append(appsScroll);

    resultsGrid.Children().Append(appsColGrid);

    wuxc::Border divider;
    divider.Width(1);
    divider.HorizontalAlignment(wux::HorizontalAlignment::Center);
    divider.VerticalAlignment(wux::VerticalAlignment::Stretch);
    divider.Background(MakeBrush(0x14, 0xFF, 0xFF, 0xFF));
    divider.Margin(wux::ThicknessHelper::FromLengths(6, 4, 6, 4));
    wuxc::Grid::SetColumn(divider, 1);
    resultsGrid.Children().Append(divider);

    wuxc::StackPanel files;
    wuxc::Grid filesColGrid;
    wuxc::RowDefinition fHeadRow, fListRow;
    fHeadRow.Height(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    fListRow.Height(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
    filesColGrid.RowDefinitions().Append(fHeadRow);
    filesColGrid.RowDefinitions().Append(fListRow);
    wuxc::Grid::SetColumn(filesColGrid, 2);

    wuxc::Border filesHeaderHolder;
    wuxc::Grid::SetRow(filesHeaderHolder, 0);
    filesColGrid.Children().Append(filesHeaderHolder);
    g_filesHeaderHolder = filesHeaderHolder;

    wuxc::ScrollViewer filesScroll;
    filesScroll.Content(files);
    filesScroll.IsTabStop(false);
    filesScroll.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);
    filesScroll.HorizontalScrollBarVisibility(wuxc::ScrollBarVisibility::Disabled);
    wuxc::Grid::SetRow(filesScroll, 1);
    filesColGrid.Children().Append(filesScroll);

    resultsGrid.Children().Append(filesColGrid);

    root.Children().Append(resultsGrid);

    // Modern Raycast-style bottom status and shortcut bar
    wuxc::Border footerBorder;
    footerBorder.Margin(wux::ThicknessHelper::FromLengths(0, 6, 0, 0));
    footerBorder.Padding(wux::ThicknessHelper::FromLengths(8, 6, 8, 4));
    footerBorder.BorderBrush(MakeBrush(0x12, 0xFF, 0xFF, 0xFF));
    footerBorder.BorderThickness(wux::ThicknessHelper::FromLengths(0, 1, 0, 0));
    wuxc::Grid::SetRow(footerBorder, 2);

    wuxc::Grid footerGrid;
    wuxc::ColumnDefinition fColLeft, fColRight;
    fColLeft.Width(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
    fColRight.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
    footerGrid.ColumnDefinitions().Append(fColLeft);
    footerGrid.ColumnDefinitions().Append(fColRight);

    wuxc::StackPanel leftStatus;
    leftStatus.Orientation(wuxc::Orientation::Horizontal);
    leftStatus.VerticalAlignment(wux::VerticalAlignment::Center);

    wuxc::FontIcon boltIcon;
    boltIcon.Glyph(L"\uE946");
    boltIcon.FontSize(11);
    boltIcon.Opacity(0.5);
    boltIcon.Margin(wux::ThicknessHelper::FromLengths(0, 0, 6, 0));
    boltIcon.VerticalAlignment(wux::VerticalAlignment::Center);
    leftStatus.Children().Append(boltIcon);

    wuxc::TextBlock statusText;
    statusText.Text(L"Everything Search");
    statusText.FontSize(11);
    statusText.Opacity(0.5);
    statusText.VerticalAlignment(wux::VerticalAlignment::Center);
    leftStatus.Children().Append(statusText);
    g_footerStatus = statusText;

    wuxc::Grid::SetColumn(leftStatus, 0);
    footerGrid.Children().Append(leftStatus);

    wuxc::StackPanel rightHints;
    rightHints.Orientation(wuxc::Orientation::Horizontal);
    rightHints.VerticalAlignment(wux::VerticalAlignment::Center);

    auto makeKeyCap = [](const wchar_t* key, const wchar_t* action) {
        wuxc::StackPanel pair;
        pair.Orientation(wuxc::Orientation::Horizontal);
        pair.VerticalAlignment(wux::VerticalAlignment::Center);
        pair.Margin(wux::ThicknessHelper::FromLengths(8, 0, 0, 0));

        wuxc::Border keyBadge;
        keyBadge.CornerRadius(wux::CornerRadius{3, 3, 3, 3});
        keyBadge.Background(MakeBrush(0x18, 0xFF, 0xFF, 0xFF));
        keyBadge.BorderBrush(MakeBrush(0x24, 0xFF, 0xFF, 0xFF));
        keyBadge.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
        keyBadge.Padding(wux::ThicknessHelper::FromLengths(4, 1, 4, 1));
        keyBadge.VerticalAlignment(wux::VerticalAlignment::Center);

        wuxc::TextBlock keyBlock;
        keyBlock.Text(winrt::hstring{key});
        keyBlock.FontSize(9.5);
        keyBlock.FontWeight(wut::FontWeights::SemiBold());
        keyBlock.Opacity(0.75);
        keyBadge.Child(keyBlock);
        pair.Children().Append(keyBadge);

        wuxc::TextBlock actionBlock;
        actionBlock.Text(winrt::hstring{action});
        actionBlock.FontSize(10.5);
        actionBlock.Opacity(0.45);
        actionBlock.Margin(wux::ThicknessHelper::FromLengths(4, 0, 0, 0));
        actionBlock.VerticalAlignment(wux::VerticalAlignment::Center);
        pair.Children().Append(actionBlock);

        return pair;
    };

    rightHints.Children().Append(makeKeyCap(L"\u2191\u2193", L"Select"));
    rightHints.Children().Append(makeKeyCap(L"\u21B5", L"Open"));
    rightHints.Children().Append(makeKeyCap(L"Ctrl+\u21B5", L"Admin"));
    rightHints.Children().Append(makeKeyCap(L"Esc", L"Close"));

    wuxc::Grid::SetColumn(rightHints, 1);
    g_footerHints = rightHints;
    bool showHints = true;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        showHints = g_settings.showKeyHints;
    }
    rightHints.Visibility(showHints ? wux::Visibility::Visible : wux::Visibility::Collapsed);
    footerGrid.Children().Append(rightHints);

    footerBorder.Child(footerGrid);
    root.Children().Append(footerBorder);

    // Spanning every row and column of the menu's grid.
    wuxc::Grid::SetRow(root, 0);
    wuxc::Grid::SetRowSpan(root, 12);
    wuxc::Grid::SetColumn(root, 0);
    wuxc::Grid::SetColumnSpan(root, 12);

    ownerPanel.Children().Append(root);
    g_appsList = apps;
    g_resultsList = files;
    g_resultsHost = root;


    SyncOverlayBackground();
    if (!root.Background()) {
        root.Background(MakeBrush(0xF2, 0x20, 0x20, 0x20));
        Rec(L"results: no AcrylicBorder found; flat background instead");
    }

    Rec(L"results: two columns and footer built");
} catch (...) {
    Rec(L"results list failed %08X", static_cast<unsigned>(winrt::to_hresult()));
}

// Runs on the XAML thread.
void RenderResults() try {
    if (!g_resultsList || !g_resultsHost || !g_appsList) {
        return;
    }

    std::vector<Row> files;
    std::vector<Row> appNames;
    DWORD total = 0;
    {
        std::lock_guard<std::mutex> lock(g_resultsMutex);
        files = g_fileRows;
        appNames = g_appRows;
        total = g_totalMatches.load();
    }

    if (files.empty() && appNames.empty()) {
        if (g_ourBox && g_ourBox.Text().empty()) {
            HideOverlayAnimated();
        }
        g_selectedApp = -1;
        Rec(L"render: nothing to show; menu restored");
        return;
    }

    g_resultsList.Children().Clear();
    g_currentAppRows = appNames;
    g_currentFileRows = files;

    // Update bottom status bar
    if (g_footerStatus) {
        if (!appNames.empty() && (appNames[0].customGlyph == L"\uE701" || appNames[0].customGlyph == L"\uE704" || appNames[0].customGlyph == L"\uE839")) {
            std::wstring statusStr = L"Network Interfaces \u2022 " + std::to_wstring(appNames.size()) + L" active \u2022 Press Enter to copy IP";
            g_footerStatus.Text(winrt::hstring{statusStr});
        } else if (!appNames.empty() && appNames[0].customGlyph == L"\uE88E") {
            std::wstring statusStr = L"Unit Converter \u2022 " + std::to_wstring(appNames.size()) + L" conversions \u2022 Press Enter to copy";
            g_footerStatus.Text(winrt::hstring{statusStr});
        } else if (!appNames.empty() && appNames[0].customGlyph == L"\uE1D0") {
            std::wstring statusStr = L"Calculator \u2022 Press Enter to copy result";
            g_footerStatus.Text(winrt::hstring{statusStr});
        } else if (!appNames.empty() && (appNames[0].openPath.starts_with(L"http:") || appNames[0].openPath.starts_with(L"https:"))) {
            std::wstring statusStr = L"Web Search \u2022 Press Enter to search in default browser";
            if (!files.empty()) {
                statusStr += L" (" + std::to_wstring(files.size()) + L" files matched)";
            }
            g_footerStatus.Text(winrt::hstring{statusStr});
        } else {
            std::wstring statusStr = L"Everything \u2022 " + std::to_wstring(total) + L" matches";
            if (!appNames.empty() || !files.empty()) {
                statusStr += L" (" + std::to_wstring(appNames.size()) + L" apps, " + std::to_wstring(files.size()) + L" files shown)";
            }
            g_footerStatus.Text(winrt::hstring{statusStr});
        }
    }

    if (g_footerHints) {
        bool showHints = true;
        {
            std::lock_guard<std::mutex> lock(g_settingsMutex);
            showHints = g_settings.showKeyHints;
        }
        g_footerHints.Visibility(showHints ? wux::Visibility::Visible : wux::Visibility::Collapsed);
    }

    auto makeHeader = [](const std::wstring& title, const wchar_t* iconGlyph, int count, const std::wstring& badgeText = L"") {
        wuxc::Grid headerGrid;
        headerGrid.Margin(wux::ThicknessHelper::FromLengths(8, 4, 8, 6));

        wuxc::ColumnDefinition leftCol, rightCol;
        leftCol.Width(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
        rightCol.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
        headerGrid.ColumnDefinitions().Append(leftCol);
        headerGrid.ColumnDefinitions().Append(rightCol);

        wuxc::StackPanel leftStack;
        leftStack.Orientation(wuxc::Orientation::Horizontal);
        leftStack.VerticalAlignment(wux::VerticalAlignment::Center);

        wuxc::FontIcon icon;
        icon.Glyph(winrt::hstring{iconGlyph});
        icon.FontSize(11.5);
        icon.Opacity(0.6);
        icon.Margin(wux::ThicknessHelper::FromLengths(0, 0, 6, 0));
        icon.VerticalAlignment(wux::VerticalAlignment::Center);
        leftStack.Children().Append(icon);

        wuxc::TextBlock titleBlock;
        titleBlock.Text(winrt::hstring{title});
        titleBlock.FontSize(11);
        titleBlock.FontWeight(wut::FontWeights::SemiBold());
        titleBlock.Opacity(0.65);
        titleBlock.CharacterSpacing(40);
        titleBlock.VerticalAlignment(wux::VerticalAlignment::Center);
        leftStack.Children().Append(titleBlock);

        wuxc::Grid::SetColumn(leftStack, 0);
        headerGrid.Children().Append(leftStack);

        std::wstring badgeStr = badgeText.empty() ? std::to_wstring(count) : badgeText;
        if (!badgeStr.empty() && count >= 0) {
            wuxc::Border badge;
            badge.CornerRadius(wux::CornerRadius{4, 4, 4, 4});
            badge.Background(MakeBrush(0x15, 0xFF, 0xFF, 0xFF));
            badge.BorderBrush(MakeBrush(0x20, 0xFF, 0xFF, 0xFF));
            badge.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
            badge.Padding(wux::ThicknessHelper::FromLengths(7, 1, 7, 1));
            badge.VerticalAlignment(wux::VerticalAlignment::Center);

            wuxc::TextBlock badgeBlock;
            badgeBlock.Text(winrt::hstring{badgeStr});
            badgeBlock.FontSize(9.5);
            badgeBlock.FontWeight(wut::FontWeights::SemiBold());
            badgeBlock.Opacity(0.7);
            badge.Child(badgeBlock);

            wuxc::Grid::SetColumn(badge, 1);
            headerGrid.Children().Append(badge);
        }

        return headerGrid;
    };

    bool isWebMode = (!appNames.empty() && (appNames[0].openPath.starts_with(L"http:") || appNames[0].openPath.starts_with(L"https:")));
    if (g_appsHeaderHolder) {
        if (!appNames.empty() && (appNames[0].customGlyph == L"\uE701" || appNames[0].customGlyph == L"\uE704" || appNames[0].customGlyph == L"\uE839")) {
            g_appsHeaderHolder.Child(makeHeader(L"NETWORK INTERFACES", L"\uE701", static_cast<int>(appNames.size())));
        } else if (!appNames.empty() && appNames[0].customGlyph == L"\uE88E") {
            g_appsHeaderHolder.Child(makeHeader(L"UNIT CONVERTER", L"\uE88E", static_cast<int>(appNames.size())));
        } else if (!appNames.empty() && appNames[0].customGlyph == L"\uE1D0") {
            g_appsHeaderHolder.Child(makeHeader(L"CALCULATOR", L"\uE1D0", static_cast<int>(appNames.size())));
        } else if (isWebMode) {
            g_appsHeaderHolder.Child(makeHeader(L"WEB SEARCH", L"\uE774", static_cast<int>(appNames.size())));
        } else {
            g_appsHeaderHolder.Child(makeHeader(L"APPS", L"\uE71D", static_cast<int>(appNames.size())));
        }
    }
    std::wstring filesBadge = std::to_wstring(files.size()) + L" of " + std::to_wstring(total);
    if (g_filesHeaderHolder) {
        g_filesHeaderHolder.Child(makeHeader(L"FILES", L"\uE8B7", static_cast<int>(files.size()), filesBadge));
    }

    auto makeAppCard = [](const Row& item) -> AppCardUI {
        wuxc::Grid layout;
        wuxc::ColumnDefinition iconCol, textCol;
        iconCol.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
        textCol.Width(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
        layout.ColumnDefinitions().Append(iconCol);
        layout.ColumnDefinitions().Append(textCol);

        bool hasBitmap = false;
        if (item.icon.size() == static_cast<size_t>(kIconSize) * kIconSize * 4) {
            wuxmi::WriteableBitmap bmp{kIconSize, kIconSize};
            auto buffer = bmp.PixelBuffer();
            auto access = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            BYTE* dest = nullptr;
            if (SUCCEEDED(access->Buffer(&dest)) && dest) {
                memcpy(dest, item.icon.data(), item.icon.size());
                bmp.Invalidate();
                wuxc::Image image;
                image.Source(bmp);
                image.Width(kIconDisplay);
                image.Height(kIconDisplay);
                image.Margin(wux::ThicknessHelper::FromLengths(0, 0, 10, 0));
                image.VerticalAlignment(wux::VerticalAlignment::Center);
                wuxc::Grid::SetColumn(image, 0);
                layout.Children().Append(image);
                hasBitmap = true;
            }
        }

        if (!hasBitmap) {
            wuxc::Border iconBox;
            iconBox.Width(kIconDisplay);
            iconBox.Height(kIconDisplay);
            iconBox.Margin(wux::ThicknessHelper::FromLengths(0, 0, 10, 0));
            iconBox.VerticalAlignment(wux::VerticalAlignment::Center);

            wuxc::FontIcon fallbackIcon;
            fallbackIcon.FontSize(15);
            fallbackIcon.Opacity(0.5);
            fallbackIcon.HorizontalAlignment(wux::HorizontalAlignment::Center);
            fallbackIcon.VerticalAlignment(wux::VerticalAlignment::Center);

            if (item.openPath.starts_with(L"ms-settings:") || item.subtitle.starts_with(L"Settings")) {
                fallbackIcon.Glyph(L"\uE713");
                fallbackIcon.Opacity(0.7);
            } else if (item.openPath.starts_with(L"http:") || item.openPath.starts_with(L"https:")) {
                fallbackIcon.Glyph(L"\uE774");
                fallbackIcon.Opacity(0.85);
            } else if (!item.customGlyph.empty()) {
                fallbackIcon.Glyph(winrt::hstring{item.customGlyph});
                fallbackIcon.Opacity(0.85);
            } else {
                fallbackIcon.Glyph(L"\uE71D");
            }
            iconBox.Child(fallbackIcon);
            wuxc::Grid::SetColumn(iconBox, 0);
            layout.Children().Append(iconBox);
        }

        wuxc::StackPanel text;
        wuxc::Grid::SetColumn(text, 1);
        text.VerticalAlignment(wux::VerticalAlignment::Center);

        wuxc::TextBlock name;
        name.Text(winrt::hstring{item.title});
        name.FontSize(12.5);
        name.FontWeight(wut::FontWeights::SemiBold());
        name.TextTrimming(wux::TextTrimming::CharacterEllipsis);
        name.TextWrapping(wux::TextWrapping::NoWrap);
        text.Children().Append(name);

        if (!item.subtitle.empty()) {
            wuxc::TextBlock sub;
            sub.Text(winrt::hstring{item.subtitle});
            sub.Opacity(0.45);
            sub.FontSize(10.5);
            sub.Margin(wux::ThicknessHelper::FromLengths(0, 1, 0, 0));
            sub.TextTrimming(wux::TextTrimming::CharacterEllipsis);
            sub.TextWrapping(wux::TextWrapping::NoWrap);
            text.Children().Append(sub);
        }
        layout.Children().Append(text);

        wuxc::Button button;
        button.Content(layout);
        button.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
        button.HorizontalContentAlignment(wux::HorizontalAlignment::Stretch);
        button.Background(MakeBrush(0, 0, 0, 0));
        button.BorderBrush(MakeBrush(0, 0, 0, 0));
        button.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
        button.CornerRadius(wux::CornerRadius{6, 6, 6, 6});
        button.Padding(wux::ThicknessHelper::FromLengths(10, 6, 10, 6));
        button.Margin(wux::ThicknessHelper::FromLengths(2, 1, 2, 1));

        button.PointerEntered([btn = button](wf::IInspectable const&, wux::Input::PointerRoutedEventArgs const&) {
            for (size_t i = 0; i < g_activeApps.size(); ++i) {
                if (g_activeApps[i].button == btn) {
                    SetAppSelection(static_cast<int>(i));
                    break;
                }
            }
        });

        button.Click([btn = button](wf::IInspectable const&, wux::RoutedEventArgs const&) {
            for (size_t i = 0; i < g_activeApps.size(); ++i) {
                if (g_activeApps[i].button == btn) {
                    LaunchSelectedApp(static_cast<int>(i), false);
                    return;
                }
            }
        });

        wuxc::MenuFlyout flyout;
        if (!item.copyText.empty()) {
            wuxc::MenuFlyoutItem copyItem;
            copyItem.Text(L"Copy to clipboard");
            wuxc::FontIcon copyIcon;
            copyIcon.Glyph(L"\uE8C8");
            copyItem.Icon(copyIcon);
            copyItem.Click([txt = item.copyText](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyTextToClipboard(txt);
            });
            flyout.Items().Append(copyItem);
        } else {
            bool isWebItem = item.openPath.starts_with(L"http:") || item.openPath.starts_with(L"https:");
            wuxc::MenuFlyoutItem openItem;
            openItem.Text(isWebItem ? L"Search in browser" : L"Open");
            wuxc::FontIcon openIcon;
            openIcon.Glyph(isWebItem ? L"\uE774" : L"\uE8A7");
            openItem.Icon(openIcon);
            openItem.Click([btn = button](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                for (size_t i = 0; i < g_activeApps.size(); ++i) {
                    if (g_activeApps[i].button == btn) {
                        LaunchSelectedApp(static_cast<int>(i), false);
                        return;
                    }
                }
            });
            flyout.Items().Append(openItem);

        if (item.canRunAsAdmin && !isWebItem) {
            wuxc::MenuFlyoutItem adminItem;
            adminItem.Text(L"Run as administrator");
            wuxc::FontIcon adminIcon;
            adminIcon.Glyph(L"\uE7EF");
            adminItem.Icon(adminIcon);
            adminItem.Click([btn = button](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                for (size_t i = 0; i < g_activeApps.size(); ++i) {
                    if (g_activeApps[i].button == btn) {
                        LaunchSelectedApp(static_cast<int>(i), true);
                        return;
                    }
                }
            });
            flyout.Items().Append(adminItem);
        }

        if (isWebItem) {
            std::wstring webUrl = item.openPath;
            wuxc::MenuFlyoutItem copyUrlItem;
            copyUrlItem.Text(L"Copy search link");
            wuxc::FontIcon copyIcon;
            copyIcon.Glyph(L"\uE8C8");
            copyUrlItem.Icon(copyIcon);
            copyUrlItem.Click([webUrl](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                CopyTextToClipboard(webUrl);
            });
            flyout.Items().Append(copyUrlItem);
        } else if (!item.openPath.empty() && GetFileAttributesW(item.openPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::wstring locTarget = item.openPath;

            wuxc::MenuFlyoutSeparator sep1;
            flyout.Items().Append(sep1);

            wuxc::MenuFlyoutItem cutItem;
            cutItem.Text(L"Cut");
            wuxc::FontIcon cutIcon;
            cutIcon.Glyph(L"\uE8C6");
            cutItem.Icon(cutIcon);
            cutItem.Click([locTarget](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyOrCutFileToClipboard(locTarget, true /* isCut */);
            });
            flyout.Items().Append(cutItem);

            wuxc::MenuFlyoutItem copyItem;
            copyItem.Text(L"Copy");
            wuxc::FontIcon copyIcon;
            copyIcon.Glyph(L"\uE8C8");
            copyItem.Icon(copyIcon);
            copyItem.Click([locTarget](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyOrCutFileToClipboard(locTarget, false /* isCut */);
            });
            flyout.Items().Append(copyItem);

            wuxc::MenuFlyoutItem copyPathItem;
            copyPathItem.Text(L"Copy path");
            wuxc::FontIcon copyPathIcon;
            copyPathIcon.Glyph(L"\uE71B");
            copyPathItem.Icon(copyPathIcon);
            copyPathItem.Click([locTarget](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyTextToClipboard(locTarget);
            });
            flyout.Items().Append(copyPathItem);

            wuxc::MenuFlyoutSeparator sep2;
            flyout.Items().Append(sep2);

            wuxc::MenuFlyoutItem locItem;
            locItem.Text(L"Open file location");
            wuxc::FontIcon locIcon;
            locIcon.Glyph(L"\uE838");
            locItem.Icon(locIcon);
            locItem.Click([locTarget](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                OpenFileLocation(locTarget);
            });
            flyout.Items().Append(locItem);
        }
        }

        button.ContextFlyout(flyout);

        return AppCardUI{item.appIndex, item.title, item.openPath, button, item.canRunAsAdmin};
    };

    auto makeAppEmptyCard = []() -> wuxc::Border {
        wuxc::Border emptyCard;
        emptyCard.CornerRadius(wux::CornerRadius{8, 8, 8, 8});
        emptyCard.Background(MakeBrush(0x0A, 0xFF, 0xFF, 0xFF));
        emptyCard.BorderBrush(MakeBrush(0x10, 0xFF, 0xFF, 0xFF));
        emptyCard.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
        emptyCard.Padding(wux::ThicknessHelper::FromLengths(16, 22, 16, 22));
        emptyCard.Margin(wux::ThicknessHelper::FromLengths(6, 10, 6, 8));
        emptyCard.HorizontalAlignment(wux::HorizontalAlignment::Stretch);

        wuxc::StackPanel emptyStack;
        emptyStack.HorizontalAlignment(wux::HorizontalAlignment::Center);

        wuxc::FontIcon emptyIcon;
        emptyIcon.Glyph(L"\uE71D");
        emptyIcon.FontSize(24);
        emptyIcon.Opacity(0.2);
        emptyIcon.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptyIcon.Margin(wux::ThicknessHelper::FromLengths(0, 0, 0, 6));
        emptyStack.Children().Append(emptyIcon);

        wuxc::TextBlock emptyTitle;
        emptyTitle.Text(L"No matching applications");
        emptyTitle.FontSize(11.5);
        emptyTitle.FontWeight(wut::FontWeights::SemiBold());
        emptyTitle.Opacity(0.45);
        emptyTitle.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptyStack.Children().Append(emptyTitle);

        wuxc::TextBlock emptySubtitle;
        emptySubtitle.Text(L"Check files or refine your query");
        emptySubtitle.FontSize(10.5);
        emptySubtitle.Opacity(0.3);
        emptySubtitle.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptySubtitle.Margin(wux::ThicknessHelper::FromLengths(0, 2, 0, 0));
        emptyStack.Children().Append(emptySubtitle);

        emptyCard.Child(emptyStack);
        return emptyCard;
    };

    auto isMatch = [](const AppCardUI& card, const Row& row) {
        if (!card.openPath.empty() && !row.openPath.empty()) {
            return card.openPath == row.openPath && card.title == row.title;
        }
        return card.title == row.title;
    };

    if (appNames.empty()) {
        for (int i = static_cast<int>(g_activeApps.size()) - 1; i >= 0; --i) {
            g_appsList.Children().RemoveAt(i);
        }
        g_activeApps.clear();
        g_appButtons.clear();

        if (g_appsList.Children().Size() == 0) {
            g_appsList.Children().Append(makeAppEmptyCard());
        }
        SetAppSelection(-1);
    } else {
        if (g_activeApps.empty() && g_appsList.Children().Size() > 0) {
            g_appsList.Children().Clear();
        }

        // 1. Remove cards that are no longer in appNames
        for (int i = static_cast<int>(g_activeApps.size()) - 1; i >= 0; --i) {
            bool found = false;
            for (const auto& newApp : appNames) {
                if (isMatch(g_activeApps[i], newApp)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                g_appsList.Children().RemoveAt(i);
                g_activeApps.erase(g_activeApps.begin() + i);
            }
        }

        // 2. Insert new cards or reorder existing ones
        for (size_t targetIdx = 0; targetIdx < appNames.size(); ++targetIdx) {
            const Row& want = appNames[targetIdx];
            int existingIdx = -1;
            for (size_t i = targetIdx; i < g_activeApps.size(); ++i) {
                if (isMatch(g_activeApps[i], want)) {
                    existingIdx = static_cast<int>(i);
                    break;
                }
            }

            if (existingIdx >= 0) {
                g_activeApps[existingIdx].appIndex = want.appIndex;
                g_activeApps[existingIdx].canRunAsAdmin = want.canRunAsAdmin;

                if (static_cast<size_t>(existingIdx) != targetIdx) {
                    auto card = g_activeApps[existingIdx];
                    g_activeApps.erase(g_activeApps.begin() + existingIdx);
                    g_activeApps.insert(g_activeApps.begin() + targetIdx, card);

                    g_appsList.Children().RemoveAt(existingIdx);
                    g_appsList.Children().InsertAt(static_cast<uint32_t>(targetIdx), card.button);
                }
            } else {
                AppCardUI newCard = makeAppCard(want);
                g_activeApps.insert(g_activeApps.begin() + targetIdx, newCard);
                g_appsList.Children().InsertAt(static_cast<uint32_t>(targetIdx), newCard.button);
            }
        }

        g_appButtons.clear();
        for (const auto& card : g_activeApps) {
            g_appButtons.push_back(card.button);
        }

        SetAppSelection(0);
    }

    // Files renderer
    auto makeFileRow = [](const Row& item) {
        wuxc::Grid layout;
        wuxc::ColumnDefinition iconCol, textCol;
        iconCol.Width(wux::GridLengthHelper::FromValueAndType(0, wux::GridUnitType::Auto));
        textCol.Width(wux::GridLengthHelper::FromValueAndType(1, wux::GridUnitType::Star));
        layout.ColumnDefinitions().Append(iconCol);
        layout.ColumnDefinitions().Append(textCol);

        bool hasBitmap = false;
        if (item.icon.size() == static_cast<size_t>(kIconSize) * kIconSize * 4) {
            wuxmi::WriteableBitmap bmp{kIconSize, kIconSize};
            auto buffer = bmp.PixelBuffer();
            auto access = buffer.as<::Windows::Storage::Streams::IBufferByteAccess>();
            BYTE* dest = nullptr;
            if (SUCCEEDED(access->Buffer(&dest)) && dest) {
                memcpy(dest, item.icon.data(), item.icon.size());
                bmp.Invalidate();
                wuxc::Image image;
                image.Source(bmp);
                image.Width(kIconDisplay);
                image.Height(kIconDisplay);
                image.Margin(wux::ThicknessHelper::FromLengths(0, 0, 10, 0));
                image.VerticalAlignment(wux::VerticalAlignment::Center);
                wuxc::Grid::SetColumn(image, 0);
                layout.Children().Append(image);
                hasBitmap = true;
            }
        }

        if (!hasBitmap) {
            wuxc::Border iconBox;
            iconBox.Width(kIconDisplay);
            iconBox.Height(kIconDisplay);
            iconBox.Margin(wux::ThicknessHelper::FromLengths(0, 0, 10, 0));
            iconBox.VerticalAlignment(wux::VerticalAlignment::Center);

            wuxc::FontIcon fallbackIcon;
            fallbackIcon.FontSize(15);
            fallbackIcon.Opacity(0.5);
            fallbackIcon.HorizontalAlignment(wux::HorizontalAlignment::Center);
            fallbackIcon.VerticalAlignment(wux::VerticalAlignment::Center);

            DWORD attr = (!item.openPath.empty()) ? GetFileAttributesW(item.openPath.c_str()) : INVALID_FILE_ATTRIBUTES;
            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                fallbackIcon.Glyph(L"\uE8B7");
            } else {
                fallbackIcon.Glyph(L"\uE8A5");
            }
            iconBox.Child(fallbackIcon);
            wuxc::Grid::SetColumn(iconBox, 0);
            layout.Children().Append(iconBox);
        }

        wuxc::StackPanel text;
        wuxc::Grid::SetColumn(text, 1);
        text.VerticalAlignment(wux::VerticalAlignment::Center);

        wuxc::TextBlock name;
        name.Text(winrt::hstring{item.title});
        name.FontSize(12.5);
        name.FontWeight(wut::FontWeights::SemiBold());
        name.TextTrimming(wux::TextTrimming::CharacterEllipsis);
        name.TextWrapping(wux::TextWrapping::NoWrap);
        text.Children().Append(name);

        if (!item.subtitle.empty()) {
            wuxc::TextBlock sub;
            sub.Text(winrt::hstring{item.subtitle});
            sub.Opacity(0.45);
            sub.FontSize(10.5);
            sub.Margin(wux::ThicknessHelper::FromLengths(0, 1, 0, 0));
            sub.TextTrimming(wux::TextTrimming::CharacterEllipsis);
            sub.TextWrapping(wux::TextWrapping::NoWrap);
            text.Children().Append(sub);
        }
        layout.Children().Append(text);

        wuxc::Button button;
        button.Content(layout);
        button.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
        button.HorizontalContentAlignment(wux::HorizontalAlignment::Stretch);
        button.Background(MakeBrush(0, 0, 0, 0));
        button.BorderThickness(wux::ThicknessHelper::FromUniformLength(0));
        button.CornerRadius(wux::CornerRadius{6, 6, 6, 6});
        button.Padding(wux::ThicknessHelper::FromLengths(10, 6, 10, 6));
        button.Margin(wux::ThicknessHelper::FromLengths(2, 1, 2, 1));

        if (!item.openPath.empty()) {
            std::wstring target = item.openPath;
            button.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                OpenResult(target);
            });

            wuxc::MenuFlyout flyout;
            wuxc::MenuFlyoutItem openItem;
            openItem.Text(L"Open");
            wuxc::FontIcon openIcon;
            openIcon.Glyph(L"\uE8A7");
            openItem.Icon(openIcon);
            openItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                OpenResult(target);
            });
            flyout.Items().Append(openItem);

            std::wstring lower = target;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
            bool canElevate = lower.ends_with(L".exe") || lower.ends_with(L".bat") ||
                              lower.ends_with(L".cmd") || lower.ends_with(L".ps1") ||
                              lower.ends_with(L".msc") || lower.ends_with(L".lnk");
            if (canElevate) {
                wuxc::MenuFlyoutItem adminItem;
                adminItem.Text(L"Run as administrator");
                wuxc::FontIcon adminIcon;
                adminIcon.Glyph(L"\uE7EF");
                adminItem.Icon(adminIcon);
                adminItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                    DismissStartMenu();
                    OpenResult(target, true /* asAdmin */);
                });
                flyout.Items().Append(adminItem);
            }

            wuxc::MenuFlyoutSeparator sep1;
            flyout.Items().Append(sep1);

            wuxc::MenuFlyoutItem cutItem;
            cutItem.Text(L"Cut");
            wuxc::FontIcon cutIcon;
            cutIcon.Glyph(L"\uE8C6");
            cutItem.Icon(cutIcon);
            cutItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyOrCutFileToClipboard(target, true /* isCut */);
            });
            flyout.Items().Append(cutItem);

            wuxc::MenuFlyoutItem copyItem;
            copyItem.Text(L"Copy");
            wuxc::FontIcon copyIcon;
            copyIcon.Glyph(L"\uE8C8");
            copyItem.Icon(copyIcon);
            copyItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyOrCutFileToClipboard(target, false /* isCut */);
            });
            flyout.Items().Append(copyItem);

            wuxc::MenuFlyoutItem copyPathItem;
            copyPathItem.Text(L"Copy path");
            wuxc::FontIcon copyPathIcon;
            copyPathIcon.Glyph(L"\uE71B");
            copyPathItem.Icon(copyPathIcon);
            copyPathItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                tools::CopyTextToClipboard(target);
            });
            flyout.Items().Append(copyPathItem);

            wuxc::MenuFlyoutSeparator sep2;
            flyout.Items().Append(sep2);

            wuxc::MenuFlyoutItem locItem;
            locItem.Text(L"Open file location");
            wuxc::FontIcon locIcon;
            locIcon.Glyph(L"\uE838");
            locItem.Icon(locIcon);
            locItem.Click([target](wf::IInspectable const&, wux::RoutedEventArgs const&) {
                DismissStartMenu();
                OpenFileLocation(target);
            });
            flyout.Items().Append(locItem);

            button.ContextFlyout(flyout);
        }
        return button;
    };

    // Files on the right.
    if (files.empty()) {
        wuxc::Border emptyCard;
        emptyCard.CornerRadius(wux::CornerRadius{8, 8, 8, 8});
        emptyCard.Background(MakeBrush(0x0A, 0xFF, 0xFF, 0xFF));
        emptyCard.BorderBrush(MakeBrush(0x10, 0xFF, 0xFF, 0xFF));
        emptyCard.BorderThickness(wux::ThicknessHelper::FromUniformLength(1));
        emptyCard.Padding(wux::ThicknessHelper::FromLengths(16, 22, 16, 22));
        emptyCard.Margin(wux::ThicknessHelper::FromLengths(6, 10, 6, 8));
        emptyCard.HorizontalAlignment(wux::HorizontalAlignment::Stretch);

        wuxc::StackPanel emptyStack;
        emptyStack.HorizontalAlignment(wux::HorizontalAlignment::Center);

        wuxc::FontIcon emptyIcon;
        emptyIcon.Glyph(L"\uE8B7");
        emptyIcon.FontSize(24);
        emptyIcon.Opacity(0.2);
        emptyIcon.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptyIcon.Margin(wux::ThicknessHelper::FromLengths(0, 0, 0, 6));
        emptyStack.Children().Append(emptyIcon);

        wuxc::TextBlock emptyTitle;
        emptyTitle.Text(L"No matching files");
        emptyTitle.FontSize(11.5);
        emptyTitle.FontWeight(wut::FontWeights::SemiBold());
        emptyTitle.Opacity(0.45);
        emptyTitle.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptyStack.Children().Append(emptyTitle);

        wuxc::TextBlock emptySubtitle;
        emptySubtitle.Text(L"Everything index returned 0 items");
        emptySubtitle.FontSize(10.5);
        emptySubtitle.Opacity(0.3);
        emptySubtitle.HorizontalAlignment(wux::HorizontalAlignment::Center);
        emptySubtitle.Margin(wux::ThicknessHelper::FromLengths(0, 2, 0, 0));
        emptyStack.Children().Append(emptySubtitle);

        emptyCard.Child(emptyStack);
        g_resultsList.Children().Append(emptyCard);
    } else {
        for (const Row& file : files) {
            g_resultsList.Children().Append(makeFileRow(file));
        }
    }

    if (g_ourBox && g_ourBox.Text().empty()) {
        HideOverlayAnimated();
    } else {
        RevealOverlayAnimated();
    }
    Rec(L"render: %zu apps, %zu files, host %.0fx%.0f", appNames.size(),
        files.size(), g_resultsHost.ActualWidth(),
        g_resultsHost.ActualHeight());
} catch (...) {
    Rec(L"render failed %08X", static_cast<unsigned>(winrt::to_hresult()));
}

// Called from the search thread once results are in.
void RequestRender() {
    try {
        if (!g_ourBox) {
            return;
        }
        auto dispatcher = g_ourBox.Dispatcher();
        if (!dispatcher) {
            return;
        }
        dispatcher.RunAsync(wuc::CoreDispatcherPriority::High,
                            wuc::DispatchedHandler{[] { RenderResults(); }});
    } catch (...) {
    }
}

// ---------------------------------------------------------------------------
// Asking Everything, from inside the Start menu
//
// On its own thread, with its own message pump: the IPC is a WM_COPYDATA
// round trip and the reply lands on a window, so it cannot run on the XAML
// thread without blocking the menu while the user types.
//
// Debounced, because a keystroke every ~60ms would otherwise be a query every
// ~60ms. 120ms was measured as comfortable in the broker.
// ---------------------------------------------------------------------------



void QueueQuery(std::wstring text) {
    {
        std::lock_guard<std::mutex> lock(g_queryMutex);
        g_pendingQuery = std::move(text);
    }
    g_queryDirty.store(true);
    g_queryWake.notify_all();
}

// An app's icon, from its shell identity.
//
// Not the extension cache: that answers from a file type, and an app is not a
// file type -- every app has its own icon, so there is nothing to share. It
// goes through the item itself for the same reason the index stores PIDLs
// rather than paths: AUMIDs and known-folder GUIDs cannot be re-parsed back
// into something SHGetFileInfo understands.
//
// Measured at roughly 9.7ms per app in the broker, so this runs on the search
// thread and is cached by name. Six visible rows make it about 60ms once, and
// nothing after that.
bool FetchAppIcon(const apps::App* app, int size, std::vector<BYTE>* out) {
    if (!app || !app->pidl || !out) {
        return false;
    }
    IShellItem* item = nullptr;
    if (FAILED(SHCreateItemFromIDList(app->pidl.get(), IID_PPV_ARGS(&item))) ||
        !item) {
        return false;
    }
    bool ok = false;
    IShellItemImageFactory* factory = nullptr;
    if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&factory))) && factory) {
        SIZE want{size, size};
        HBITMAP bitmap = nullptr;
        if (SUCCEEDED(factory->GetImage(
                want, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &bitmap)) &&
            bitmap) {
            ok = icons::BitmapToBgra(bitmap, size, out);
            DeleteObject(bitmap);
        }
        factory->Release();
    }
    item->Release();
    return ok;
}

// Opens an app by its shell identity.
//
// By PIDL rather than by name: apps_index.h explains why -- the parsing names
// come in several shapes, including AUMIDs and known-folder GUIDs, and
// rebuilding a path from them fails outright for some. The PIDL works for all
// of them.
//
// Called only on the search thread, which owns the index and therefore the
void LaunchAppAsync(std::wstring name, std::wstring path, ITEMIDLIST* pidl, bool asAdmin = false) {
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    bool launched = false;

    wchar_t userProfile[MAX_PATH] = {};
    if (!GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) || !userProfile[0]) {
        PWSTR kf = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &kf)) && kf) {
            wcsncpy_s(userProfile, kf, MAX_PATH - 1);
            CoTaskMemFree(kf);
        }
    }

    std::wstring lowerPath = path;
    for (auto& c : lowerPath) c = static_cast<wchar_t>(towlower(c));
    std::wstring lowerName = name;
    for (auto& c : lowerName) c = static_cast<wchar_t>(towlower(c));

    bool isTerminalOrShell = (lowerPath.find(L"cmd.exe") != std::wstring::npos ||
                              lowerPath.find(L"powershell") != std::wstring::npos ||
                              lowerPath.find(L"pwsh") != std::wstring::npos ||
                              lowerPath.find(L"windowsterminal") != std::wstring::npos ||
                              lowerPath.ends_with(L"wt.exe") ||
                              lowerName == L"command prompt" ||
                              lowerName.find(L"powershell") != std::wstring::npos ||
                              lowerName == L"terminal" ||
                              lowerName == L"windows terminal");

    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring parentDir = (lastSlash != std::wstring::npos) ? path.substr(0, lastSlash) : L"";
    LPCWSTR workDir = isTerminalOrShell ? userProfile : (!parentDir.empty() ? parentDir.c_str() : userProfile);

    // Expand environment strings if any (e.g. %windir%\system32\...)
    if (path.find(L'%') != std::wstring::npos) {
        wchar_t expanded[MAX_PATH] = {};
        if (ExpandEnvironmentStringsW(path.c_str(), expanded, MAX_PATH) > 0) {
            path = expanded;
        }
    }

    // 0. If path is a URI (e.g. ms-settings:, http:, https:) or command:
    if (!path.empty()) {
        if (path.starts_with(L"ms-settings:") || path.starts_with(L"http:") || path.starts_with(L"https:")) {
            HINSTANCE hInst = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(hInst) > 32) {
                Rec(L"launched URI setting: %ls", path.c_str());
                launched = true;
            } else {
                Rec(L"URI launch failed (%ld): %ls", reinterpret_cast<INT_PTR>(hInst), path.c_str());
            }
        } else if (path.starts_with(L"control ")) {
            std::wstring params = path.substr(8);
            HINSTANCE hInst = ShellExecuteW(nullptr, asAdmin ? L"runas" : L"open", L"control.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(hInst) > 32) {
                Rec(L"launched control applet: %ls", path.c_str());
                launched = true;
            }
        }
    }

    // 1. If path is a real file on disk or executable in PATH (e.g. C:\Windows\System32\cmd.exe, ncpa.cpl, services.msc, cleanmgr.exe):
    bool isFileOnDisk = false;
    std::wstring extraParams;
    if (!launched && !path.empty() && path.find(L"http") != 0 && !path.starts_with(L"ms-settings:")) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            isFileOnDisk = true;
        } else {
            // Try resolving via PATH / System32
            wchar_t resolved[MAX_PATH] = {};
            if (SearchPathW(nullptr, path.c_str(), nullptr, MAX_PATH, resolved, nullptr) > 0) {
                path = resolved;
                isFileOnDisk = true;
            } else {
                // Check if path has arguments (e.g. "rundll32.exe sysdm.cpl,EditEnvironmentVariables" or "explorer.exe shell:::{...}")
                size_t spacePos = path.find(L' ');
                if (spacePos != std::wstring::npos) {
                    std::wstring exePart = path.substr(0, spacePos);
                    std::wstring paramPart = path.substr(spacePos + 1);
                    if (SearchPathW(nullptr, exePart.c_str(), nullptr, MAX_PATH, resolved, nullptr) > 0) {
                        path = resolved;
                        extraParams = paramPart;
                        isFileOnDisk = true;
                    }
                }
            }
        }
    }

    if (isFileOnDisk) {
        std::wstring params;
        if (lowerPath.ends_with(L"cmd.exe")) {
            params = L"/k cd /d \"" + std::wstring(userProfile) + L"\"";
        } else if (lowerPath.find(L"powershell.exe") != std::wstring::npos || lowerPath.ends_with(L"pwsh.exe")) {
            params = L"-NoExit -Command \"Set-Location '" + std::wstring(userProfile) + L"'\"";
        }
        if (!extraParams.empty()) {
            if (!params.empty()) params += L" ";
            params += extraParams;
        }

        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        // If asAdmin, do NOT suppress UI (UAC elevation prompt must show).
        info.fMask = SEE_MASK_NOASYNC | (asAdmin ? 0 : SEE_MASK_FLAG_NO_UI);
        info.lpVerb = asAdmin ? L"runas" : L"open";
        info.lpFile = path.c_str();
        if (!params.empty()) {
            info.lpParameters = params.c_str();
        }
        info.lpDirectory = workDir;
        info.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&info)) {
            Rec(L"launched app (by file path): %ls (admin=%d, dir=%ls)", name.c_str(), asAdmin ? 1 : 0, workDir ? workDir : L"(null)");
            launched = true;
        } else {
            Rec(L"app file launch failed (%lu): %ls, trying fallback", GetLastError(), path.c_str());
        }
    }

    // 2. If not launched yet and we have a PIDL:
    if (!launched && pidl) {
        if (asAdmin) {
            // Try IContextMenu verb "runas"
            IShellItem* item = nullptr;
            if (SUCCEEDED(SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&item))) && item) {
                IContextMenu* menu = nullptr;
                if (SUCCEEDED(item->BindToHandler(nullptr, BHID_SFUIObject, IID_PPV_ARGS(&menu))) && menu) {
                    CMINVOKECOMMANDINFOEX ici{};
                    ici.cbSize = sizeof(ici);
                    ici.fMask = CMIC_MASK_UNICODE;
                    ici.lpVerb = "runas";
                    ici.lpVerbW = L"runas";
                    ici.lpDirectoryW = workDir;
                    ici.nShow = SW_SHOWNORMAL;
                    HRESULT hr = menu->InvokeCommand(reinterpret_cast<CMINVOKECOMMANDINFO*>(&ici));
                    menu->Release();
                    item->Release();
                    if (SUCCEEDED(hr)) {
                        Rec(L"launched app (by IContextMenu runas): %ls", name.c_str());
                        launched = true;
                    } else {
                        Rec(L"IContextMenu runas failed (%08X) for %ls", static_cast<unsigned>(hr), name.c_str());
                    }
                } else if (item) {
                    item->Release();
                }
            }

            if (!launched) {
                // Fallback: ShellExecuteExW with SEE_MASK_IDLIST and "runas"
                SHELLEXECUTEINFOW info{};
                info.cbSize = sizeof(info);
                info.fMask = SEE_MASK_IDLIST | SEE_MASK_NOASYNC;
                info.lpIDList = pidl;
                info.lpVerb = L"runas";
                info.lpDirectory = workDir;
                info.nShow = SW_SHOWNORMAL;
                if (ShellExecuteExW(&info)) {
                    Rec(L"launched app (by PIDL runas): %ls", name.c_str());
                    launched = true;
                } else {
                    Rec(L"app PIDL runas failed (%lu): %ls", GetLastError(), name.c_str());
                }
            }
        }
        
        // Fallback to normal launch if runas failed or was not requested:
        if (!launched) {
            SHELLEXECUTEINFOW info{};
            info.cbSize = sizeof(info);
            info.fMask = SEE_MASK_IDLIST | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
            info.lpIDList = pidl;
            info.lpVerb = L"open";
            info.lpDirectory = workDir;
            info.nShow = SW_SHOWNORMAL;
            if (ShellExecuteExW(&info)) {
                Rec(L"launched app (by PIDL open): %ls", name.c_str());
                launched = true;
            } else {
                Rec(L"app PIDL open failed (%lu): %ls", GetLastError(), name.c_str());
            }
        }
    }

    // 3. Fallback direct ShellExecuteExW for any remaining commands without PIDL:
    if (!launched && !path.empty()) {
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOASYNC | (asAdmin ? 0 : SEE_MASK_FLAG_NO_UI);
        info.lpVerb = asAdmin ? L"runas" : L"open";
        info.lpFile = path.c_str();
        info.lpDirectory = workDir;
        info.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&info)) {
            Rec(L"launched app (by direct fallback): %ls (admin=%d)", path.c_str(), asAdmin ? 1 : 0);
            launched = true;
        } else {
            Rec(L"app direct fallback launch failed (%lu): %ls", GetLastError(), path.c_str());
        }
    }

    if (SUCCEEDED(comHr)) {
        CoUninitialize();
    }
}

void SearchThreadMain() {
    // COM for the apps index: it enumerates shell:AppsFolder.
    HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    apps::Index appIndex;
    if (appIndex.Rebuild()) {
        Rec(L"apps: indexed");
    } else {
        Rec(L"apps: index failed");
    }

    icons::ExtensionCache iconCache(kIconSize);

    // The apps behind the rows currently on screen, in the same order.
    std::vector<const apps::App*> lastHits;

    // One fetch per app, ever. Keyed by name because that is what identifies
    // an entry across index rebuilds.
    std::map<std::wstring, std::vector<BYTE>> appIconCache;

    everything::Client client;
    if (!client.Init()) {
        Rec(L"search: could not create the reply window");
        if (SUCCEEDED(comHr)) {
            CoUninitialize();
        }
        return;
    }
    Rec(L"search: ready (Everything %ls)",
        everything::FindIpcWindow() ? L"found" : L"NOT running");

    std::wstring last;
    while (!g_searchQuit.load()) {
        std::wstring query;
        {
            std::unique_lock<std::mutex> lock(g_queryMutex);
            g_queryWake.wait_for(lock, std::chrono::milliseconds(200), [] {
                return g_queryDirty.load() || g_searchQuit.load() || (g_launchRequest.load() >= 0);
            });
            if (g_searchQuit.load()) {
                if (SUCCEEDED(comHr)) {
                    CoUninitialize();
                }
                return;
            }
            int wanted = g_launchRequest.exchange(-1);
            if (wanted >= 0) {
                bool asAdmin = g_launchAsAdmin.exchange(false);
                if (static_cast<size_t>(wanted) < lastHits.size() && lastHits[wanted]) {
                    const auto* app = lastHits[wanted];
                    std::wstring name = app->name;
                    std::wstring targetPath = app->targetPath;
                    ITEMIDLIST* pidlClone = app->pidl ? ILClone(app->pidl.get()) : nullptr;
                    lock.unlock();

                    SpawnTrackedLaunch([name = std::move(name), targetPath = std::move(targetPath), pidlClone, asAdmin] {
                        LaunchAppAsync(name, targetPath, pidlClone, asAdmin);
                        if (pidlClone) {
                            ILFree(pidlClone);
                        }
                    });
                } else {
                    lock.unlock();
                }
                continue;
            }
            if (!g_queryDirty.exchange(false)) {
                continue;
            }
            query = g_pendingQuery;
        }

        // Settle: if starting a fresh query from empty, search immediately with
        // zero delay so results are ready before overlay reveals.
        // For subsequent typing bursts, settle for 25ms so we search the newer text.
        if (!last.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            std::lock_guard<std::mutex> lock(g_queryMutex);
            if (g_pendingQuery != query) {
                g_queryDirty.store(true);
                continue;
            }
        }

        if (query == last) {
            continue;
        }
        last = query;

        if (query.empty()) {
            {
                std::lock_guard<std::mutex> lock(g_resultsMutex);
                g_results.clear();
                g_appRows.clear();
                g_fileRows.clear();
                g_totalMatches.store(0);
            }
            RequestRender();
            continue;
        }


        std::wstring qTrim = tools::Trim(query);
        std::wstring qLower = tools::ToLower(qTrim);

        bool isIpCommand = (qLower == L"/ip" || qLower.starts_with(L"/ip "));
        bool isCCommand = (qLower == L"/c" || qLower.starts_with(L"/c ") ||
                          (qLower.size() >= 3 && qLower[0] == L'/' && qLower[1] == L'c' &&
                           (iswdigit(qLower[2]) || qLower[2] == L'-' || qLower[2] == L'+' || qLower[2] == L'(')));

        bool isExplicitWeb = query.starts_with(L"?");
        std::wstring webQuery;
        ResolvedWebQuery explicitWeb;
        if (isExplicitWeb) {
            webQuery = query.substr(1);
            while (!webQuery.empty() && webQuery.front() == L' ') {
                webQuery.erase(0, 1);
            }
            explicitWeb = ResolveWebSearch(webQuery);
        }

        std::vector<everything::Result> pool;
        DWORD total = 0;
        long long ms = 0;

        int maxFiles = 12;
        int maxApps = 6;
        {
            std::lock_guard<std::mutex> lock(g_settingsMutex);
            maxFiles = g_settings.maxFileResults;
            maxApps = g_settings.maxAppResults;
        }

        if (isIpCommand || isCCommand) {
            pool.clear();
            total = 0;
            ms = 0;
        } else if (isExplicitWeb) {
            if (!explicitWeb.queryTerm.empty()) {
                auto start = std::chrono::steady_clock::now();
                if (client.Query(explicitWeb.queryTerm, ranker::kDefaultPool, &pool, &total)) {
                    ranker::Rank(&pool, explicitWeb.queryTerm, static_cast<size_t>(maxFiles));
                }
                ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - start)
                         .count();
            }
        } else {
            auto start = std::chrono::steady_clock::now();
            bool ok = client.Query(query, ranker::kDefaultPool, &pool, &total);
            ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - start)
                          .count();
            if (!ok) {
                Rec(L"search: '%ls' failed (Everything running?)", query.c_str());
                continue;
            }
            ranker::Rank(&pool, query, static_cast<size_t>(maxFiles));
        }

        // Apps are a name match over an index built once at startup, so this
        // costs nothing next to the file query.
        std::vector<Row> appRows;
        lastHits.clear();

        if (isIpCommand) {
            auto ifaces = tools::GetNetworkInterfaces();
            if (ifaces.empty()) {
                Row r;
                r.title = L"No Active Network Interfaces";
                r.subtitle = L"Check your Wi-Fi, Ethernet, or VPN connection";
                r.customGlyph = L"\uE701";
                r.canRunAsAdmin = false;
                r.appIndex = static_cast<int>(lastHits.size());
                lastHits.push_back(nullptr);
                appRows.push_back(std::move(r));
            } else {
                for (const auto& iface : ifaces) {
                    Row r;
                    r.title = iface.ip;
                    std::wstring sub = iface.typeLabel + L": " + iface.adapterName;
                    if (!iface.mask.empty()) sub += L" \u2022 Subnet: " + iface.mask;
                    if (!iface.gateway.empty()) sub += L" \u2022 GW: " + iface.gateway;
                    r.subtitle = sub + L" \u2022 Press Enter to copy";
                    r.copyText = iface.ip;
                    r.customGlyph = iface.glyph;
                    r.canRunAsAdmin = false;
                    r.appIndex = static_cast<int>(lastHits.size());
                    lastHits.push_back(nullptr);
                    appRows.push_back(std::move(r));
                }
            }
        } else if (isCCommand) {
            std::wstring cArg = tools::Trim(qTrim.substr(2));
            if (cArg.empty()) {
                Row r;
                r.title = L"Calculator & Unit Converter";
                r.subtitle = L"Usage: /c <expression> for math, or /c <number> [unit] for conversion \u2022 e.g. /c 100 * 5, /c 100 km, /c 50";
                r.customGlyph = L"\uE1D0";
                r.canRunAsAdmin = false;
                r.appIndex = static_cast<int>(lastHits.size());
                lastHits.push_back(nullptr);
                appRows.push_back(std::move(r));
            } else {
                double mathVal = 0.0;
                if (tools::EvaluateMath(cArg, mathVal)) {
                    Row r;
                    std::wstring formatted = tools::FormatCleanNumber(mathVal);
                    r.title = L"= " + formatted;
                    r.subtitle = cArg + L" \u2022 Press Enter to copy result";
                    r.copyText = formatted;
                    r.customGlyph = L"\uE1D0"; // Calculator
                    r.canRunAsAdmin = false;
                    r.appIndex = static_cast<int>(lastHits.size());
                    lastHits.push_back(nullptr);
                    appRows.push_back(std::move(r));
                } else {
                    double convNum = 0.0;
                    std::wstring convUnit;
                    bool isHelp = false;
                    if (tools::ParseConversionQuery(qTrim, convNum, convUnit, isHelp)) {
                        std::vector<CustomConversion> conversions;
                        {
                            std::lock_guard<std::mutex> lock(g_settingsMutex);
                            conversions = g_settings.unitConversions;
                        }

                        std::wstring normUnit = tools::NormalizeUnit(convUnit);
                        std::vector<Row> convRows;

                        for (const auto& c : conversions) {
                            if (!convUnit.empty()) {
                                std::wstring normFrom = tools::NormalizeUnit(c.fromUnit);
                                if (normFrom != normUnit && tools::ToLower(c.fromUnit) != convUnit) {
                                    continue;
                                }
                            }
                            double outVal = 0.0;
                            if (tools::EvaluateConversionFormula(c.formula, convNum, outVal)) {
                                Row r;
                                std::wstring numStr = tools::FormatCleanNumber(convNum);
                                std::wstring outStr = tools::FormatCleanNumber(outVal);
                                r.title = numStr + L" " + c.fromUnit + L" = " + outStr + L" " + c.toUnit;
                                r.subtitle = c.category + L" \u2022 Press Enter to copy " + outStr + L" " + c.toUnit;
                                r.copyText = outStr + L" " + c.toUnit;
                                r.customGlyph = L"\uE88E";
                                r.canRunAsAdmin = false;
                                r.appIndex = static_cast<int>(lastHits.size());
                                lastHits.push_back(nullptr);
                                convRows.push_back(std::move(r));
                            }
                        }

                        if (convUnit.empty() && convNum >= 0.0 && convNum <= 16777215.0 && (convNum == std::floor(convNum))) {
                            unsigned long long intVal = static_cast<unsigned long long>(convNum);
                            wchar_t hexBuf[32];
                            swprintf_s(hexBuf, L"0x%llX", intVal);
                            std::wstring binStr = L"0b";
                            if (intVal == 0) {
                                binStr += L"0";
                            } else {
                                for (int b = 31; b >= 0; --b) {
                                    if ((intVal >> b) & 1) {
                                        for (int j = b; j >= 0; --j) {
                                            binStr.push_back(((intVal >> j) & 1) ? L'1' : L'0');
                                        }
                                        break;
                                    }
                                }
                            }
                            wchar_t octBuf[32];
                            swprintf_s(octBuf, L"0o%llo", intVal);
                            std::wstring numStr = tools::FormatCleanNumber(convNum);

                            Row r;
                            r.title = numStr + L" = " + hexBuf + L" (Hex) = " + binStr + L" (Bin) = " + octBuf + L" (Oct)";
                            r.subtitle = L"Base Radix \u2022 Press Enter to copy " + std::wstring(hexBuf);
                            r.copyText = hexBuf;
                            r.customGlyph = L"\uE88E";
                            r.canRunAsAdmin = false;
                            r.appIndex = static_cast<int>(lastHits.size());
                            lastHits.push_back(nullptr);
                            convRows.push_back(std::move(r));
                        }

                        if (convRows.empty()) {
                            Row r;
                            if (!convUnit.empty()) {
                                r.title = L"Unknown Unit: \"" + convUnit + L"\"";
                                r.subtitle = L"No match in configured conversions. Configure custom units in Windhawk Settings.";
                            } else {
                                r.title = L"No Conversions Configured";
                                r.subtitle = L"Add unit conversion rules in Windhawk Mod Settings.";
                            }
                            r.customGlyph = L"\uE88E";
                            r.canRunAsAdmin = false;
                            r.appIndex = static_cast<int>(lastHits.size());
                            lastHits.push_back(nullptr);
                            convRows.push_back(std::move(r));
                        }

                        for (auto& cr : convRows) {
                            appRows.push_back(std::move(cr));
                        }
                    } else {
                        Row r;
                        r.title = L"Invalid Expression or Unit: \"" + cArg + L"\"";
                        r.subtitle = L"Usage: /c <expression> (e.g. /c 100 * 5) or /c <number> [unit] (e.g. /c 100 km)";
                        r.customGlyph = L"\uE1D0";
                        r.canRunAsAdmin = false;
                        r.appIndex = static_cast<int>(lastHits.size());
                        lastHits.push_back(nullptr);
                        appRows.push_back(std::move(r));
                    }
                }
            }
        } else if (isExplicitWeb) {
            Row webRow;
            if (explicitWeb.queryTerm.empty()) {
                if (explicitWeb.isShortcut) {
                    webRow.title = L"Open " + explicitWeb.serviceName;
                    webRow.subtitle = explicitWeb.serviceName + L" \u2022 " + explicitWeb.homeUrl;
                } else {
                    webRow.title = L"Search the web";
                    webRow.subtitle = explicitWeb.serviceName + L" Search";
                }
                webRow.openPath = explicitWeb.homeUrl;
            } else {
                webRow.title = L"Search " + explicitWeb.serviceName + L" for \"" + explicitWeb.queryTerm + L"\"";
                webRow.subtitle = explicitWeb.serviceName + L" Search \u2022 " + explicitWeb.queryTerm;
                webRow.openPath = explicitWeb.searchUrl;
            }
            webRow.canRunAsAdmin = false;
            webRow.appIndex = -1;
            appRows.push_back(std::move(webRow));
        } else {
            for (const apps::Match& m : appIndex.Search(query, static_cast<size_t>(maxApps))) {
                if (!m.app) {
                    continue;
                }
                Row row;
                row.title = m.app->name;
                bool canAdmin = true;
                if (m.app->isSetting || m.app->targetPath.starts_with(L"ms-settings:")) {
                    if (!m.app->area.empty()) {
                        row.subtitle = L"Settings \u2022 " + m.app->area;
                    } else {
                        row.subtitle = L"Settings";
                    }
                    canAdmin = false;
                } else if (!m.app->targetPath.empty() && GetFileAttributesW(m.app->targetPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    row.subtitle = m.app->targetPath;
                    canAdmin = true;
                } else if (!m.app->exeNameLower.empty()) {
                    row.subtitle = m.app->exeNameLower + L".exe";
                    if (m.app->name == L"Settings" || m.app->nameLower == L"settings" ||
                        m.app->targetPath.find(L"immersivecontrolpanel") != std::wstring::npos) {
                        canAdmin = false;
                    }
                } else {
                    row.subtitle = L"Application";
                }
                row.canRunAsAdmin = canAdmin;
                row.openPath = m.app->targetPath;
                row.appIndex = static_cast<int>(lastHits.size());

                auto cached = appIconCache.find(m.app->name);
                if (cached == appIconCache.end()) {
                    std::vector<BYTE> pixels;
                    if (!m.app->isSetting) {
                        FetchAppIcon(m.app, kIconSize, &pixels);
                    }
                    cached = appIconCache.emplace(m.app->name, std::move(pixels))
                                 .first;
                }
                row.icon = cached->second;

                appRows.push_back(std::move(row));
                // The App outlives this loop -- the index owns it and lives as
                // long as this thread -- so a pointer is safe here in a way it
                // would not be inside a XAML click handler.
                lastHits.push_back(m.app);
            }

            // Math expression check
            if (!query.empty()) {
                std::vector<Row> utilityRows;

                double mathVal = 0.0;
                std::wstring mathExpr = query;
                if (mathExpr.starts_with(L"=") || mathExpr.starts_with(L"/calc ")) {
                    if (mathExpr.starts_with(L"=")) mathExpr = mathExpr.substr(1);
                    else mathExpr = mathExpr.substr(6);
                }
                if (tools::EvaluateMath(mathExpr, mathVal)) {
                    Row r;
                    std::wstring formatted = tools::FormatCleanNumber(mathVal);
                    r.title = L"= " + formatted;
                    r.subtitle = tools::Trim(mathExpr) + L" \u2022 Press Enter to copy result";
                    r.copyText = formatted;
                    r.customGlyph = L"\uE1D0"; // Calculator
                    r.canRunAsAdmin = false;
                    r.appIndex = static_cast<int>(lastHits.size());
                    lastHits.push_back(nullptr);
                    utilityRows.push_back(std::move(r));
                }

                for (auto& ur : utilityRows) {
                    appRows.push_back(std::move(ur));
                }
            }
        }

        std::vector<Row> fileRows;
        for (const everything::Result& r : pool) {
            Row row;
            row.title = r.name;
            row.subtitle = r.path;
            row.openPath = r.path;
            if (!row.openPath.empty() && row.openPath.back() != L'\\') {
                row.openPath += L'\\';
            }
            row.openPath += r.name;
            // One shell call per distinct extension, not per row: the cache
            // answers from the registered file type without touching disk.
            if (const std::vector<BYTE>* pixels =
                    iconCache.Get(r.name, r.isFolder)) {
                row.icon = *pixels;
            }
            fileRows.push_back(std::move(row));
        }

        {
            std::lock_guard<std::mutex> lock(g_resultsMutex);
            g_results = pool;
            g_appRows = appRows;
            g_fileRows = fileRows;
            g_totalMatches.store(total);
        }
        RequestRender();
        Rec(L"search: '%ls' -> %u files (kept %zu), %zu apps (%lld ms)", query.c_str(),
            total, pool.size(), appRows.size(), static_cast<long long>(ms));
        for (size_t i = 0; i < appRows.size(); ++i) {
            Rec(L"    [app %zu] %ls -> %ls", i, appRows[i].title.c_str(), appRows[i].subtitle.c_str());
        }
        for (size_t i = 0; i < pool.size() && i < 5; ++i) {
            Rec(L"    [file %zu] %ls  %ls", i, pool[i].name.c_str(), pool[i].path.c_str());
        }
    }
}


// Everything of the shell's own that this mod hid, and what it looked like
// before we touched it.
//
// Weak, because the Start menu rebuilds its tree and we must not keep dead
// elements alive. Recorded in the order they were hidden and restored in
// reverse, so if the same element was hidden twice the value that comes back
// is the one from before the first time.
struct SuppressedElement {
    winrt::weak_ref<wux::FrameworkElement> element;
    wux::Visibility visibility = wux::Visibility::Visible;
    double opacity = 1.0;
    bool hitTestVisible = true;
    double height = std::numeric_limits<double>::quiet_NaN();
    double maxHeight = std::numeric_limits<double>::quiet_NaN();
    wux::Thickness margin{};
    bool isControl = false;
    bool tabStop = true;
};

[[clang::no_destroy]] std::vector<SuppressedElement> g_suppressed;

// Hides one of the shell's elements, remembering how to put it back.
//
// Every caller used to do this inline and none of them recorded anything,
// which is why disabling the mod left the Start menu with no search box at
// all and a cell that no longer measures.
void SuppressShellElement(wux::FrameworkElement const& fe, bool collapse = true,
                          bool zeroSize = false) {
    if (!fe) {
        return;
    }
    try {
        SuppressedElement saved;
        saved.element = winrt::make_weak(fe);
        saved.visibility = fe.Visibility();
        saved.opacity = fe.Opacity();
        saved.hitTestVisible = fe.IsHitTestVisible();
        saved.height = fe.Height();
        saved.maxHeight = fe.MaxHeight();
        saved.margin = fe.Margin();
        if (auto ctl = fe.try_as<wuxc::Control>()) {
            saved.isControl = true;
            saved.tabStop = ctl.IsTabStop();
        }
        g_suppressed.push_back(std::move(saved));

        fe.Opacity(0.0);
        fe.IsHitTestVisible(false);
        if (collapse) {
            fe.Visibility(wux::Visibility::Collapsed);
        }
        if (zeroSize) {
            fe.MaxHeight(0.0);
            fe.Height(0.0);
            fe.Margin(wux::ThicknessHelper::FromLengths(0, 0, 0, 0));
        }
    } catch (...) {
    }
}

// Undoes all of it. XAML thread, and before this DLL goes away.
void RestoreShellElements() {
    size_t restored = 0;
    for (auto it = g_suppressed.rbegin(); it != g_suppressed.rend(); ++it) {
        auto fe = it->element.get();
        if (!fe) {
            continue;  // that tree is already gone; nothing to put back
        }
        try {
            fe.Visibility(it->visibility);
            fe.Opacity(it->opacity);
            fe.IsHitTestVisible(it->hitTestVisible);
            // NaN is how XAML spells Auto, so this restores Auto correctly.
            fe.Height(it->height);
            fe.MaxHeight(it->maxHeight);
            fe.Margin(it->margin);
            if (it->isControl) {
                if (auto ctl = fe.try_as<wuxc::Control>()) {
                    ctl.IsTabStop(it->tabStop);
                }
            }
            ++restored;
        } catch (...) {
        }
    }
    Rec(L"teardown: restored %zu shell element(s) of %zu", restored,
        g_suppressed.size());
    g_suppressed.clear();
}

void RecursivelyHideTextBlocks(wux::DependencyObject const& node, int depth = 0) {
    if (!node || depth > 8) return;
    try {
        if (auto tb = node.try_as<wuxc::TextBlock>()) {
            SuppressShellElement(tb);
        }
        int count = wuxm::VisualTreeHelper::GetChildrenCount(node);
        for (int i = 0; i < count; ++i) {
            RecursivelyHideTextBlocks(wuxm::VisualTreeHelper::GetChild(node, i), depth + 1);
        }
    } catch (...) {}
}

void HideStockPlaceholder(wux::FrameworkElement const& stock) {
    if (!stock) return;
    try {
        if (auto ph = FindDescendantByName(stock, L"PlaceholderText", 8)) {
            SuppressShellElement(ph.try_as<wux::FrameworkElement>());
        }
        if (auto caret = FindDescendantByName(stock, L"TextCaret", 8)) {
            SuppressShellElement(caret.try_as<wux::FrameworkElement>());
        }
        if (auto stb = FindDescendantByName(stock, L"SearchTextBox", 8)) {
            SuppressShellElement(stb.try_as<wux::FrameworkElement>());
        }

        // Recursively hide all TextBlocks inside the stock search button
        RecursivelyHideTextBlocks(stock, 0);
    } catch (...) {}
}

void PlaceOurSearchBox(wux::FrameworkElement const& stockButton) try {
    g_stockButton = stockButton;
    SuppressShellElement(stockButton);
    if (auto ctl = stockButton.try_as<wuxc::Control>()) {
        ctl.IsTabStop(false);
    }
    HideStockPlaceholder(stockButton);

    auto parent = wuxm::VisualTreeHelper::GetParent(stockButton);
    auto cell = parent ? parent.try_as<wuxc::Panel>() : nullptr;
    if (!cell) {
        Rec(L"own box: stock button parent is not a Panel; cannot place");
        return;
    }

    auto owner = wuxm::VisualTreeHelper::GetParent(cell);
    auto ownerPanel = owner ? owner.try_as<wuxc::Panel>() : nullptr;
    if (!ownerPanel) {
        Rec(L"own box: cell parent is not a Panel; cannot place");
        return;
    }

    // The shell's own search cell, zeroed so our box can take its place.
    // Recorded, because leaving a container collapsed at zero height after
    // the mod goes away is what the Start menu cannot survive.
    SuppressShellElement(cell.try_as<wux::FrameworkElement>(), /*collapse=*/true,
                         /*zeroSize=*/true);


    // Verify if g_resultsHost is valid and currently attached to this ownerPanel
    bool needsBuild = false;
    if (!g_resultsHost) {
        needsBuild = true;
    } else {
        auto currentParent = wuxm::VisualTreeHelper::GetParent(g_resultsHost);
        if (!currentParent || currentParent != ownerPanel) {
            Rec(L"PlaceOurSearchBox: visual tree changed (Start Menu Styler)! Re-attaching results host.");
            if (currentParent) {
                if (auto oldPanel = currentParent.try_as<wuxc::Panel>()) {
                    uint32_t idx = 0;
                    if (oldPanel.Children().IndexOf(g_resultsHost, idx)) {
                        oldPanel.Children().RemoveAt(idx);
                    }
                }
            }
            needsBuild = true;
        }
    }

    if (needsBuild) {
        if (g_revealAnim) { g_revealAnim.Stop(); g_revealAnim = nullptr; }
        if (g_hideAnim) { g_hideAnim.Stop(); g_hideAnim = nullptr; }
        g_resultsTranslate = nullptr;
        g_resultsHost = nullptr;
        g_ourBox = nullptr;
        g_appsList = nullptr;
        g_resultsList = nullptr;
        g_activeApps.clear();
        g_appsHeaderHolder = nullptr;
        g_filesHeaderHolder = nullptr;
        BuildResultsList(ownerPanel);
    }

    // Proactively clean any other search boxes across the tree
    try {
        wux::DependencyObject node = cell;
        wux::DependencyObject menuRoot = cell;
        for (int up = 0; up < 12; ++up) {
            auto parentNode = wuxm::VisualTreeHelper::GetParent(node);
            if (!parentNode) break;
            menuRoot = parentNode;
            node = parentNode;
        }
        HideAllOtherSearchBoxes(menuRoot);
        DisarmScrollTabStops(menuRoot);
    } catch (...) {}

    SubclassStartMenuWindow();

    if (!g_coreEventsHooked) {
        try {
            if (auto window = wux::Window::Current()) {
                if (auto core = window.CoreWindow()) {
                    if (!g_charReceivedToken) {
                        g_charReceivedToken = core.CharacterReceived([](wuc::CoreWindow const&, wuc::CharacterReceivedEventArgs const& args) {
                            try {
                                unsigned code = args.KeyCode();
                                if (ProcessKeyChar(static_cast<wchar_t>(code))) {
                                    args.Handled(true);
                                }
                            } catch (...) {}
                        });
                    }

                    if (!g_keyDownToken) {
                        g_keyDownToken = core.KeyDown([](wuc::CoreWindow const&, wuc::KeyEventArgs const& args) {
                            try {
                                if (!g_ourBox) return;
                                auto key = args.VirtualKey();

                                if (key == winrt::Windows::System::VirtualKey::Escape) {
                                    if (g_isOverlayVisible.load() || g_isHiding.load()) {
                                        if (g_ourBox) g_ourBox.Text(L"");
                                        HideOverlayAnimated();
                                        args.Handled(true);
                                        return;
                                    }
                                }

                                if (key == winrt::Windows::System::VirtualKey::Down ||
                                    key == winrt::Windows::System::VirtualKey::Up ||
                                    key == winrt::Windows::System::VirtualKey::Enter) {
                                    if (g_isOverlayVisible.load()) {
                                        bool ctrl = (GetKeyState(VK_CONTROL) < 0) || ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
                                        HandleNavigationKey(key, ctrl);
                                        args.Handled(true);
                                        return;
                                    }
                                }

                                auto focused = wux::Input::FocusManager::GetFocusedElement();
                                if (focused && focused.try_as<wuxc::TextBox>()) {
                                    return;
                                }

                                std::wstring text{g_ourBox.Text()};
                                if (text.empty()) return;

                                if (key == winrt::Windows::System::VirtualKey::Back) {
                                    text.pop_back();
                                    g_ourBox.Text(text);
                                    if (text.empty()) {
                                        HideOverlayAnimated();
                                    } else {
                                        RevealOverlayAnimated();
                                        g_ourBox.Focus(wux::FocusState::Programmatic);
                                        g_ourBox.SelectionStart(static_cast<int32_t>(text.size()));
                                    }
                                    args.Handled(true);
                                }
                            } catch (...) {}
                        });
                    }

                    if (!g_activatedToken) {
                        g_activatedToken = core.Activated([](wuc::CoreWindow const&, wuc::WindowActivatedEventArgs const& args) {
                            if (args.WindowActivationState() != wuc::CoreWindowActivationState::Deactivated) {
                                g_suppressRefocus.store(false);
                                if (g_ourBox && !g_isOverlayVisible.load()) {
                                    g_ourBox.Text(L"");
                                }
                                if (g_resultsHost && !g_isOverlayVisible.load()) {
                                    g_resultsHost.Visibility(wux::Visibility::Visible);
                                    g_resultsHost.Opacity(0.0);
                                    g_resultsHost.IsHitTestVisible(false);
                                    if (g_resultsTranslate) g_resultsTranslate.Y(-8.0);
                                    SyncOverlayBackground();
                                }
                                TriggerMenuOpenFocus();
                            } else {
                                g_suppressRefocus.store(true);
                                if (g_resultsHost) {
                                    g_resultsHost.Visibility(wux::Visibility::Visible);
                                    g_resultsHost.Opacity(0.0);
                                    g_resultsHost.IsHitTestVisible(false);
                                    if (g_resultsTranslate) g_resultsTranslate.Y(-8.0);
                                }
                                if (g_ourBox) {
                                    g_ourBox.Text(L"");
                                }
                                g_isOverlayVisible.store(false);
                                g_isHiding.store(false);
                                if (g_hideAnim) g_hideAnim.Stop();
                                if (g_revealAnim) g_revealAnim.Stop();
                                if (g_openFocus) {
                                    g_openFocus.Stop();
                                    g_openFocus = nullptr;
                                }
                                if (g_refocus) {
                                    g_refocus.Stop();
                                    g_refocus = nullptr;
                                }
                            }
                        });
                    }
                    g_coreEventsHooked = true;
                }
            }
        } catch (...) {}
    }

    TriggerMenuOpenFocus();
} catch (...) {
    Rec(L"PlaceOurSearchBox error: %08X", static_cast<unsigned>(winrt::to_hresult()));
}

}  // namespace



// Undoes everything this mod put into StartMenuExperienceHost's UI.
//
// Must run on the XAML thread, and must run before Wh_ModUninit returns.
// Everything it releases is either a XAML object -- which throws
// RPC_E_WRONG_THREAD if touched from anywhere else -- or a callback whose code
// lives in this DLL, which Windhawk unmaps the moment uninit returns. Leaving
// one behind is not a leak, it is a crash on the next keystroke: XAML calls
// the handler, the handler is gone, and the process dies of 0xc000041d in
// Windows.UI.Xaml.dll.
void TeardownStartMenuUi() {
    Rec(L"teardown: enter (thread %lu)", GetCurrentThreadId());
    StopAttachWatch();
        // The handlers are code in this DLL, and the box is in somebody else's
    // tree; leaving either behind would be a crash on the next keystroke.
    if (g_focusProbe) {
        g_focusProbe.Stop();
        g_focusProbe = nullptr;
    }
    if (g_openFocus) {
        g_openFocus.Stop();
        g_openFocus = nullptr;
    }
    if (g_refocus) {
        g_refocus.Stop();
        g_refocus = nullptr;
    }
    Rec(L"teardown: timers stopped");
    if (g_appsList) {
        try {
            g_appsList.Children().Clear();
        } catch (...) {
        }
        g_appsList = nullptr;
    }
    if (g_resultsList) {
        try {
            g_resultsList.Children().Clear();
        } catch (...) {
        }
    }
    if (g_resultsHost) {
        try {
            g_resultsHost.Visibility(wux::Visibility::Collapsed);
        } catch (...) {
        }
    }
    if (g_revealAnim) {
        g_revealAnim.Stop();
        g_revealAnim = nullptr;
    }
    if (g_hideAnim) {
        g_hideAnim.Stop();
        g_hideAnim = nullptr;
    }
    g_resultsTranslate = nullptr;
    g_resultsList = nullptr;
    g_resultsHost = nullptr;
    g_activeApps.clear();
    g_appsHeaderHolder = nullptr;
    g_filesHeaderHolder = nullptr;

    // Our own elements come out of the shell's panels first. Clearing the
    // globals only dropped our references; the elements themselves were still
    // sitting in somebody else's tree.
    for (auto element : {g_resultsHost.try_as<wux::FrameworkElement>(),
                         g_ourBox.try_as<wux::FrameworkElement>()}) {
        if (!element) {
            continue;
        }
        try {
            auto parent = wuxm::VisualTreeHelper::GetParent(element);
            if (auto panel = parent ? parent.try_as<wuxc::Panel>() : nullptr) {
                uint32_t index = 0;
                if (panel.Children().IndexOf(element, index)) {
                    panel.Children().RemoveAt(index);
                }
            }
        } catch (...) {
        }
    }

    RestoreShellElements();
    Rec(L"teardown: lists and host released");

    try {
        if (auto window = wux::Window::Current()) {
            if (auto core = window.CoreWindow()) {
                if (g_charReceivedToken) {
                    core.CharacterReceived(g_charReceivedToken);
                    g_charReceivedToken = {};
                }
                if (g_keyDownToken) {
                    core.KeyDown(g_keyDownToken);
                    g_keyDownToken = {};
                }
                if (g_activatedToken) {
                    core.Activated(g_activatedToken);
                    g_activatedToken = {};
                }
            }
        }
    } catch (...) {}
    g_coreEventsHooked = false;
    Rec(L"teardown: CoreWindow events revoked");

    g_ourBoxChanged.revoke();
    g_ourBoxLost.revoke();
    g_ourBox = nullptr;
    Rec(L"teardown: box revoked");

    if (g_subclassed) {
        EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != GetCurrentProcessId()) return TRUE;
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
            if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
                RemoveWindowSubclass(hwnd, StartMenuSubclassProc, 101);
                return FALSE;
            }
            return TRUE;
        }, 0);
        g_subclassed = false;
    }

    Rec(L"teardown: subclass removed");
    if (g_hGetMsgHook) {
        UnhookWindowsHookEx(g_hGetMsgHook);
        g_hGetMsgHook = nullptr;
    }
    Rec(L"teardown: done");
}

// Lets Wh_ModUninit reach the XAML thread.
//
// SendMessage to a window owned by that thread runs the handler there and
// blocks until it has finished, which is the guarantee uninit needs: by the
// time it returns, nothing of ours is still attached. A posted message would
// only promise that the work had been queued.
UINT g_teardownMessage = 0;

UINT GetTeardownMessage() {
    if (!g_teardownMessage) {
        // A registered message rather than WM_APP+n: WM_APP values belong to
        // the window's own class, and this window is the shell's.
        g_teardownMessage =
            RegisterWindowMessageW(L"WindhawkStartEverythingTeardown");
    }
    return g_teardownMessage;
}

// ===========================================================================
// Mod entry points
// ===========================================================================

namespace {
[[clang::no_destroy]] std::thread g_uncloakWatchdog;
}  // namespace

BOOL Wh_ModInit() {
    Wh_Log(L">");
    g_targetProcess = IdentifyCurrentProcess();

    if (g_targetProcess == TargetProcess::Explorer) {
        InitExplorer();
        return TRUE;
    }
    if (g_targetProcess == TargetProcess::SearchHost) {
        InitSearchHost();
        return TRUE;
    }
    if (g_targetProcess == TargetProcess::StartMenu) {
        LoadSettings();
        return TRUE;
    }
    return FALSE;
}

void Wh_ModAfterInit() {
    Rec(L"=== attached to pid %lu (%ls) ===", GetCurrentProcessId(),
        g_targetProcess == TargetProcess::StartMenu ? L"StartMenu" :
        g_targetProcess == TargetProcess::SearchHost ? L"SearchHost" :
        g_targetProcess == TargetProcess::Explorer ? L"Explorer" : L"Unknown");

    if (g_targetProcess == TargetProcess::SearchHost) {
        StartSearchHostWatchdog();
        return;
    }

    if (g_targetProcess != TargetProcess::StartMenu) {
        return;
    }

    g_searchQuit.store(false);
    g_searchThread = std::thread(SearchThreadMain);

    StartAttachWatch();



    g_uncloakWatchdog = std::thread([] {
        bool wasCloaked = true;
        DWORD openTick = 0;
        while (!g_quit.load()) {
            HWND ours = GetOurCoreWindow();
            if (ours && IsWindow(ours)) {
                bool isCloaked = IsOurWindowCloaked();
                if (!isCloaked) {
                    HWND fg = GetForegroundWindow();
                    DWORD fgPid = 0;
                    if (fg) GetWindowThreadProcessId(fg, &fgPid);
                    DWORD now = GetTickCount();

                    if (wasCloaked) {
                        openTick = now;
                        Rec(L"watchdog: menu uncloaked! fg=%p pid=%lu ours=%p -> claiming foreground & focus",
                            fg, fgPid, ours);
                        g_suppressRefocus.store(false);
                        TakeForeground(true);
                        if (g_resultsHost) {
                            try {
                                g_resultsHost.Dispatcher().RunAsync(
                                    winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                                    []() {
                                        TriggerMenuOpenFocus();
                                    });
                            } catch (...) {}
                        }
                    } else if (fg != ours && fgPid != GetCurrentProcessId()) {
                        bool withinGrace = (now - openTick < 600);
                        bool isSearchOrNull = (fg == nullptr || IsProcessNamed(fgPid, L"SearchHost.exe"));
                        if (withinGrace || isSearchOrNull) {
                            Rec(L"watchdog: reclaiming foreground from %ls (fg=%p pid=%lu)",
                                isSearchOrNull ? L"SearchHost/NULL" : L"Other", fg, fgPid);
                            g_suppressRefocus.store(false);
                            TakeForeground(true);
                            if (g_resultsHost) {
                                try {
                                    g_resultsHost.Dispatcher().RunAsync(
                                        winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                                        []() {
                                            TriggerMenuOpenFocus();
                                        });
                                } catch (...) {}
                            }
                        }
                    }
                }
                wasCloaked = isCloaked;
            }
            Sleep(25);
        }
    });
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    Wh_Log(L">");
    if (g_targetProcess != TargetProcess::StartMenu) {
        *bReload = FALSE;
        return TRUE;
    }
    LoadSettings();
    if (g_resultsHost) {
        try {
            g_resultsHost.Dispatcher().RunAsync(
                winrt::Windows::UI::Core::CoreDispatcherPriority::Normal,
                []() {
                    if (g_footerHints) {
                        bool show = true;
                        {
                            std::lock_guard<std::mutex> lock(g_settingsMutex);
                            show = g_settings.showKeyHints;
                        }
                        g_footerHints.Visibility(show ? wux::Visibility::Visible : wux::Visibility::Collapsed);
                    }
                });
        } catch (...) {}
    }
    *bReload = FALSE;
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L">");

    g_quit.store(true);

    if (g_searchHostWatchdog.joinable()) {
        g_searchHostWatchdog.join();
    }

    // The watchdog subclasses every window in this process, twice a second
    // for a minute, and nothing ever removed them -- so in SearchHost the
    // same unload crash was waiting, with more windows to fire it. Joining
    // the watchdog first is what makes this safe: it cannot add another one
    // behind us.
    //
    // Removing a subclass is meant to happen on the thread that owns the
    // window. This does not, and neither did the SetWindowSubclass calls it
    // is undoing; matching them is still strictly better than leaving the
    // process pointing at an unmapped DLL.
    EnumWindows([](HWND hwnd, LPARAM) -> BOOL {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == GetCurrentProcessId()) {
            RemoveWindowSubclass(hwnd, SearchHostSubclassProc, 201);
        }
        return TRUE;
    }, 0);

    if (g_targetProcess != TargetProcess::StartMenu) {
        return;
    }

    g_searchQuit.store(true);
    g_queryWake.notify_all();
    if (g_searchThread.joinable()) {
        g_searchThread.join();
    }
    if (g_uncloakWatchdog.joinable()) {
        g_uncloakWatchdog.join();
    }
    WaitForTrackedLaunches();

    // The part that was missing, and the reason disabling this mod used to
    // take the Start menu down with it.
    //
    // Sent, not posted, and sent to a window the XAML thread owns, so the
    // work happens on that thread and has finished before this returns. The
    // timeout is a concession to a wedged shell: hanging Windhawk's unload
    // forever is worse than logging that the teardown did not land, and
    // SMTO_ABORTIFHUNG gets us out if the thread is already gone.
    Rec(L"uninit: start menu teardown beginning (hwnd %p)", g_hCoreWindow);
    bool tornDown = false;
    if (g_hCoreWindow && IsWindow(g_hCoreWindow)) {
        DWORD_PTR result = 0;
        if (SendMessageTimeoutW(g_hCoreWindow, GetTeardownMessage(), 0, 0,
                                SMTO_ABORTIFHUNG, 5000, &result)) {
            tornDown = true;
            Rec(L"uninit: teardown ran on the XAML thread");
        } else {
            Rec(L"uninit: teardown message timed out (%lu)", GetLastError());
        }
    }

    if (!tornDown) {
        // Last resort. Touching XAML from this thread can throw
        // RPC_E_WRONG_THREAD, but the alternative is returning with live
        // handlers in a DLL that is about to be unmapped, which is certain
        // rather than merely likely to crash.
        Rec(L"uninit: tearing down off-thread, which may throw");
        try {
            TeardownStartMenuUi();
        } catch (...) {
            Rec(L"uninit: off-thread teardown threw %08X",
                static_cast<unsigned>(winrt::to_hresult()));
        }
    }
    Rec(L"uninit: returning");
}
