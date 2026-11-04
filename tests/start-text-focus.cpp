// Native XAML controls in an isolated offscreen host. No Explorer access,
// keyboard injection, or edits to the user's Start layout.
#define NOMINMAX
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Markup.h>
#include <atomic>
#include <cstdio>
#include <thread>
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
namespace wuxm=wux::Media;
namespace wuxi=wux::Input;
std::atomic<bool> g_quit{false};
constexpr wchar_t kStartTextEditingProperty[] = L"StartEverything_StartTextEditing";
// The fixture has an island rather than Start's CoreWindow. Only the
// Window::Current lookup is substituted; controls, focus and HWND props are real.
wux::UIElement testContent{nullptr};
struct TestWindow {
    wux::UIElement Content() const { return testContent; }
    explicit operator bool() const { return bool(testContent); }
};
TestWindow TestCurrentWindow() { return {}; }
constexpr UINT kFocusMessage=WM_APP+97;
UINT StartTextFocusMessage(){return kFocusMessage;}
// PRODUCTION_CLASSIFIER
// PRODUCTION_REFRESH
int checks=0, failures=0;
void Check(bool ok,const char* message) {
    ++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",message);
}
void Pump() {
    MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
}
LRESULT CALLBACK TestWndProc(HWND hwnd,UINT message,WPARAM param,LPARAM data) {
    if(message==kFocusMessage){
        if(param==1)StopStartTextFocus(hwnd);else ObserveStartTextFocus(hwnd);
        return 1;
    }
    return DefWindowProcW(hwnd,message,param,data);
}
int main() try {
    setvbuf(stdout,nullptr,_IONBF,0);
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    WNDCLASSW cls{};cls.lpfnWndProc=TestWndProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"StartTextFocusFixture";
    Check(RegisterClassW(&cls)!=0,"isolated focus bridge window class is registered");
    auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls.lpszClassName,L"Isolated native Start editor tests",WS_POPUP,
                             -30000,-30000,400,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Check(hwnd!=nullptr,"isolated window is created");
    auto source=wux::Hosting::DesktopWindowXamlSource{};
    auto native=source.as<IDesktopWindowXamlSourceNative>();
    winrt::check_hresult(native->AttachToWindow(hwnd));
    HWND child=nullptr;winrt::check_hresult(native->get_WindowHandle(&child));
    SetWindowPos(child,nullptr,0,0,400,400,SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
    auto root=wuxc::StackPanel{};
    auto rename=wuxc::TextBox{};rename.Name(L"ExpandedFolderNameTextBox");rename.Text(L"Original group");
    rename.Height(40);
    rename.Template(wux::Markup::XamlReader::Load(
        L"<ControlTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' TargetType='TextBox'><Grid><Border/></Grid></ControlTemplate>")
        .as<wuxc::ControlTemplate>());
    auto button=wuxc::Button{};button.Content(winrt::box_value(L"Pinned app"));button.Height(40);
    auto readOnly=wuxc::TextBox{};readOnly.IsReadOnly(true);
    auto rich=wuxc::RichEditBox{};auto password=wuxc::PasswordBox{};
    for(auto const& control:{rename.as<wux::UIElement>(),button.as<wux::UIElement>(),
                            readOnly.as<wux::UIElement>(),rich.as<wux::UIElement>(),password.as<wux::UIElement>()})
        root.Children().Append(control);
    source.Content(root);rename.ApplyTemplate();root.Measure({400,400});root.Arrange({0,0,400,400});root.UpdateLayout();Pump();
    ShowWindow(hwnd,SW_SHOWNOACTIVATE);
    const auto readyUntil=GetTickCount64()+3000;
    while(GetTickCount64()<readyUntil){
        rename.ApplyTemplate();root.UpdateLayout();Pump();
        if(wuxm::VisualTreeHelper::GetChildrenCount(rename)>0 && rename.Focus(wux::FocusState::Programmatic) &&
           button.Focus(wux::FocusState::Programmatic))break;
        Sleep(5);
    }
    Pump();
    testContent=root;
    Check(!StartElementAcceptsText(nullptr),"missing focus allows ordinary Start typing");
    Check(StartElementAcceptsText(rename),"real Start folder-name TextBox is editable");
    Check(!StartElementAcceptsText(button),"pinned app buttons retain type-to-search");
    Check(!StartElementAcceptsText(readOnly),"read-only text is not a rename field");
    Check(StartElementAcceptsText(rich),"native rich-text editor retains typing");
    rich.IsReadOnly(true);Check(!StartElementAcceptsText(rich),"read-only rich text does not block search");
    Check(StartElementAcceptsText(password),"native password field retains typing");
    const auto inner=wuxm::VisualTreeHelper::GetChildrenCount(rename)?wuxm::VisualTreeHelper::GetChild(rename,0):nullptr;
    Check(inner && StartElementAcceptsText(inner),"focus within a text-control template is recognized");
    ObserveStartTextFocus(hwnd);
    Check(g_startTextFocusWindow==hwnd,"XAML focus observer is installed on the owning thread");
    for(int i=0;i<5;++i){
        Check(rename.Focus(wux::FocusState::Programmatic),"logical focus enters the real folder editor");
        Check(GetPropW(hwnd,kStartTextEditingProperty)!=nullptr,"rename guard is set synchronously before the next character");
        Pump();
        auto focused=wux::Input::FocusManager::GetFocusedElement(root.XamlRoot()).try_as<wux::DependencyObject>();
        Check(StartElementAcceptsText(focused),"focused native editor is classified without reading its text");
        Check(GetPropW(hwnd,kStartTextEditingProperty)!=nullptr,"Start HWND publishes that its editor owns typing");
        Check(button.Focus(wux::FocusState::Programmatic),"logical focus returns to a pinned app");
        Check(GetPropW(hwnd,kStartTextEditingProperty)==nullptr,"search resumes synchronously when a pinned app takes focus");
        Pump();
        focused=wux::Input::FocusManager::GetFocusedElement(root.XamlRoot()).try_as<wux::DependencyObject>();
        Check(!StartElementAcceptsText(focused),"leaving the editor immediately restores search eligibility");
        Check(GetPropW(hwnd,kStartTextEditingProperty)==nullptr,"leaving the editor clears the shared typing guard");
    }
    Check(rename.Text()==L"Original group","classification never changes folder text");
    rename.Focus(wux::FocusState::Programmatic);Pump();RefreshStartTextFocus(hwnd);
    Check(GetPropW(hwnd,kStartTextEditingProperty)!=nullptr,"initial refresh recognizes an already focused rename field");
    auto cancel=button.GettingFocus(winrt::auto_revoke,[](auto const&,auto const& args){args.Cancel(true);});
    button.Focus(wux::FocusState::Programmatic);Pump();
    Check(wuxi::FocusManager::GetFocusedElement(root.XamlRoot())==rename && GetPropW(hwnd,kStartTextEditingProperty),
          "a canceled focus change keeps the rename editor protected");
    cancel.revoke();
    StopStartTextFocus(hwnd);button.Focus(wux::FocusState::Programmatic);Pump();
    rename.Focus(wux::FocusState::Programmatic);Pump();
    Check(!GetPropW(hwnd,kStartTextEditingProperty)&&!g_startTextFocusWindow,"teardown removes focus handlers and clears the guard");
    ObserveStartTextFocus(hwnd);ObserveStartTextFocus(hwnd);
    Check(GetPropW(hwnd,kStartTextEditingProperty)!=nullptr,"repeated setup remains safe and observes an already focused editor");
    std::atomic<bool> cleanupDone{false};
    std::thread cleanup([hwnd,&cleanupDone]{StopStartTextFocus(hwnd);cleanupDone=true;});
    while(!cleanupDone){Pump();Sleep(1);}cleanup.join();
    button.Focus(wux::FocusState::Programmatic);Pump();rename.Focus(wux::FocusState::Programmatic);Pump();
    Check(!GetPropW(hwnd,kStartTextEditingProperty)&&!g_startTextFocusWindow,"foreign-thread teardown revokes through the owning UI thread");
    ObserveStartTextFocus(hwnd);g_quit=true;
    const auto owner=g_startTextFocusThread.load();
    std::thread stalledCleanup([hwnd]{StopStartTextFocus(hwnd);});stalledCleanup.join();
    Check(!GetPropW(hwnd,kStartTextEditingProperty) && g_startTextFocusThread==owner,
          "unresponsive UI fallback clears the guard without revoking from the wrong thread");
    button.Focus(wux::FocusState::Programmatic);rename.Focus(wux::FocusState::Programmatic);Pump();
    Check(!GetPropW(hwnd,kStartTextEditingProperty),"retained callbacks remain inert during shutdown");
    StopStartTextFocus(hwnd);g_quit=false;
    StopStartTextFocus(hwnd);g_quit=true;ObserveStartTextFocus(hwnd);Pump();
    Check(!GetPropW(hwnd,kStartTextEditingProperty),"shutdown cannot republish an editor flag");
    g_quit=false;
    testContent=nullptr;RefreshStartTextFocus(hwnd);
    Check(GetPropW(hwnd,kStartTextEditingProperty)==nullptr,"missing Start content cannot leave an editor flag stuck");
    StopStartTextFocus(hwnd);
    source.Content(nullptr);source.Close();DestroyWindow(hwnd);manager.Close();
    std::printf("RESULT %d checks, %d failures\n",checks,failures);return failures?1:0;
} catch(winrt::hresult_error const& e){std::printf("XAML failure %08X\n",unsigned(e.code()));return 1;}
