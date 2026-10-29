// Exercise the real hook and delivery code without installing a hook, sending
// input, reading the user's clipboard, or creating any desktop windows. Fake
// host/focus state lets the tests cover handoff races deterministically.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

constexpr UINT kSearchWindowInputMessage = WM_APP + 1;
constexpr UINT kSearchWindowDismissMenuMessage = WM_APP + 2;
constexpr ULONG_PTR kSearchWindowImeReplayTag = 0x5345494d;
const HWND host = reinterpret_cast<HWND>(1), start = reinterpret_cast<HWND>(2);
const HWND searchHost = reinterpret_cast<HWND>(3), other = reinterpret_cast<HWND>(4);
HWND foreground = start;
bool nativeFocus = true, visible = false, wakeSucceeds = true, insideHook = false;
int checks = 0, failures = 0, pastes = 0, shown = 0, immediate = 0, nativeDelivered = 0;
std::wstring clipboard = L"clipboard text";
HKL layout = reinterpret_cast<HKL>(0x409);
std::atomic<HWND> g_searchWindowWindow{host}, g_hExplorerHelperWnd{nullptr};
std::atomic<bool> g_contextMenuOpen{false}, g_dragging{false}, g_isOverlayVisible{false};
std::atomic<unsigned> g_alternativeShortcutActive{0};
bool g_searchWindowWaitingForStart = false;
HWND g_searchWindowChild = reinterpret_cast<HWND>(5);

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) ++failures;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", message);
}
namespace winrt {
using hstring = std::wstring;
HRESULT to_hresult() { return E_FAIL; }
namespace Windows {
namespace System { enum class VirtualKey { Enter }; }
namespace UI::Xaml::Hosting {
enum class XamlSourceFocusNavigationReason { First };
struct XamlSourceFocusNavigationRequest { XamlSourceFocusNavigationReason reason; };
}
}
}
struct TestBox {
    std::wstring value;
    int32_t caret = 0, selection = 0;
    const std::wstring& Text() const { return value; }
    void Text(const std::wstring& text) { value = text; caret = 0; selection = 0; }
    int32_t SelectionStart() const { return caret; }
    void SelectionStart(int32_t position) { caret = position; }
    int32_t SelectionLength() const { return selection; }
    void SelectionLength(int32_t length) { selection = length; }
    void SelectedText(const std::wstring& text) { value.replace(caret, selection, text); }
    void PasteFromClipboard() {
        Check(!insideHook, "clipboard access stays outside the keyboard hook");
        ++pastes;
        SelectedText(clipboard);
        caret += static_cast<int32_t>(clipboard.size());
        selection = 0;
    }
    int XamlRoot() const { return 0; }
    void Focus(int) {}
    bool operator!=(const TestBox& box) const { return this != &box; }
} g_ourBox;
namespace wux {
namespace FocusState { constexpr int Programmatic = 0; }
namespace Input {
struct FocusManager { static TestBox& GetFocusedElement(int) { return g_ourBox; } };
}
}
struct TestSource {
    bool HasFocus() const { return nativeFocus; }
    template<class T> void NavigateFocus(T const&) {}
} g_searchWindowSource;

HWND TestForeground() { return foreground; }
HWND TestAncestor(HWND window, UINT) { return window; }
DWORD TestThread(HWND window, DWORD*) { return static_cast<DWORD>(reinterpret_cast<UINT_PTR>(window)); }
BOOL TestVisible(HWND) { return visible; }
BOOL TestPostMessage(HWND, UINT, WPARAM, LPARAM) { return wakeSucceeds; }
HWND TestSetFocus(HWND window) { return window; }
enum class Remap { None, VirtualKey, Unicode, Deferred, Block };
Remap remap = Remap::None;
int downstreamCalls = 0;
std::vector<std::pair<WPARAM,KBDLLHOOKSTRUCT>> deferred;
LRESULT TestCallNext(HHOOK, int, WPARAM, LPARAM);
#define GetForegroundWindow TestForeground
#define GetAncestor TestAncestor
#define GetWindowThreadProcessId TestThread
#define IsWindowVisible TestVisible
#define PostMessageW TestPostMessage
#define SetFocus TestSetFocus
#define CallNextHookEx TestCallNext

HWND CachedStartWindow() { return start; }
bool IsSearchHostWindow(HWND window) { return window == searchHost; }
bool SearchWindowHasNativeFocus(HWND) { return nativeFocus; }
bool AlternativeShortcutMatches(unsigned, UINT, BYTE const*) { return false; }
HKL SearchWindowInputLayout(HWND) { return layout; }
bool SearchWindowImeContextReady() { return false; }
struct SearchWindowInput;
bool DeliverNativeSearchKey(SearchWindowInput const&) { ++nativeDelivered; return true; }
void QueueSearchBoxRefocus(bool = false) {}
void DismissSearchContextMenu() { g_contextMenuOpen = false; }
void SearchWindowTrace(const wchar_t*, HRESULT = S_OK) {}
void SearchWindowShow() {
    ++shown;
    if (foreground == start) ++immediate;
    foreground = host;
    visible = true;
    g_ourBox.Text(L"");
}
void SearchWindowHide() { visible = false; }
void HandleNavigationKey(winrt::Windows::System::VirtualKey, bool) {}

// PRODUCTION_INPUT

void Reset(HWND window = start) {
    foreground = window;
    visible = window == host;
    nativeFocus = true;
    remap = Remap::None;
    downstreamCalls = 0;
    deferred.clear();
    wakeSucceeds = true;
    g_searchWindowWindow = host;
    g_hExplorerHelperWnd = nullptr;
    g_searchWindowInputStop = false;
    g_searchWindowInputReady = window == host;
    g_isOverlayVisible = visible;
    g_contextMenuOpen = false;
    g_searchWindowMenuTyping = false;
    g_searchWindowMenuCaret = -1;
    g_searchWindowMenuSelection = 0;
    g_searchWindowInputs.clear();
    g_searchWindowBuffered = 0;
    memset(g_searchWindowKeyState, 0, sizeof(g_searchWindowKeyState));
    memset(g_searchWindowSwallowed, 0, sizeof(g_searchWindowSwallowed));
    memset(g_searchWindowNativeKeyDown, 0, sizeof(g_searchWindowNativeKeyDown));
    layout = reinterpret_cast<HKL>(0x409);
    g_ourBox.Text(L"");
    pastes = shown = immediate = nativeDelivered = 0;
}
LRESULT Key(UINT vk, bool down = true, ULONG_PTR tag = 0, UINT scan = 0) {
    KBDLLHOOKSTRUCT event{};
    event.vkCode = vk;
    event.scanCode = scan;
    event.dwExtraInfo = tag;
    insideHook = true;
    const auto result = SearchWindowKeyboardProc(HC_ACTION, down ? WM_KEYDOWN : WM_KEYUP,
                                                reinterpret_cast<LPARAM>(&event));
    insideHook = false;
    return result;
}
LRESULT TestCallNext(HHOOK, int code, WPARAM message, LPARAM param) {
    ++downstreamCalls;
    const auto& key = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(param);
    if(code != HC_ACTION || remap == Remap::None || (key.flags & LLKHF_INJECTED) || key.vkCode != 'C')return 0;
    if(remap == Remap::Block)return 7;
    auto replaced=key;replaced.flags|=LLKHF_INJECTED;
    replaced.vkCode=remap==Remap::Unicode?VK_PACKET:'D';
    replaced.scanCode=remap==Remap::Unicode?L'\u00E9':MapVirtualKeyW('D',MAPVK_VK_TO_VSC);
    if(remap==Remap::Deferred)deferred.push_back({message,replaced});
    else SearchWindowKeyboardProc(HC_ACTION,message,reinterpret_cast<LPARAM>(&replaced));
    return 1;
}
void Ctrl() { Key(VK_LCONTROL); }
bool PasteQueued() {
    return g_searchWindowInputs.size() == 1 && g_searchWindowInputs[0].kind == SearchWindowInput::Paste;
}
int main() {
    Reset();remap=Remap::VirtualKey;
    Check(Key('C')==1 && Key('C',false)==1,"a physical key consumed by a remapper is not captured again");
    Check(downstreamCalls==4 && g_searchWindowInputs.size()==1 && g_searchWindowInputs[0].text==L"d",
          "EPKL-style C-to-D remapping buffers only D and calls each hook chain once");
    Check(!g_searchWindowKeyState['C'] && !g_searchWindowKeyState['D'] && !g_searchWindowSwallowed['D'],
          "replacement key releases leave no physical or injected key stuck");
    ApplySearchWindowInput();Check(g_ourBox.Text()==L"d" && immediate==1,"the remapped opening character reaches immediate Start search");
    Reset();remap=Remap::Unicode;Key('C');Key('C',false);ApplySearchWindowInput();
    Check(g_ourBox.Text()==L"\u00E9" && downstreamCalls==4,"Unicode remapper output is preserved without translating the physical key");
    Reset();remap=Remap::Deferred;Key('C');Key('C',false);
    Check(g_searchWindowInputs.empty() && deferred.size()==2,"delayed remapper output does not leak an original opening character");
    remap=Remap::None;
    for(auto& event:deferred)SearchWindowKeyboardProc(HC_ACTION,event.first,reinterpret_cast<LPARAM>(&event.second));
    ApplySearchWindowInput();Check(g_ourBox.Text()==L"d","asynchronous remapper output still opens search with the right character");
    Reset();remap=Remap::Block;
    Check(Key('C')==7 && g_searchWindowInputs.empty() && downstreamCalls==1,"another hook's nonzero decision is preserved exactly");
    Reset();Key('C');Key('C',false);ApplySearchWindowInput();
    Check(g_ourBox.Text()==L"c" && downstreamCalls==2,"unmapped opening text is captured once after other hooks allow it");
    Reset(); Ctrl();
    Check(Key('V') == 1 && PasteQueued(), "Start Ctrl+V is captured before native Start handles it");
    Check(pastes == 0 && shown == 0, "hook only queues input and does not open UI or read the clipboard");
    ApplySearchWindowInput();
    Check(shown == 1 && immediate == 1, "opening paste uses the same immediate Start handoff as text");
    Check(g_ourBox.Text() == clipboard && pastes == 1, "paste reaches the new search box exactly once");
    Check(g_searchWindowBuffered == 0, "paste drains the opening input buffer");
    Check(Key('V', false) == 1 && Key(VK_LCONTROL, false) == 0, "captured V release is balanced and Ctrl release passes through");

    Reset(); Ctrl(); Key('V'); Key('V', false); Key(VK_LCONTROL, false);
    Key(VK_PACKET, true, 0, L'!'); Key(VK_PACKET, false, 0, L'!');
    ApplySearchWindowInput();
    Check(g_ourBox.Text() == clipboard + L"!", "text typed during the handoff follows the pasted text");
    Reset(); Ctrl(); Key('V'); Key('V', false); Key('V'); ApplySearchWindowInput();
    Check(g_ourBox.Text() == clipboard + clipboard && pastes == 2, "two deliberate opening pastes preserve their order");

    Reset(host); Ctrl();
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "paste in a focused search stays native");
    Reset(other); Ctrl();
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "another application cannot trigger search paste");
    Reset(searchHost); Ctrl();
    Check(Key('V') == 1 && PasteQueued(), "paste is retained during a native SearchHost handoff");
    Reset(host); g_searchWindowInputReady = false; g_ourBox.Text(L"abcde");
    g_ourBox.SelectionStart(1); g_ourBox.SelectionLength(3); Ctrl(); Key('V'); ApplySearchWindowInput();
    Check(g_ourBox.Text() == L"a" + clipboard + L"e" && !shown,
          "paste during focus recovery replaces the existing selection without reopening search");
    Reset(host); nativeFocus = false; Ctrl();
    Check(Key('V') == 1 && PasteQueued(), "paste survives a lost native textbox focus");

    Reset(); Ctrl(); Key(VK_LSHIFT);
    Check(Key('V') == 1 && PasteQueued(), "Ctrl+Shift+V also pastes text during opening");
    Reset(); Ctrl(); Key(VK_LMENU);
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "Ctrl+Alt+V is not mistaken for paste");
    Reset(); Ctrl(); Key(VK_RMENU);
    Key('V');
    Check(std::none_of(g_searchWindowInputs.begin(), g_searchWindowInputs.end(),
                     [](auto const& input) { return input.kind == SearchWindowInput::Paste; }),
          "AltGr text is not mistaken for paste");
    Reset(); Ctrl(); Key(VK_LWIN);
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "Windows clipboard-history shortcuts remain untouched");
    Reset(); Ctrl();
    Check(Key('C') == 0 && g_searchWindowInputs.empty(), "other Ctrl shortcuts remain untouched");
    Reset(host); g_contextMenuOpen = true; Ctrl();
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "context-menu shortcuts keep their current behavior");

    for (const auto language : {0x804, 0x411, 0x412}) {
        Reset(); layout = reinterpret_cast<HKL>(language); Ctrl();
        Check(Key('V') == 1 && PasteQueued(), "CJK opening paste is clipboard text rather than an IME key replay");
        ApplySearchWindowInput();
        Check(g_ourBox.Text() == clipboard && nativeDelivered == 0, "CJK paste needs no synthetic key injection");
    }
    Reset(); layout = reinterpret_cast<HKL>(0x804);
    Check(Key(VK_PROCESSKEY) == 1 && g_searchWindowInputs.size() == 1 &&
          g_searchWindowInputs[0].kind == SearchWindowInput::NativeKey,
          "ordinary CJK opening typing still uses the native IME path");

    Reset(); Ctrl(); g_searchWindowBuffered = 256;
    Check(Key('V') == 0 && !g_searchWindowSwallowed['V'], "full input buffer passes the chord through");
    Reset(); Ctrl(); wakeSucceeds = false;
    Check(Key('V') == 0 && g_searchWindowInputs.empty() && !g_searchWindowBuffered,
          "failed UI wake does not lose the native paste or leave queued input");
    Reset(); Ctrl(); g_searchWindowInputStop = true;
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "shutdown cannot consume paste");
    Reset(); Ctrl();
    Check(Key('V', true, kSearchWindowImeReplayTag) == 0 && g_searchWindowInputs.empty(),
          "our tagged IME replay cannot recursively become a clipboard action");
    Reset(); g_searchWindowWindow = nullptr; g_hExplorerHelperWnd = other; Ctrl();
    Check(Key('V') == 1 && PasteQueued(), "opening paste is buffered before lazy search creation");
    Reset(); g_searchWindowWindow = nullptr; Ctrl();
    Check(Key('V') == 0 && g_searchWindowInputs.empty(), "no search relay means native paste passes through");
    std::printf("RESULT %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
