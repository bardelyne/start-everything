// ==WindhawkMod==
// @id              start-everything
// @name            Everything results in the Start menu (work in progress)
// @description     Hides the Start menu's search box and reports what surrounds it, so results can be rendered in Start itself instead of the search host.
// @version         0.1
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         StartMenuExperienceHost.exe
// @architecture    x86-64
// @license         GPL-3.0
// @compilerOptions -loleaut32 -lruntimeobject
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Everything results in the Start menu

Work in progress, and currently a reconnaissance tool rather than a feature.

Typing in the Start menu hands off to `SearchHost.exe`, which runs its own
query, starts a WebView2 browser for web results, and completes what you type
against locally installed apps. Collapsing two elements stops that handoff
entirely -- `StartMenu.SearchBoxToggleButton` here and
`Cortana.UI.Views.RichSearchBoxControl` in the search host -- and then nothing
happens when you type, because the only real text box was the one in the
search host.

So the search box has to be replaced rather than merely removed, and it has to
be replaced *here*: when the handoff does not happen, the search host's page is
never shown, so anything drawn there would be invisible.

Why here and not there: `StartMenuExperienceHost` runs at medium integrity and
is not an AppContainer, so it can talk to Everything directly. The search host
cannot -- it is a low-integrity sandbox, which is why the existing panel needs
a separate broker process and a `WM_COPYDATA` bridge to be fed at all.

This build only hides the button and logs what is around it. Output goes to
`%TEMP%\start-everything.log`.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- hideSearchBox: false
  $name: Hide the Start menu's search box
  $description: >-
    Collapses StartMenu.SearchBoxToggleButton. On its own this leaves you
    unable to search at all, so it is off by default until there is a
    replacement box to put in its place.
- ownSearchBox: false
  $name: Put a real search box in its place
  $description: >-
    StartMenu.SearchBoxToggleButton only looks like a search box: underneath
    it is a Button holding a placeholder TextBlock and a Rectangle drawn to
    look like a caret. Nothing can be typed into it. Clicking or typing hands
    off to the search host, which owns the only real text box.

    This collapses it and puts an actual TextBox in the same grid cell, so
    there is something to type into without leaving the Start menu.
- dumpTree: true
  $name: Log the surrounding tree
  $description: >-
    Once per session, log the button's ancestors and the tree beneath the
    highest of them, to find where a search box of our own would go.
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
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>

#pragma pop_macro("GetCurrentTime")

#include <robuffer.h>
#include <roapi.h>
#include <windhawk_utils.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace wf = winrt::Windows::Foundation;
namespace wut = winrt::Windows::UI::Text;
namespace wuc = winrt::Windows::UI::Core;
namespace wux = winrt::Windows::UI::Xaml;
namespace wuxc = winrt::Windows::UI::Xaml::Controls;
namespace wuxm = winrt::Windows::UI::Xaml::Media;
namespace wuxmi = winrt::Windows::UI::Xaml::Media::Imaging;

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


struct Settings {
    bool hideSearchBox = false;
    bool ownSearchBox = false;
    bool dumpTree = true;
};

Settings g_settings;
std::atomic<DWORD> g_xamlThreadId{0};
std::atomic<bool> g_dumped{false};
std::atomic<int> g_seen{0};

void LoadSettings() {
    g_settings.hideSearchBox = Wh_GetIntSetting(L"hideSearchBox") != 0;
    g_settings.ownSearchBox = Wh_GetIntSetting(L"ownSearchBox") != 0;
    g_settings.dumpTree = Wh_GetIntSetting(L"dumpTree") != 0;
    Rec(L"=== settings: hideSearchBox=%d ownSearchBox=%d dumpTree=%d ===",
        g_settings.hideSearchBox ? 1 : 0, g_settings.ownSearchBox ? 1 : 0,
        g_settings.dumpTree ? 1 : 0);
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

void DumpTree(wux::DependencyObject const& root, int depth, int maxDepth) {
    if (depth > maxDepth) {
        return;
    }
    std::wstring indent(static_cast<size_t>(depth) * 2, L' ');
    double w = 0, h = 0;
    if (auto fe = root.try_as<wux::FrameworkElement>()) {
        w = fe.ActualWidth();
        h = fe.ActualHeight();
    }
    Rec(L"%ls%ls  [%.0fx%.0f]", indent.c_str(), ElementLabel(root).c_str(), w, h);

    int count = wuxm::VisualTreeHelper::GetChildrenCount(root);
    for (int i = 0; i < count; ++i) {
        DumpTree(wuxm::VisualTreeHelper::GetChild(root, i), depth + 1, maxDepth);
    }
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

[[clang::no_destroy]] wuxc::TextBox g_ourBox{nullptr};
[[clang::no_destroy]] wuxc::TextBox::TextChanged_revoker g_ourBoxChanged;
[[clang::no_destroy]] wux::UIElement::LostFocus_revoker g_ourBoxLost;

void PlaceOurSearchBox(wux::FrameworkElement const& stockButton) try {
    if (g_ourBox) {
        return;  // one per session; the menu is not rebuilt per opening
    }

    auto parent = wuxm::VisualTreeHelper::GetParent(stockButton);
    auto cell = parent ? parent.try_as<wuxc::Panel>() : nullptr;
    if (!cell) {
        Rec(L"own box: the stock button's parent is not a Panel; cannot place");
        return;
    }

    wuxc::TextBox box;
    box.Name(L"WindhawkStartSearchBox");
    box.PlaceholderText(L"Search with Everything");
    // Matched to the button it replaces: 768x32 inside a 772x64 cell.
    box.Width(stockButton.ActualWidth() > 0 ? stockButton.ActualWidth() : 768);
    box.Height(stockButton.ActualHeight() > 0 ? stockButton.ActualHeight() : 32);
    box.HorizontalAlignment(wux::HorizontalAlignment::Center);
    box.VerticalAlignment(wux::VerticalAlignment::Center);

    cell.Children().Append(box);
    g_ourBox = box;

    g_ourBoxChanged = box.TextChanged(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxc::TextChangedEventArgs const&) {
            try {
                auto b = sender.as<wuxc::TextBox>();
                Rec(L"own box: '%ls'", std::wstring{b.Text()}.c_str());
            } catch (...) {
            }
        });

    // Focus, when the menu becomes active.
    //
    // Waiting to be clicked was not enough: with the box unfocused, typing
    // went to the search host as before. Taking it here is not the same
    // gamble it was in the search host either -- there the host had its own
    // box and wanted focus back every half second, whereas here the only
    // other candidate, SearchBoxToggleButton, has been collapsed.
    //
    // Window::Current() may be null if this ever becomes a XAML island, as
    // the Control Center already is, so the result is logged rather than
    // assumed.
    try {
        auto window = wux::Window::Current();
        if (!window) {
            Rec(L"own box: no Window::Current() -- cannot hook activation");
        } else {
            window.Activated([](wf::IInspectable const&,
                                wuc::WindowActivatedEventArgs const& args) {
                if (args.WindowActivationState() ==
                    wuc::CoreWindowActivationState::Deactivated) {
                    return;
                }
                if (!g_ourBox) {
                    return;
                }
                bool took = g_ourBox.Focus(wux::FocusState::Programmatic);
                Rec(L"own box: menu activated, Focus() -> %d", took ? 1 : 0);
            });
            Rec(L"own box: watching window activation for focus");
        }
    } catch (...) {
        Rec(L"own box: activation hook threw %08X",
            static_cast<unsigned>(winrt::to_hresult()));
    }

    // Type anywhere in the menu and it goes to our box.
    //
    // Focus alone is not enough: click the menu's body and focus leaves the
    // box, and the next keystroke goes to the search host instead -- which is
    // how the stock menu behaves, since typing anywhere is meant to start a
    // search. Rather than fight to keep focus (that approach made the box
    // unusable when it was tried in the search host), take the characters at
    // the window, where they arrive before anything decides what to do with
    // them.
    //
    // Only when our box does not already have focus: when it does, the box
    // handles its own input and appending here would double every letter.
    try {
        if (auto window = wux::Window::Current()) {
            if (auto core = window.CoreWindow()) {
                core.CharacterReceived([](wuc::CoreWindow const&,
                                          wuc::CharacterReceivedEventArgs const&
                                              args) {
                    try {
                        if (!g_ourBox) {
                            return;
                        }
                        unsigned code = args.KeyCode();
                        // Printable only. Backspace, Esc, Tab and the rest
                        // have meanings in the menu that are not ours to take.
                        if (code < 0x20 || code == 0x7F) {
                            return;
                        }
                        auto focused =
                            wux::Input::FocusManager::GetFocusedElement();
                        auto ours = focused
                                        ? focused.try_as<wuxc::TextBox>()
                                        : nullptr;
                        if (ours && ours == g_ourBox) {
                            return;  // the box is already receiving this
                        }
                        std::wstring text{g_ourBox.Text()};
                        text.push_back(static_cast<wchar_t>(code));
                        g_ourBox.Text(text);
                        g_ourBox.Focus(wux::FocusState::Programmatic);
                        g_ourBox.SelectionStart(
                            static_cast<int32_t>(text.size()));
                        Rec(L"own box: took '%c' from the window", code);
                    } catch (...) {
                    }
                });
                Rec(L"own box: watching CharacterReceived");
            }
        }
    } catch (...) {
        Rec(L"own box: CharacterReceived hook threw %08X",
            static_cast<unsigned>(winrt::to_hresult()));
    }

    // And once now, for the opening that is already on screen.
    if (box.Focus(wux::FocusState::Programmatic)) {
        Rec(L"own box: took focus immediately");
    }

    g_ourBoxLost = box.LostFocus(
        winrt::auto_revoke,
        [](wf::IInspectable const&, wux::RoutedEventArgs const&) {
            Rec(L"own box: lost focus");
        });

    Rec(L"own box: placed in %ls (%.0fx%.0f)", ElementLabel(cell).c_str(),
        box.Width(), box.Height());
} catch (...) {
    Rec(L"own box: failed %08X", static_cast<unsigned>(winrt::to_hresult()));
}

}  // namespace

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
                auto service =
                    watcher->m_XamlDiagnostics.as<IVisualTreeService3>();
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
        HRESULT hr = m_XamlDiagnostics.as<IVisualTreeService3>()
                         ->UnadviseVisualTreeChange(this);
        if (FAILED(hr)) {
            Wh_Log(L"UnadviseVisualTreeChange failed: %08X", hr);
        }
    }

   private:
    HRESULT STDMETHODCALLTYPE
    OnVisualTreeChange(ParentChildRelation, VisualElement element,
                       VisualMutationType mutationType) override try {
        if (mutationType != Add || !element.Type) {
            return S_OK;
        }

        // Two names, both given by UWPSpy, and both have to go for the
        // handoff to stop: StartMenu.SearchBoxToggleButton here, and
        // Cortana.UI.Views.RichSearchBoxControl over in SearchHost. Hiding
        // only one leaves a way back into the search host.
        if (wcscmp(element.Type, L"StartMenu.SearchBoxToggleButton") != 0) {
            return S_OK;
        }

        wf::IInspectable obj;
        if (FAILED(m_XamlDiagnostics->GetIInspectableFromHandle(
                element.Handle,
                reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))))) {
            return S_OK;
        }
        auto button = obj.try_as<wux::FrameworkElement>();
        if (!button) {
            return S_OK;
        }

        g_xamlThreadId.store(GetCurrentThreadId());
        int n = ++g_seen;
        Rec(L"SearchBoxToggleButton #%d seen (%.0fx%.0f)", n,
            button.ActualWidth(), button.ActualHeight());

        if (g_settings.hideSearchBox || g_settings.ownSearchBox) {
            button.Visibility(wux::Visibility::Collapsed);
            Rec(L"  collapsed it");
        }
        if (g_settings.ownSearchBox) {
            PlaceOurSearchBox(button);
        }

        // What surrounds it, once, so we know where a box of ours could go
        // and what it would have to look like. Walking up to the menu root
        // rather than down: the button is a leaf, and the question is what
        // container owns the space it occupies.
        if (g_settings.dumpTree && !g_dumped.exchange(true)) {
            wux::DependencyObject node = button;
            wux::DependencyObject root = button;
            for (int up = 0; up < 8; ++up) {
                auto parent = wuxm::VisualTreeHelper::GetParent(node);
                if (!parent) {
                    break;
                }
                Rec(L"  parent %d: %ls", up + 1, ElementLabel(parent).c_str());
                root = parent;
                node = parent;
            }
            Rec(L"--- tree from %ls ---", ElementLabel(root).c_str());
            DumpTree(root, 0, 12);
            Rec(L"--- ends ---");
        }

        return S_OK;
    } catch (...) {
        Rec(L"OnVisualTreeChange error: %08X",
            static_cast<unsigned>(winrt::to_hresult()));
        return S_OK;
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
    0x3a7e14c9, 0x8d52, 0x4f06,
    {0xb1, 0x9a, 0x2c, 0x74, 0xe8, 0x35, 0x9d, 0x60}};

class WindhawkTAP : public winrt::implements<WindhawkTAP, IObjectWithSite,
                                             winrt::non_agile> {
   public:
    HRESULT STDMETHODCALLTYPE SetSite(IUnknown* pUnkSite) override try {
        // The handlers are code in this DLL, and the box is in somebody else's
    // tree; leaving either behind would be a crash on the next keystroke.
    g_ourBoxChanged.revoke();
    g_ourBoxLost.revoke();
    g_ourBox = nullptr;

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

    HRESULT STDMETHODCALLTYPE GetSite(REFIID riid,
                                      void** ppvSite) noexcept override {
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

    HRESULT STDMETHODCALLTYPE LockServer(BOOL) noexcept override {
        return S_OK;
    }
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

using PFN_INITIALIZE_XAML_DIAGNOSTICS_EX =
    decltype(&InitializeXamlDiagnosticsEx);

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

    const HMODULE wuxDll = LoadLibraryEx(L"Windows.UI.Xaml.dll", nullptr,
                                         LOAD_LIBRARY_SEARCH_SYSTEM32);
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

namespace {
std::atomic<bool> g_tapInjected{false};
[[clang::no_destroy]] std::thread g_tapThread;
std::atomic<bool> g_quit{false};

// The same trap as in the other two mods: calling InitializeXamlDiagnosticsEx
// before a XAML window exists fails every attempt and can leave the host
// unable to continue. Wait for one.
bool XamlWindowExists() {
    bool found = false;
    EnumWindows(
        [](HWND hwnd, LPARAM param) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(hwnd, &pid);
            if (pid != GetCurrentProcessId()) {
                return TRUE;
            }
            wchar_t cls[128] = {};
            GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
            if (wcsstr(cls, L"Windows.UI.Core.CoreWindow")) {
                *reinterpret_cast<bool*>(param) = true;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&found));
    return found;
}
}  // namespace

BOOL Wh_ModInit() {
    Wh_Log(L">");
    LoadSettings();
    return TRUE;
}

void Wh_ModAfterInit() {
    Rec(L"=== attached to pid %lu ===", GetCurrentProcessId());
    g_quit.store(false);
    g_tapThread = std::thread([] {
        for (int attempt = 0; attempt < 120 && !g_quit.load(); ++attempt) {
            if (XamlWindowExists()) {
                HRESULT hr = InjectWindhawkTAP();
                Rec(L"tap attempt %d -> %08X", attempt + 1,
                    static_cast<unsigned>(hr));
                if (SUCCEEDED(hr)) {
                    g_tapInjected.store(true);
                    return;
                }
            }
            Sleep(500);
        }
        Rec(L"tap: gave up");
    });
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    Wh_Log(L">");
    bool wasHiding = g_settings.hideSearchBox;
    LoadSettings();
    // Un-hiding has to rebuild the tree, so ask for a reload either way.
    *bReload = (wasHiding != g_settings.hideSearchBox);
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L">");
    g_quit.store(true);
    if (g_tapThread.joinable()) {
        g_tapThread.join();
    }
    if (g_visualTreeWatcher) {
        g_visualTreeWatcher->UnadviseVisualTreeChange();
        g_visualTreeWatcher = nullptr;
    }
}
