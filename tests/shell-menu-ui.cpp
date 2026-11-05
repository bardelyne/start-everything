// Real native XAML menu items with a fake shell/launcher. No user UI input and
// no actual pin, unpin or Settings launch is performed.
#define NOMINMAX
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <optional>
#include <algorithm>
#include <string>
#include <vector>
#include <chrono>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
namespace wuxcp=wuxc::Primitives;
namespace wuxma=wux::Media::Animation;
std::atomic<HWND> g_searchWindowWindow{nullptr};
std::vector<std::shared_ptr<void>> handlers;
template<class Source,class Revoker>void KeepHandler(Source const&,Revoker revoker){handlers.push_back(std::make_shared<Revoker>(std::move(revoker)));}
template<class... T>void Wh_Log(T&&...){}
wuxc::FontIcon SearchFontIcon(){return wuxc::FontIcon{};}
namespace i18n {enum class Key{AppSettings,CheckingPins};const wchar_t* Text(Key key){return key==Key::CheckingPins?L"Checking pins…":L"App settings";}}
bool current=true,reenter=false;
int pinCalls=0,dismissed=0;
std::atomic<bool> blockDiscovery{false},discoveryEntered{false};
std::atomic<DWORD> discoveryThread{0};

std::wstring rowTarget=L"old-target",rowPackage=L"old-package",observedTarget,observedPackage,opened,invokedTarget,invokedVerb;
namespace shellactions {
enum class Pin{Start,Unstart,Taskbar,Untaskbar};
struct Command{Pin kind;std::wstring verb,label;bool enabled=true;std::wstring target;};
std::vector<Command> commands;
std::wstring settingsArguments=L"page=SettingsPageInstalledApps&target=SystemSettings_StorageSense_HiddenAppAdvancedPageLink&parameter=test&invoke=true";
std::vector<Command> Pins(std::wstring const& target,HWND){
    discoveryThread=GetCurrentThreadId();discoveryEntered=true;
    while(blockDiscovery.load())Sleep(1);
    observedTarget=target;
    if(reenter){rowTarget=L"new-target";rowPackage=L"new-package";current=false;}
    return commands;
}
std::wstring SettingsArguments(std::wstring const& identity,std::wstring const&){observedPackage=identity;return settingsArguments;}
HRESULT InvokePin(std::wstring const& target,std::wstring_view verb,HWND){++pinCalls;invokedTarget=target;invokedVerb=verb;return S_OK;}
}
void DismissSearchForLaunch(std::function<void()> action){++dismissed;action();}
void OpenAppSettings(std::wstring const& arguments){opened=arguments;}
unsigned lateStyles=0;
void PrepareCopiedMenu(wuxc::MenuFlyout const&){++lateStyles;}
// PRODUCTION_SHELL_MENU
LRESULT CALLBACK MenuWndProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    if (message == shellmenus::Message()) { CompleteShellMenuItems(); return 0; }
    return DefWindowProcW(hwnd, message, w, l);
}

int checks=0;
void Check(bool ok,const char* message){++checks;if(!ok){printf("FAIL %s\n",message);ExitProcess(1);}}
void Pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
void AwaitMenu() {
    const auto until=GetTickCount64()+5000;
    while(!g_pendingShellMenus.empty()&&GetTickCount64()<until){Pump();Sleep(1);}
    Check(g_pendingShellMenus.empty(),"background menu result returns to the owning UI thread");
}
void Invoke(wuxc::MenuFlyoutItem const& item){
    wux::Automation::Peers::MenuFlyoutItemAutomationPeer peer(item);
    peer.as<wux::Automation::Provider::IInvokeProvider>().Invoke();
    const auto until=GetTickCount64()+100;while(GetTickCount64()<until){Pump();Sleep(1);}
}
int main()try{
    setvbuf(stdout,nullptr,_IONBF,0);winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    WNDCLASSW cls{};cls.lpfnWndProc=MenuWndProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"StartEverything.AsyncShellMenuFixture";
    Check(RegisterClassW(&cls)!=0,"register private menu completion window");
    HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,cls.lpszClassName,L"Isolated shell menu tests",WS_POPUP,-30000,-30000,300,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    g_searchWindowWindow=hwnd;
    auto source=wux::Hosting::DesktopWindowXamlSource{};source.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(hwnd);
    source.Content(wuxc::Grid{});Pump();
    shellactions::commands={{shellactions::Pin::Start,L"PinToStartScreen",L"Native localized pin"},
        {shellactions::Pin::Untaskbar,L"taskbarunpin",L"Native localized unpin",false}};
    wuxc::MenuFlyout menu;wuxc::MenuFlyoutItem open;open.Text(L"Open");menu.Items().Append(open);
    AddShellMenuItems(menu,rowTarget,rowPackage,{},[]{return current;});
    Check(menu.Items().Size()==3,"pending actions show a single checking row after a separator");
    auto checking=menu.Items().GetAt(2).as<wuxc::MenuFlyoutItem>();
    Check(checking.Text()==L"Checking pins…"&&!checking.IsEnabled()&&checking.Icon(),
          "checking pins is localized, disabled, and has a pin icon");
    AwaitMenu();
    Check(menu.Items().Size()==5,"shell actions follow one separator and preserve existing menu entries");
    Check(lateStyles==1,"background-discovered actions receive the copied menu appearance after insertion");
    for(auto const& item:menu.Items())
        if(auto text=item.try_as<wuxc::MenuFlyoutItem>())
            Check(text.Text()!=L"Checking pins…","finished actions replace the checking indicator");
    auto pin=menu.Items().GetAt(2).as<wuxc::MenuFlyoutItem>();auto disabled=menu.Items().GetAt(3).as<wuxc::MenuFlyoutItem>();
    auto settings=menu.Items().GetAt(4).as<wuxc::MenuFlyoutItem>();
    Check(pin.Text()==L"Native localized pin"&&settings.Text()==L"App settings","native pin labels and translated App settings label reach real XAML items");
    Check(!disabled.IsEnabled()&&disabled.Icon(),"native disabled state and action icons are retained");
    Invoke(pin);Check(pinCalls==1&&invokedTarget==L"old-target"&&invokedVerb==L"PinToStartScreen","the chosen XAML pin action forwards its exact target and verb");
    shellactions::commands[0].target=L"C:\\Original shortcut.lnk";
    wuxc::MenuFlyout shortcutMenu;AddShellMenuItems(shortcutMenu,rowTarget,{},{});
    AwaitMenu();
    Invoke(shortcutMenu.Items().GetAt(0).as<wuxc::MenuFlyoutItem>());
    Check(pinCalls==2&&invokedTarget==L"C:\\Original shortcut.lnk","fallback pinning uses the original shortcut instead of the app ID or executable");
    Check(dismissed==0,"Pin and Unpin keep the search window open");
    Invoke(settings);Check(opened==shellactions::settingsArguments&&dismissed==1,"App settings launches its validated native page arguments through normal launch dismissal");
    current=false;Invoke(pin);Invoke(settings);
    Check(pinCalls==2&&dismissed==1,"recycled rows cannot trigger stale pinning or Settings actions");
    current=true;shellactions::commands.clear();shellactions::settingsArguments.clear();
    wuxc::MenuFlyout unsupported;AddShellMenuItems(unsupported,L"unsupported",L"unsupported");
    AwaitMenu();
    Check(unsupported.Items().Size()==0,"unsupported targets add no actions or empty separators");
    shellactions::commands={{shellactions::Pin::Start,L"PinToStartScreen",L"Pin"}};
    shellactions::settingsArguments=L"valid";reenter=true;
    wuxc::MenuFlyout replaced;AddShellMenuItems(replaced,rowTarget,rowPackage,{},[]{return current;});
    AwaitMenu();
    Check(observedTarget==L"old-target"&&observedPackage==L"old-package","shell reentrancy cannot mix metadata from two different recycled rows");
    Check(replaced.Items().Size()==0,"a target changed during native menu discovery exposes no stale actions");
    current=true;reenter=false;shellactions::commands.clear();
    wuxc::MenuFlyout inventory;AddShellMenuItems(inventory,{},L"supported-package",L"exact-version",[]{return current;});
    AwaitMenu();
    Check(inventory.Items().Size()==1&&inventory.Items().GetAt(0).as<wuxc::MenuFlyoutItem>().Text()==L"App settings",
          "installed inventory can expose App settings without a launch or pin option");
    blockDiscovery=true;discoveryEntered=false;
    current=true;wuxc::MenuFlyout slow;
    std::thread release([&]{Sleep(1500);blockDiscovery=false;});
    AddShellMenuItems(slow,L"slow-target",L"slow-package",{},[]{return current;});
    const auto until=GetTickCount64()+1000;
    while(!discoveryEntered.load()&&GetTickCount64()<until){Pump();Sleep(1);}
    Check(blockDiscovery&&discoveryThread!=GetCurrentThreadId(),"blocked shell discovery does not block the XAML/menu thread");
    Check(slow.Items().Size()==1&&!slow.Items().GetAt(0).as<wuxc::MenuFlyoutItem>().IsEnabled(),
          "a blocked lookup leaves only the disabled checking indicator");
    current=false;release.join();AwaitMenu();
    Check(slow.Items().Size()==0,"a row replaced during slow background discovery receives no old menu actions");
    current=true;
    blockDiscovery=true;discoveryEntered=false;
    wuxc::MenuFlyout requestedTwice;wuxc::MenuFlyoutItem existing;existing.Text(L"Open");
    requestedTwice.Items().Append(existing);
    AddShellMenuItems(requestedTwice,L"first-request",L"test-package");
    AddShellMenuItems(requestedTwice,L"replacement-request",L"test-package");
    Check(requestedTwice.Items().Size()==3,"replacing a pending request never duplicates the checking row or separator");
    blockDiscovery=false;AwaitMenu();
    Check(requestedTwice.Items().Size()==3&&requestedTwice.Items().GetAt(2).as<wuxc::MenuFlyoutItem>().Text()==L"App settings",
          "replacement result removes its indicator and adds only the actual Settings action");
    for(int panel=0;panel<2;++panel){
        wuxc::MenuFlyout immediate;
        DisableMenuAnimations(immediate);
        Check(!immediate.AreOpenCloseAnimationsEnabled(),"both panel menus disable native opening and closing animations");
        wuxcp::Popup popup;wuxc::MenuFlyoutPresenter presenter;
        popup.Child(presenter);popup.XamlRoot(source.Content().XamlRoot());
        wuxma::TransitionCollection transitions;transitions.Append(wuxma::EntranceThemeTransition{});
        popup.ChildTransitions(transitions);
        wuxma::TransitionCollection itemTransitions;itemTransitions.Append(wuxma::EntranceThemeTransition{});
        presenter.Transitions(itemTransitions);
        int activated=0;
        wuxc::MenuFlyoutItem action;action.Text(L"Open");
        KeepHandler(action,action.Click(winrt::auto_revoke,[&](auto const&,auto const&){++activated;}));
        presenter.Items().Append(action);
        popup.IsOpen(true);Pump();
        DisableMenuTransitions(popup,presenter);
        Check(popup.ChildTransitions().Size()==0&&presenter.Transitions().Size()==0,
              "popup and presenter entrance transitions are removed");
        auto visual=wux::Hosting::ElementCompositionPreview::GetElementVisual(presenter);
        Check(visual.Opacity()==1&&!visual.TryGetAnimationController(L"Opacity"),
              "the menu remains fully visible without an opacity animation");
        Check(presenter.IsHitTestVisible()&&action.IsHitTestVisible()&&action.IsEnabled(),
              "menu actions remain enabled and hit-testable");
        Check(action.Focus(wux::FocusState::Keyboard),"an existing menu option can receive keyboard selection");
        Invoke(action);Check(activated==1,"an existing menu option still invokes normally");
        wuxc::MenuFlyoutItem late;late.Text(L"Pin to Start");
        KeepHandler(late,late.Click(winrt::auto_revoke,[&](auto const&,auto const&){++activated;}));
        presenter.Items().Append(late);presenter.UpdateLayout();
        DisableMenuTransitions(popup,presenter);
        Check(action.Focus(wux::FocusState::Keyboard),"late insertion does not prevent selection of earlier options");
        Invoke(action);Check(activated==2,"existing options still work after late actions arrive");
        Check(late.Focus(wux::FocusState::Keyboard),"the newly inserted action can receive keyboard selection");
        Invoke(late);Check(activated==3,"the newly inserted action invokes normally");
        Check(!visual.TryGetAnimationController(L"Opacity"),"late actions never start a presenter opacity animation");
        for(int cycle=0;cycle<4;++cycle){
            popup.IsOpen(false);Pump();popup.IsOpen(true);Pump();
            DisableMenuTransitions(popup,presenter);
            Check(action.Focus(wux::FocusState::Keyboard),"reopening keeps menu options selectable");
            Invoke(action);Check(activated==4+cycle,"reopening keeps menu options functional");
        }
        popup.IsOpen(false);Pump();popup.Child(nullptr);
    }
    blockDiscovery=true;discoveryEntered=false;
    wuxc::MenuFlyout cancelled;AddShellMenuItems(cancelled,L"cancelled");
    const auto waiting=GetTickCount64()+1000;
    while(!discoveryEntered.load()&&GetTickCount64()<waiting){Pump();Sleep(1);}
    Check(cancelled.Items().Size()==1,"pending lookup has a checking indicator before shutdown");
    blockDiscovery=false;
    StopShellMenuItems();
    Check(g_pendingShellMenus.empty()&&!shellmenus::worker,"teardown joins discovery and removes every pending UI callback");
    Check(cancelled.Items().Size()==0,"shutdown removes the checking indicator without leaving a separator");
    AddShellMenuItems(cancelled,L"stopped");
    Check(cancelled.Items().Size()==0&&g_pendingShellMenus.empty(),"failed enqueue immediately removes the checking indicator");
    handlers.clear();source.Content(nullptr);source.Close();DestroyWindow(hwnd);UnregisterClassW(cls.lpszClassName,cls.hInstance);manager.Close();
    printf("PASS: %d native shell-menu UI checks; no real shell actions executed\n",checks);
}catch(winrt::hresult_error const& error){printf("XAML error %08X\n",unsigned(error.code()));return 1;}
