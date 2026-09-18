// ==WindhawkMod==
// @id              takeover
// @name            Search takeover test (temporary)
// @description     Hides the results web view and puts our own layout in its place. Reversible; diagnostic only.
// @version         0.1
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         SearchHost.exe
// @architecture    x86-64
// @license         GPL-3.0
// @compilerOptions -lole32 -loleaut32 -lruntimeobject
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Search reconnaissance

Temporary diagnostic mod. It attaches the XAML diagnostics TAP to the search
UI and records every element added to the visual tree, so the structure can be
studied. It injects nothing and changes no behaviour.

Output goes to `%TEMP%\takeover.log`.
*/
// ==/WindhawkModReadme==

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
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>

#pragma pop_macro("GetCurrentTime")

#include <roapi.h>
#include <windhawk_utils.h>

#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wf = winrt::Windows::Foundation;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxm = winrt::Windows::UI::Xaml::Media;

#include <set>
#include <thread>
#include <atomic>

static HRESULT InjectWindhawkTAP() noexcept;

namespace {

// Append a line to %TEMP%\takeover.log. The host owns the debugger channel
// and DbgView needs a human watching it; a file can be read afterwards.
void Rec(const wchar_t* fmt, ...) {
    wchar_t body[2048] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, fmt, args);
    va_end(args);

    wchar_t path[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, path);
    if (!n || n > MAX_PATH - 32) {
        return;
    }
    wcscat_s(path, MAX_PATH, L"takeover.log");
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    static const wchar_t kCrLf[] = {13, 10, 0};
    wchar_t line[2200];
    int len = wsprintfW(line, L"%ls%ls", body, kCrLf);
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(h);
}

std::atomic<DWORD> g_xamlThreadId{0};

// Strong refs, not weak: we must be able to put the tree back on unload, and
// a weak ref to a non-UIElement can already be dead by then.
[[clang::no_destroy]] wuxc::Grid g_webViewGrid{nullptr};
[[clang::no_destroy]] wuxc::Border g_panel{nullptr};
[[clang::no_destroy]] wuxc::Grid g_rootGrid{nullptr};
wux::Thickness g_originalMargin{};
bool g_injected = false;
HWND g_flyout = nullptr;
[[clang::no_destroy]] wux::UIElement g_webHost{nullptr};
[[clang::no_destroy]] wuxc::Grid g_takeover{nullptr};
[[clang::no_destroy]] wuxc::Panel g_parentPanel{nullptr};
RECT g_originalRect{};

constexpr double kPanelWidth = 330;

// The flyout's own top-level window. Widening it buys space without squeezing
// Microsoft's results into a narrower layout.
HWND FindFlyoutWindow() {
    struct Ctx { DWORD pid; HWND best; } ctx{GetCurrentProcessId(), nullptr};
    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != c->pid) return TRUE;
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
            if (!wcsstr(cls, L"Windows.UI.Core.CoreWindow")) return TRUE;
            RECT r{};
            GetWindowRect(hwnd, &r);
            int w = r.right - r.left, h = r.bottom - r.top;
            // The flyout, not a full-screen surface.
            if (w > 400 && w < 1400 && h > 400) {
                c->best = hwnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));
    return ctx.best;
}
std::mutex g_seenMutex;
std::set<std::wstring> g_seenTypes;
winrt::weak_ref<wux::FrameworkElement> g_searchPage;
std::atomic<bool> g_dumpedResults{false};

HMODULE GetCurrentModuleHandle() {
    HMODULE module;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           L"", &module)) {
        return nullptr;
    }
    return module;
}

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

void DumpTree(wux::DependencyObject const& root, int depth, int maxDepth) {
    if (depth > maxDepth) {
        return;
    }

    std::wstring indent(static_cast<size_t>(depth) * 2, L' ');
    std::wstring extra;
    if (auto fe = root.try_as<wux::FrameworkElement>()) {
        wchar_t buf[128];
        swprintf(buf, 128, L"  [row=%d col=%d w=%.0f h=%.0f]",
                 wuxc::Grid::GetRow(fe), wuxc::Grid::GetColumn(fe),
                 fe.ActualWidth(), fe.ActualHeight());
        extra = buf;
    }
    std::wstring geom;
    if (auto fe = root.try_as<wux::FrameworkElement>()) {
        wchar_t g[128];
        wsprintfW(g, L"   [%dx%d]", static_cast<int>(fe.ActualWidth()),
                  static_cast<int>(fe.ActualHeight()));
        geom = g;
    }
    Rec(L"%ls%ls%ls%ls", indent.c_str(), ElementLabel(root).c_str(),
        extra.c_str(), geom.c_str());

    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        DumpTree(wuxm::VisualTreeHelper::GetChild(root, i), depth + 1, maxDepth);
    }
}

wux::DependencyObject FindDescendant(wux::DependencyObject const& root,
                                     std::wstring_view label, int maxDepth) {
    if (maxDepth < 0) {
        return nullptr;
    }
    if (ElementLabel(root) == label) {
        return root;
    }
    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        auto child = wuxm::VisualTreeHelper::GetChild(root, i);
        if (auto found = FindDescendant(child, label, maxDepth - 1)) {
            return found;
        }
    }
    return nullptr;
}

wux::DependencyObject FindDescendantByName(wux::DependencyObject const& root,
                                           std::wstring_view name, int maxDepth) {
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


bool XamlWindowExists() {
    bool found = false;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != GetCurrentProcessId()) {
                return TRUE;
            }
            wchar_t className[128] = {};
            GetClassNameW(hwnd, className, ARRAYSIZE(className));
            if (wcsstr(className, L"Windows.UI.Core.CoreWindow") ||
                wcsstr(className, L"ControlCenter")) {
                *reinterpret_cast<bool*>(param) = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&found));
    return found;
}

std::atomic<bool> g_tapInjected{false};

// Owns the mod's Win32 listening. Two jobs, both needing a message loop:
//   * WM_DISPLAYCHANGE, which is broadcast to top-level windows only, so a
//     message-only window would never see it -- hence a real invisible popup.
//   * a WinEvent hook for windows being shown, which is how we learn the
//     flyout was opened. XamlRoot.Changed looked like the right signal but

}  // namespace

// ===========================================================================
// XAML diagnostics plumbing (adapted from m417z's Notification Center Styler)
// ===========================================================================

class VisualTreeWatcher
    : public winrt::implements<VisualTreeWatcher, IVisualTreeServiceCallback2,
                               winrt::non_agile> {
   public:
    explicit VisualTreeWatcher(winrt::com_ptr<IUnknown> site)
        : m_XamlDiagnostics(site.as<IXamlDiagnostics>()) {
        Wh_Log(L"Constructing VisualTreeWatcher");

        // Calling AdviseVisualTreeChange on this thread can hang the app in
        // Advising::RunOnUIThread; doing it from a fresh thread avoids that.
        HANDLE thread = CreateThread(
            nullptr, 0,
            [](LPVOID param) -> DWORD {
                auto* watcher = reinterpret_cast<VisualTreeWatcher*>(param);
                auto service = watcher->m_XamlDiagnostics.as<IVisualTreeService3>();
                HRESULT hr = service->AdviseVisualTreeChange(watcher);
                watcher->Release();
                if (FAILED(hr)) {
                    Wh_Log(L"AdviseVisualTreeChange failed: %08X", hr);
                }
                return 0;
            },
            this, 0, nullptr);
        if (thread) {
            AddRef();
            CloseHandle(thread);
        }
    }

    VisualTreeWatcher(const VisualTreeWatcher&) = delete;
    VisualTreeWatcher& operator=(const VisualTreeWatcher&) = delete;

    void UnadviseVisualTreeChange() {
        HRESULT hr =
            m_XamlDiagnostics.as<IVisualTreeService3>()->UnadviseVisualTreeChange(this);
        if (FAILED(hr)) {
            Wh_Log(L"UnadviseVisualTreeChange failed: %08X", hr);
        }
    }

   private:
    HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,
                                                 VisualElement element,
                                                 VisualMutationType mutationType) override try {
        if (mutationType != Add || !element.Type) {
            return S_OK;
        }
        if (wcscmp(element.Type, L"Cortana.UI.Views.TaskbarSearchPage") != 0) {
            return S_OK;
        }
        Rec(L"TaskbarSearchPage added");

        wf::IInspectable obj;
        winrt::check_hresult(m_XamlDiagnostics->GetIInspectableFromHandle(
            element.Handle,
            reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));
        auto page = obj.try_as<wux::FrameworkElement>();
        if (!page) {
            return S_OK;
        }
        g_xamlThreadId.store(GetCurrentThreadId());

        auto rootObj = FindDescendantByName(page, L"RootGrid", 8);
        auto root = rootObj ? rootObj.try_as<wuxc::Grid>() : nullptr;
        if (!root) {
            Rec(L"FAIL: no Grid#RootGrid");
            return S_OK;
        }

        // The whole results surface is one web view; its XAML container is
        // what we can actually move.
        auto wvObj = FindDescendantByName(page, L"WebViewGrid", 20);
        auto wv = wvObj ? wvObj.try_as<wuxc::Grid>() : nullptr;
        if (!wv) {
            Rec(L"FAIL: no Grid#WebViewGrid - results area not found");
            return S_OK;
        }
        Rec(L"found WebViewGrid  %.0fx%.0f", wv.ActualWidth(), wv.ActualHeight());

        if (g_injected) {
            return S_OK;
        }

        // Do NOT measure or resize here. At this point the page exists but has
        // not been laid out: WebViewGrid reports 0x0 and the window is still
        // at a default size. Acting now is what invalidated the earlier runs.
        // Wait for a layout pass where the real sizes are available.
        auto pageRef = page;
        auto rootRef = root;
        auto wvRef = wv;
        auto token = std::make_shared<wux::FrameworkElement::LayoutUpdated_revoker>();
        *token = page.LayoutUpdated(
            winrt::auto_revoke,
            [pageRef, rootRef, token](wf::IInspectable const&,
                                      wf::IInspectable const&) {
                if (g_injected) {
                    return;
                }

                // Path confirmed with UWPSpy:
                //   RootGrid > QueryFormulationRoot > QueryFormulation > Grid
                //     > HostedWebView2Control > WebViewGrid > WebView2
                // Hiding the host takes the entire results surface out of the
                // layout, so nothing is asked to reflow into a narrower space
                // -- which is what collapsed their list before.
                auto hostObj = FindDescendantByName(
                    pageRef, L"QueryFormulationHostedWebView2", 20);
                auto host = hostObj ? hostObj.try_as<wux::FrameworkElement>()
                                    : nullptr;
                if (!host || host.ActualWidth() < 100) {
                    return;  // not laid out yet
                }

                auto parent = wuxm::VisualTreeHelper::GetParent(host)
                                  .try_as<wuxc::Panel>();
                if (!parent) {
                    Rec(L"FAIL: the web view host's parent is not a Panel");
                    return;
                }

                Rec(L"layout ready: host %.0fx%.0f", host.ActualWidth(),
                    host.ActualHeight());

                try {
                    g_webHost = host;
                    host.Visibility(wux::Visibility::Collapsed);

                    wuxc::Grid split;
                    split.Name(L"WindhawkTakeoverPanel");
                    split.HorizontalAlignment(wux::HorizontalAlignment::Stretch);
                    split.VerticalAlignment(wux::VerticalAlignment::Stretch);
                    wuxc::ColumnDefinition left, right;
                    left.Width(wux::GridLengthHelper::FromValueAndType(
                        1, wux::GridUnitType::Star));
                    right.Width(wux::GridLengthHelper::FromValueAndType(
                        1, wux::GridUnitType::Star));
                    split.ColumnDefinitions().Append(left);
                    split.ColumnDefinitions().Append(right);

                    wuxc::TextBlock apps;
                    apps.Text(L"Apps");
                    apps.FontSize(15);
                    apps.Margin(wux::ThicknessHelper::FromLengths(24, 24, 24, 24));
                    wuxc::Grid::SetColumn(apps, 0);
                    split.Children().Append(apps);

                    wuxc::TextBlock files;
                    files.Text(L"Files  -  from Everything");
                    files.FontSize(15);
                    files.Margin(wux::ThicknessHelper::FromLengths(24, 24, 24, 24));
                    wuxc::Grid::SetColumn(files, 1);
                    split.Children().Append(files);

                    parent.Children().Append(split);
                    g_panel = nullptr;
                    g_takeover = split;
                    g_parentPanel = parent;
                    g_injected = true;
                    Rec(L"SUCCESS: web view hidden, two-column layout in its place");
                } catch (...) {
                    Rec(L"FAIL during apply: %08X",
                        static_cast<unsigned>(winrt::to_hresult()));
                }
                token->revoke();
            });

        return S_OK;
    } catch (...) {
        Wh_Log(L"OnVisualTreeChange error: %08X", winrt::to_hresult());
        return S_OK;  // never fail the shell's callback
    }

    HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,
                                                    VisualElementState,
                                                    LPCWSTR) noexcept override {
        return S_OK;
    }

    winrt::com_ptr<IXamlDiagnostics> m_XamlDiagnostics = nullptr;
};

namespace {
[[clang::no_destroy]] winrt::com_ptr<VisualTreeWatcher> g_visualTreeWatcher;
}

// {C85D8CC7-5463-40E8-A432-F5916B6427E5}
static constexpr CLSID CLSID_WindhawkTAP = {
    0xc85d8cc7, 0x5463, 0x40e8, {0xa4, 0x32, 0xf5, 0x91, 0x6b, 0x64, 0x27, 0xe5}};

class WindhawkTAP : public winrt::implements<WindhawkTAP, IObjectWithSite,
                                             winrt::non_agile> {
   public:
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* pUnkSite) override try {
        if (g_visualTreeWatcher) {
            g_visualTreeWatcher->UnadviseVisualTreeChange();
            g_visualTreeWatcher = nullptr;
        }

        site.copy_from(pUnkSite);

        if (site) {
            // Balance the refcount taken by InitializeXamlDiagnosticsEx.
            FreeLibrary(GetCurrentModuleHandle());
            g_visualTreeWatcher = winrt::make_self<VisualTreeWatcher>(site);
        }

        return S_OK;
    } catch (...) {
        HRESULT hr = winrt::to_hresult();
        Wh_Log(L"SetSite error: %08X", hr);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE GetSite(REFIID riid, void** ppvSite) noexcept override {
        return site.as(riid, ppvSite);
    }

   private:
    winrt::com_ptr<IUnknown> site;
};

template <class T>
struct SimpleFactory
    : winrt::implements<SimpleFactory<T>, IClassFactory, winrt::non_agile> {
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* pUnkOuter, REFIID riid,
                                             void** ppvObject) override try {
        if (pUnkOuter) {
            return CLASS_E_NOAGGREGATION;
        }
        *ppvObject = nullptr;
        return winrt::make<T>().as(riid, ppvObject);
    } catch (...) {
        return winrt::to_hresult();
    }

    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override { return S_OK; }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdll-attribute-on-redeclaration"

__declspec(dllexport) _Use_decl_annotations_ STDAPI
    DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) try {
    if (rclsid == CLSID_WindhawkTAP) {
        *ppv = nullptr;
        return winrt::make<SimpleFactory<WindhawkTAP>>().as(riid, ppv);
    }
    return CLASS_E_CLASSNOTAVAILABLE;
} catch (...) {
    return winrt::to_hresult();
}

__declspec(dllexport) _Use_decl_annotations_ STDAPI DllCanUnloadNow() {
    return winrt::get_module_lock() ? S_FALSE : S_OK;
}

#pragma clang diagnostic pop

using PFN_INITIALIZE_XAML_DIAGNOSTICS_EX = decltype(&InitializeXamlDiagnosticsEx);

static HRESULT InjectWindhawkTAP() noexcept {
    HMODULE module = GetCurrentModuleHandle();
    if (!module) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    WCHAR location[MAX_PATH];
    switch (GetModuleFileName(module, location, ARRAYSIZE(location))) {
        case 0:
        case ARRAYSIZE(location):
            return HRESULT_FROM_WIN32(GetLastError());
    }

    const HMODULE wuxDll =
        LoadLibraryEx(L"Windows.UI.Xaml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!wuxDll) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const auto ixde = reinterpret_cast<PFN_INITIALIZE_XAML_DIAGNOSTICS_EX>(
        GetProcAddress(wuxDll, "InitializeXamlDiagnosticsEx"));
    if (!ixde) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // There is no way to know which diagnostics slot is free, so walk the
    // connection names until one takes. ERROR_NOT_FOUND means "that name is
    // not available", i.e. keep going -- which is the entire reason the loop
    // exists. Anything else, success or a real error, ends it.
    //
    // Getting this backwards is not academic: the Notification Center Styler
    // targets ShellHost.exe as well, so if it holds slot 1 and we stop there,
    // one of the two mods silently does nothing.
    HRESULT hr = E_FAIL;
    for (int i = 0; i < 64; i++) {
        WCHAR connectionName[256];
        wsprintf(connectionName, L"VisualDiagConnection%d", i + 1);

        hr = ixde(connectionName, GetCurrentProcessId(), L"", location,
                  CLSID_WindhawkTAP, nullptr);
        if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
            break;
        }
    }

    return hr;
}


// ===========================================================================
// Mod entry points
// ===========================================================================

// Retry until the XAML runtime exists. Calling InjectWindhawkTAP before then
// returns ERROR_NOT_FOUND, which is what happens whenever the host has just
// restarted -- exactly the case here, since installing the mod restarts it.
[[clang::no_destroy]] std::thread g_tapThread;
std::atomic<bool> g_tapQuit{false};

void Wh_ModAfterInit() {
    Wh_Log(L">");
    Rec(L"=== takeover attached, pid=%lu ===", GetCurrentProcessId());
    g_tapQuit.store(false);
    g_tapThread = std::thread([] {
        try {
            for (int attempt = 0; attempt < 120 && !g_tapQuit.load(); attempt++) {
                if (XamlWindowExists()) {
                    HRESULT hr = InjectWindhawkTAP();
                    Rec(L"InjectWindhawkTAP (attempt %d) -> %08X", attempt + 1,
                        static_cast<unsigned>(hr));
                    if (SUCCEEDED(hr)) {
                        Rec(L"TAP attached - now open search to trigger the test");
                        return;
                    }
                }
                Sleep(500);
            }
            Rec(L"gave up waiting for XAML");
        } catch (...) {
            Rec(L"TAP thread threw");
        }
    });
}

BOOL Wh_ModInit() {
    Wh_Log(L">");
    return TRUE;
}

void RemoveInjection() {
    try {
        if (g_webHost) {
            g_webHost.Visibility(wux::Visibility::Visible);
            Rec(L"restored the results web view");
            g_webHost = nullptr;
        }
        if (g_takeover && g_parentPanel) {
            uint32_t i = 0;
            if (g_parentPanel.Children().IndexOf(g_takeover, i)) {
                g_parentPanel.Children().RemoveAt(i);
                Rec(L"removed our layout");
            }
            g_takeover = nullptr;
            g_parentPanel = nullptr;
        }
        if (g_flyout) {
            SetWindowPos(g_flyout, nullptr, 0, 0,
                         g_originalRect.right - g_originalRect.left,
                         g_originalRect.bottom - g_originalRect.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            Rec(L"restored flyout width");
            g_flyout = nullptr;
        }
        if (g_panel && g_rootGrid) {
            uint32_t index = 0;
            if (g_rootGrid.Children().IndexOf(g_panel, index)) {
                g_rootGrid.Children().RemoveAt(index);
                Rec(L"removed our panel");
            }
        }
    } catch (...) {
        Rec(L"teardown threw %08X", static_cast<unsigned>(winrt::to_hresult()));
    }
    g_panel = nullptr;
    g_webViewGrid = nullptr;
    g_rootGrid = nullptr;
    g_injected = false;
}

void Wh_ModUninit() {
    Wh_Log(L">");
    g_tapQuit.store(true);
    if (g_tapThread.joinable()) {
        g_tapThread.join();
    }
    if (g_visualTreeWatcher) {
        g_visualTreeWatcher->UnadviseVisualTreeChange();
        g_visualTreeWatcher = nullptr;
    }
    // Must happen on the thread that owns the tree, and before we return --
    // Windhawk frees this image the moment we do.
    DWORD xamlThread = g_xamlThreadId.load();
    if (xamlThread == GetCurrentThreadId()) {
        RemoveInjection();
    } else if (xamlThread) {
        Rec(L"uninit on the wrong thread; leaving the tree alone");
    }
    if (g_visualTreeWatcher) {
        g_visualTreeWatcher->UnadviseVisualTreeChange();
        g_visualTreeWatcher = nullptr;
    }
    Rec(L"=== recon detached ===");
}
