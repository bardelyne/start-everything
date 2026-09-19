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

    This keeps it -- the border and the magnifier are what make the Start menu
    look like the Start menu -- and only takes away its click, which is what
    hands off to the search host. A real, transparent TextBox goes on top, so
    what you see is Microsoft's chrome with our text in it.
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
#include "broker/everything_ipc.h"
#include "broker/file_ranker.h"

#include <winrt/Windows.System.h>
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
#include <condition_variable>
#include <mutex>
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
[[clang::no_destroy]] wuxc::TextBox::TextChanged_revoker g_ourBoxChanged;
[[clang::no_destroy]] wux::UIElement::LostFocus_revoker g_ourBoxLost;
[[clang::no_destroy]] wux::DispatcherTimer g_focusProbe{nullptr};
[[clang::no_destroy]] wux::DispatcherTimer g_openFocus{nullptr};
[[clang::no_destroy]] wux::DispatcherTimer g_refocus{nullptr};

// Makes the Start menu's own window the active one.
//
// SearchHost takes the foreground about 140ms after Win is pressed and holds
// it, even with its search box made inert. Everything inside Start is then
// happening in an inactive window: XAML focus can be set and reported, but no
// caret is drawn, which is exactly what was on screen.
//
// AttachThreadInput first, because SetForegroundWindow is refused for a
// process that does not own the foreground -- the standard way round it is to
// share input state with the thread that does, briefly, and detach again.
// If it is refused anyway, that is logged rather than retried: a foreground
// fight with the shell is not something to win by persistence.
void TakeForeground() {
    // Bounded on purpose. If SearchHost insists on the foreground, this should
    // give up and say so rather than trade activations with it forever.
    static int attempts = 0;
    if (++attempts > 60) {
        return;
    }
    try {
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
        if (!ours) {
            Rec(L"foreground: no CoreWindow of ours to activate");
            return;
        }

        HWND current = GetForegroundWindow();
        if (current == ours) {
            static bool said = false;
            if (!said) {
                said = true;
                Rec(L"foreground: already ours");
            }
            return;
        }

        DWORD theirThread = GetWindowThreadProcessId(current, nullptr);
        DWORD ourThread = GetCurrentThreadId();
        bool attached = false;
        if (theirThread && theirThread != ourThread) {
            attached = AttachThreadInput(ourThread, theirThread, TRUE) != FALSE;
        }
        BOOL ok = SetForegroundWindow(ours);
        if (attached) {
            AttachThreadInput(ourThread, theirThread, FALSE);
        }
        Rec(L"foreground: SetForegroundWindow -> %d (attached=%d)", ok ? 1 : 0,
            attached ? 1 : 0);
    } catch (...) {
    }
}

// Brings our window back a tick later.
//
// A tick, because doing it inside the focus change that is still in progress
// is overridden immediately -- the same reason the first LostFocus attempt
// failed.
void RestoreWindowSoon() {
    try {
        auto back = wux::DispatcherTimer();
        back.Interval(std::chrono::milliseconds(60));
        back.Tick([back](wf::IInspectable const&, wf::IInspectable const&) {
            back.Stop();
            TakeForeground();
            if (g_ourBox) {
                auto now = wux::Input::FocusManager::GetFocusedElement();
                // A click on something real should win; only take the element
                // back when nothing else has it.
                if (!now || now.try_as<wuxc::ScrollViewer>()) {
                    g_ourBox.Focus(wux::FocusState::Programmatic);
                }
            }
        });
        back.Start();
        g_refocus = back;
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

[[clang::no_destroy]] std::thread g_searchThread;
[[clang::no_destroy]] std::mutex g_queryMutex;
[[clang::no_destroy]] std::condition_variable g_queryWake;
[[clang::no_destroy]] std::wstring g_pendingQuery;
std::atomic<bool> g_searchQuit{false};
std::atomic<bool> g_queryDirty{false};

// Results, handed from the search thread to the XAML thread.
[[clang::no_destroy]] std::mutex g_resultsMutex;
[[clang::no_destroy]] std::vector<everything::Result> g_results;
std::atomic<DWORD> g_totalMatches{0};

void QueueQuery(std::wstring text) {
    {
        std::lock_guard<std::mutex> lock(g_queryMutex);
        g_pendingQuery = std::move(text);
    }
    g_queryDirty.store(true);
    g_queryWake.notify_all();
}

void SearchThreadMain() {
    everything::Client client;
    if (!client.Init()) {
        Rec(L"search: could not create the reply window");
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
                return g_queryDirty.load() || g_searchQuit.load();
            });
            if (g_searchQuit.load()) {
                return;
            }
            if (!g_queryDirty.exchange(false)) {
                continue;
            }
            query = g_pendingQuery;
        }

        // Settle: if more was typed while we waited, search the newer text
        // rather than the older.
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        {
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
            std::lock_guard<std::mutex> lock(g_resultsMutex);
            g_results.clear();
            g_totalMatches.store(0);
            continue;
        }

        std::vector<everything::Result> pool;
        DWORD total = 0;
        auto start = std::chrono::steady_clock::now();
        bool ok = client.Query(query, ranker::kDefaultPool, &pool, &total);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - start)
                      .count();
        if (!ok) {
            Rec(L"search: '%ls' failed (Everything running?)", query.c_str());
            continue;
        }

        ranker::Rank(&pool, query, 12);
        {
            std::lock_guard<std::mutex> lock(g_resultsMutex);
            g_results = pool;
            g_totalMatches.store(total);
        }
        Rec(L"search: '%ls' -> %u matches, kept %zu (%lld ms)", query.c_str(),
            total, pool.size(), static_cast<long long>(ms));
        for (size_t i = 0; i < pool.size() && i < 5; ++i) {
            Rec(L"    %ls  %ls", pool[i].name.c_str(), pool[i].path.c_str());
        }
    }
}

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
    box.FontSize(14);
    // Matched to the button it replaces: 768x32 inside a 772x64 cell.
    box.Width(stockButton.ActualWidth() > 0 ? stockButton.ActualWidth() : 768);
    box.Height(stockButton.ActualHeight() > 0 ? stockButton.ActualHeight() : 32);
    box.HorizontalAlignment(wux::HorizontalAlignment::Center);
    box.VerticalAlignment(wux::VerticalAlignment::Center);

    // Transparent, borderless, and appended after the stock button so it is
    // drawn over it. The chrome underneath is Microsoft's; only the text and
    // the caret are ours, which is why this looks native rather than like a
    // control bolted on.
    try {
        wuxm::SolidColorBrush clear{winrt::Windows::UI::Colors::Transparent()};
        box.Background(clear);
        box.BorderThickness(wux::ThicknessHelper::FromUniformLength(0));
        // The stock box indents its text past the magnifier; match it so the
        // caret does not sit on top of the icon.
        // Top padding rather than alignment: the glyphs sit high in the
        // line box, so centring the line still leaves the text looking
        // high against Microsoft's chrome.
        box.Padding(wux::ThicknessHelper::FromLengths(40, 6, 8, 0));
        // A TextBox centres its text by VerticalContentAlignment, not by
        // padding, and its default MinHeight is taller than the 32px box we
        // are sitting in -- which is what pushed the text off centre.
        box.VerticalContentAlignment(wux::VerticalAlignment::Center);
        box.VerticalAlignment(wux::VerticalAlignment::Stretch);
        box.MinHeight(0);

        // Setting Background is not enough. A TextBox paints its own
        // background and border from theme brushes chosen by visual state, so
        // resting looks clear while hover and focus put a lighter slab back
        // over Microsoft's chrome -- which is what showed up on screen. These
        // are the brushes that template reads; overriding them on the element
        // covers every state.
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
                                   wuxm::SolidColorBrush{
                                       winrt::Windows::UI::Colors::Transparent()});
        }
    } catch (...) {
        Rec(L"own box: could not clear the chrome %08X",
            static_cast<unsigned>(winrt::to_hresult()));
    }

    cell.Children().Append(box);
    g_ourBox = box;

    g_ourBoxChanged = box.TextChanged(
        winrt::auto_revoke,
        [](wf::IInspectable const& sender, wuxc::TextChangedEventArgs const&) {
            try {
                auto b = sender.as<wuxc::TextBox>();
                std::wstring text{b.Text()};
                Rec(L"own box: '%ls'", text.c_str());
                QueueQuery(std::move(text));
            } catch (...) {
            }
        });

    // Listening, rather than holding focus.
    //
    // Two focus-based attempts failed. Grabbing it on LostFocus ran in the
    // middle of input processing and was overridden immediately -- the box lit
    // up on mouse-down and went dark on mouse-up. Polling for it with a timer
    // worked but was blunt: it would take focus back from anything in the menu
    // that legitimately wanted it, including keyboard navigation of the app
    // list.
    //
    // So nothing here takes focus at all. The characters are read at the
    // window as they arrive, which is what the stock menu does too -- typing
    // anywhere starts a search there, without the box being focused first.
    //
    // When the box does have focus, because it was clicked, it handles its own
    // input and this stays out of the way; otherwise every letter would be
    // doubled.
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
                        static bool said = false;
                        if (!said) {
                            said = true;
                            Rec(L"focus: first character arrived while focus "
                                L"was on %ls",
                                FocusedElementLabel().c_str());
                            Rec(L"geometry: our box %.0fx%.0f at padding "
                                L"top=%.0f",
                                g_ourBox.ActualWidth(), g_ourBox.ActualHeight(),
                                g_ourBox.Padding().Top);
                        }
                        std::wstring text{g_ourBox.Text()};
                        text.push_back(static_cast<wchar_t>(code));
                        g_ourBox.Text(text);
                        // Caret to the end, so clicking into the box later
                        // continues from where the text does.
                        g_ourBox.SelectionStart(
                            static_cast<int32_t>(text.size()));
                    } catch (...) {
                    }
                });
                // Backspace and Escape never arrive as characters, so the
                // box would fill up with no way to correct it.
                core.KeyDown([](wuc::CoreWindow const&,
                                wuc::KeyEventArgs const& args) {
                    try {
                        if (!g_ourBox) {
                            return;
                        }
                        auto focused =
                            wux::Input::FocusManager::GetFocusedElement();
                        if (focused && focused.try_as<wuxc::TextBox>()) {
                            return;  // the box is handling its own editing
                        }
                        std::wstring text{g_ourBox.Text()};
                        if (text.empty()) {
                            return;
                        }
                        auto key = args.VirtualKey();
                        if (key == winrt::Windows::System::VirtualKey::Back) {
                            text.pop_back();
                        } else if (key ==
                                   winrt::Windows::System::VirtualKey::Escape) {
                            // Clear rather than close: with text in the box,
                            // Escape reads as "undo the search".
                            text.clear();
                        } else {
                            return;
                        }
                        g_ourBox.Text(text);
                        g_ourBox.SelectionStart(
                            static_cast<int32_t>(text.size()));
                        args.Handled(true);
                    } catch (...) {
                    }
                });
                Rec(L"own box: listening for characters and edit keys");
            }
        }
    } catch (...) {
        Rec(L"own box: CharacterReceived hook threw %08X",
            static_cast<unsigned>(winrt::to_hresult()));
    }

    // Where focus actually goes, rather than guessing at it. Logged on every
    // change, with the element that took it, because three focus strategies
    // have now failed and none of them said what they were losing to.
    // When a ScrollViewer takes focus, stop that one being a tab stop.
    //
    // The log named the culprit: focus goes to
    // Windows.UI.Xaml.Controls.ScrollViewer -- the app list's scroll
    // container, which claims focus as the menu builds.
    //
    // Deleting it is not an option: it is what scrolls the apps. Taking it out
    // of the tab order is, and it leaves scrolling, the wheel and the list's
    // own items alone. Done only to a ScrollViewer actually caught taking
    // focus from us, rather than to every one in the tree, so nothing is
    // disabled on a guess.
    //
    // The cost is that Tab may no longer land on the app list. That is a real
    // regression if anyone navigates the menu that way, and it is the reason
    // this is worth watching rather than assuming it is free.
    g_ourBoxLost = box.LostFocus(
        winrt::auto_revoke,
        [](wf::IInspectable const&, wux::RoutedEventArgs const&) {
            try {
                auto focused = wux::Input::FocusManager::GetFocusedElement();
                if (!focused) {
                    return;
                }
                auto dobj = focused.try_as<wux::DependencyObject>();
                if (!dobj) {
                    return;
                }
                std::wstring who = ElementLabel(dobj);
                Rec(L"focus: left our box, went to %ls", who.c_str());

                // The window comes back whatever took focus, not only when
                // it was the ScrollViewer.
                //
                // This branch used to return here for every other case, so
                // TakeForeground() was unreachable in practice -- the log
                // showed no foreground line at all while the caret kept
                // disappearing. What actually happens most of the time is
                // that focus leaves and returns to our own box, and the
                // window activation goes to SearchHost in between; an
                // inactive window draws no caret and the keystrokes go with
                // the activation.
                RestoreWindowSoon();

                auto scroller = focused.try_as<wuxc::ScrollViewer>();
                if (!scroller || !scroller.IsTabStop()) {
                    return;
                }
                static int disarmed = 0;
                if (disarmed >= 8) {
                    return;  // something else is going on; stop meddling
                }
                ++disarmed;
                scroller.IsTabStop(false);
                Rec(L"focus: took %ls out of the tab order (%d)", who.c_str(),
                    disarmed);
                // Deferred by a tick. Calling Focus() here runs inside the
                // focus change that is still in progress, and whatever is
                // taking focus takes it again immediately afterwards -- the
                // same reason the earlier LostFocus attempt failed, visible on
                // screen as the box lighting up on mouse-down and going dark
                // on mouse-up.
                if (g_ourBox) {
                    auto back = wux::DispatcherTimer();
                    back.Interval(std::chrono::milliseconds(60));
                    back.Tick([back](wf::IInspectable const&,
                                     wf::IInspectable const&) {
                        back.Stop();
                        if (!g_ourBox) {
                            return;
                        }
                        // The window, not just the element.
                        //
                        // Clicking the menu's body hands the foreground back
                        // to SearchHost, and an inactive window draws no
                        // caret -- which reads as "the box lost focus" even
                        // though the log shows focus still on it. So restore
                        // the window first.
                        TakeForeground();

                        // A click on something real should still win, so only
                        // reclaim the element if nothing took it meanwhile.
                        auto now =
                            wux::Input::FocusManager::GetFocusedElement();
                        if (now && !now.try_as<wuxc::ScrollViewer>()) {
                            return;
                        }
                        g_ourBox.Focus(wux::FocusState::Programmatic);
                    });
                    back.Start();
                    g_refocus = back;
                }
            } catch (...) {
            }
        });

    // And once now, for the opening that is already on screen.
    // Nothing here takes focus, deliberately.
    //
    // Three attempts established why. Focusing on placement did not survive
    // the menu opening. Re-focusing from LostFocus ran in the middle of input
    // processing and was overridden a moment later -- the box lit up on
    // mouse-down and went dark on mouse-up. A polling timer held focus but
    // took it back from anything else in the menu that wanted it, including
    // keyboard navigation of the app list.
    //
    // The listener below reads characters at the window instead, which is
    // also how the stock menu behaves: typing starts a search without the box
    // being focused first.


    Rec(L"own box: placed in %ls (%.0fx%.0f)", ElementLabel(cell).c_str(),
        box.Width(), box.Height());

    // Focus once, when the menu opens -- and then leave it alone.
    //
    // The log answered what had been guesswork: focus goes to
    // Windows.UI.Xaml.Controls.ScrollViewer, the app list. That is the menu's
    // own doing and it is reasonable, which is why holding focus against it
    // was the wrong idea -- a polling timer that wins that argument also
    // breaks arrow-key navigation through the apps.
    //
    // Taking it once per opening is different. The ScrollViewer claims focus
    // while the menu builds; a short delay lands after that, and nothing
    // takes it again unless the user clicks something, which should win.
    //
    // Typing does not depend on this. The character listener already works
    // whatever has focus; this is for the caret, so the box looks ready.
    try {
        if (auto window = wux::Window::Current()) {
            if (auto core = window.CoreWindow()) {
                core.VisibilityChanged(
                    [](wuc::CoreWindow const&,
                       wuc::VisibilityChangedEventArgs const& args) {
                        if (!args.Visible() || !g_ourBox) {
                            return;
                        }
                        try {
                            auto once = wux::DispatcherTimer();
                            once.Interval(std::chrono::milliseconds(150));
                            once.Tick([once](wf::IInspectable const&,
                                             wf::IInspectable const&) {
                                once.Stop();
                                // The window first, then the element.
                                //
                                // XAML focus inside Start is not enough: the
                                // foreground window is SearchHost's, measured,
                                // and an inactive window draws no caret. That
                                // is why the box filled with text while looking
                                // dead -- the shell routes characters to this
                                // window regardless of which one is active.
                                TakeForeground();
                                if (g_ourBox) {
                                    g_ourBox.Focus(
                                        wux::FocusState::Programmatic);
                                }
                            });
                            once.Start();
                            g_openFocus = once;
                        } catch (...) {
                        }
                    });
                Rec(L"own box: will focus once per opening");
            }
        }
    } catch (...) {
        Rec(L"own box: visibility hook threw %08X",
            static_cast<unsigned>(winrt::to_hresult()));
    }

    // A few samples after placement: what holds focus when the menu is up is
    // the question, and it is not answerable at the moment of placement.
    try {
        auto probe = wux::DispatcherTimer();
        probe.Interval(std::chrono::milliseconds(700));
        probe.Tick([probe, n = std::make_shared<int>(0)](
                       wf::IInspectable const&, wf::IInspectable const&) {
            if (++*n > 8) {
                probe.Stop();
                return;
            }
            Rec(L"focus: t+%dms on %ls", *n * 700,
                FocusedElementLabel().c_str());
        });
        probe.Start();
        g_focusProbe = probe;
    } catch (...) {
    }
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

        if (g_settings.hideSearchBox) {
            button.Visibility(wux::Visibility::Collapsed);
            Rec(L"  collapsed it");
        } else if (g_settings.ownSearchBox) {
            // Keep it, and keep it looking like Windows: the border, the
            // magnifier and the hover states are all here and worth having.
            // Only the click has to go -- that is what hands off to the search
            // host. Left visible but inert, the same treatment that stopped
            // SearchHost crashing: an element still laid out where its own
            // code can find it.
            button.IsHitTestVisible(false);
            Rec(L"  kept for its chrome, click disabled");

            // Its placeholder and its drawn-on caret would show through
            // underneath ours.
            if (auto ph = FindDescendantByName(button, L"PlaceholderText", 6)) {
                if (auto fe = ph.try_as<wux::FrameworkElement>()) {
                    fe.Opacity(0.0);
                }
            }
            if (auto caret = FindDescendantByName(button, L"TextCaret", 6)) {
                if (auto fe = caret.try_as<wux::FrameworkElement>()) {
                    fe.Opacity(0.0);
                }
            }
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

    g_searchQuit.store(false);
    g_searchThread = std::thread(SearchThreadMain);
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

    // Before anything else: it owns a window and a pump, and its code is in
    // this image.
    g_searchQuit.store(true);
    g_queryWake.notify_all();
    if (g_searchThread.joinable()) {
        g_searchThread.join();
    }
    g_quit.store(true);
    if (g_tapThread.joinable()) {
        g_tapThread.join();
    }
    if (g_visualTreeWatcher) {
        g_visualTreeWatcher->UnadviseVisualTreeChange();
        g_visualTreeWatcher = nullptr;
    }
}
