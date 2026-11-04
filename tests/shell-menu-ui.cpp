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
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
#include <winrt/Windows.UI.Xaml.Automation.Provider.h>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
std::atomic<HWND> g_searchWindowWindow{nullptr};
std::vector<std::shared_ptr<void>> handlers;
template<class Source,class Revoker>void KeepHandler(Source const&,Revoker revoker){handlers.push_back(std::make_shared<Revoker>(std::move(revoker)));}
template<class... T>void Wh_Log(T&&...){}
wuxc::FontIcon SearchFontIcon(){return wuxc::FontIcon{};}
namespace i18n {enum class Key{AppSettings};const wchar_t* Text(Key){return L"App settings";}}
bool current=true,reenter=false;
int pinCalls=0,dismissed=0;
std::wstring rowTarget=L"old-target",rowPackage=L"old-package",observedTarget,observedPackage,opened,invokedTarget,invokedVerb;
namespace shellactions {
enum class Pin{Start,Unstart,Taskbar,Untaskbar};
struct Command{Pin kind;std::wstring verb,label;bool enabled=true;std::wstring target;};
std::vector<Command> commands;
std::wstring settingsArguments=L"page=SettingsPageInstalledApps&target=SystemSettings_StorageSense_HiddenAppAdvancedPageLink&parameter=test&invoke=true";
std::vector<Command> Pins(std::wstring const& target,HWND){
    observedTarget=target;
    if(reenter){rowTarget=L"new-target";rowPackage=L"new-package";current=false;}
    return commands;
}
std::wstring SettingsArguments(std::wstring const& identity,std::wstring const&){observedPackage=identity;return settingsArguments;}
HRESULT InvokePin(std::wstring const& target,std::wstring_view verb,HWND){++pinCalls;invokedTarget=target;invokedVerb=verb;return S_OK;}
}
void DismissSearchForLaunch(std::function<void()> action){++dismissed;action();}
void OpenAppSettings(std::wstring const& arguments){opened=arguments;}
// PRODUCTION_SHELL_MENU
int checks=0;
void Check(bool ok,const char* message){++checks;if(!ok){printf("FAIL %s\n",message);ExitProcess(1);}}
void Pump(){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}}
void Invoke(wuxc::MenuFlyoutItem const& item){
    wux::Automation::Peers::MenuFlyoutItemAutomationPeer peer(item);
    peer.as<wux::Automation::Provider::IInvokeProvider>().Invoke();
    const auto until=GetTickCount64()+100;while(GetTickCount64()<until){Pump();Sleep(1);}
}
int main()try{
    setvbuf(stdout,nullptr,_IONBF,0);winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
    HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Isolated shell menu tests",WS_POPUP,-30000,-30000,300,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    g_searchWindowWindow=hwnd;
    auto source=wux::Hosting::DesktopWindowXamlSource{};source.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(hwnd);
    source.Content(wuxc::Grid{});Pump();
    shellactions::commands={{shellactions::Pin::Start,L"PinToStartScreen",L"Native localized pin"},
        {shellactions::Pin::Untaskbar,L"taskbarunpin",L"Native localized unpin",false}};
    wuxc::MenuFlyout menu;wuxc::MenuFlyoutItem open;open.Text(L"Open");menu.Items().Append(open);
    AddShellMenuItems(menu,rowTarget,rowPackage,{},[]{return current;});
    Check(menu.Items().Size()==5,"shell actions follow one separator and preserve existing menu entries");
    auto pin=menu.Items().GetAt(2).as<wuxc::MenuFlyoutItem>();auto disabled=menu.Items().GetAt(3).as<wuxc::MenuFlyoutItem>();
    auto settings=menu.Items().GetAt(4).as<wuxc::MenuFlyoutItem>();
    Check(pin.Text()==L"Native localized pin"&&settings.Text()==L"App settings","native pin labels and translated App settings label reach real XAML items");
    Check(!disabled.IsEnabled()&&disabled.Icon(),"native disabled state and action icons are retained");
    Invoke(pin);Check(pinCalls==1&&invokedTarget==L"old-target"&&invokedVerb==L"PinToStartScreen","the chosen XAML pin action forwards its exact target and verb");
    shellactions::commands[0].target=L"C:\\Original shortcut.lnk";
    wuxc::MenuFlyout shortcutMenu;AddShellMenuItems(shortcutMenu,rowTarget,{},{});
    Invoke(shortcutMenu.Items().GetAt(0).as<wuxc::MenuFlyoutItem>());
    Check(pinCalls==2&&invokedTarget==L"C:\\Original shortcut.lnk","fallback pinning uses the original shortcut instead of the app ID or executable");
    Check(dismissed==0,"Pin and Unpin keep the search window open");
    Invoke(settings);Check(opened==shellactions::settingsArguments&&dismissed==1,"App settings launches its validated native page arguments through normal launch dismissal");
    current=false;Invoke(pin);Invoke(settings);
    Check(pinCalls==2&&dismissed==1,"recycled rows cannot trigger stale pinning or Settings actions");
    current=true;shellactions::commands.clear();shellactions::settingsArguments.clear();
    wuxc::MenuFlyout unsupported;AddShellMenuItems(unsupported,L"unsupported",L"unsupported");
    Check(unsupported.Items().Size()==0,"unsupported targets add no actions or empty separators");
    shellactions::commands={{shellactions::Pin::Start,L"PinToStartScreen",L"Pin"}};
    shellactions::settingsArguments=L"valid";reenter=true;
    wuxc::MenuFlyout replaced;AddShellMenuItems(replaced,rowTarget,rowPackage,{},[]{return current;});
    Check(observedTarget==L"old-target"&&observedPackage==L"old-package","shell reentrancy cannot mix metadata from two different recycled rows");
    Check(replaced.Items().Size()==0,"a target changed during native menu discovery exposes no stale actions");
    current=true;reenter=false;shellactions::commands.clear();
    wuxc::MenuFlyout inventory;AddShellMenuItems(inventory,{},L"supported-package",L"exact-version",[]{return current;});
    Check(inventory.Items().Size()==1&&inventory.Items().GetAt(0).as<wuxc::MenuFlyoutItem>().Text()==L"App settings",
          "installed inventory can expose App settings without a launch or pin option");
    handlers.clear();source.Content(nullptr);source.Close();DestroyWindow(hwnd);manager.Close();
    printf("PASS: %d native shell-menu UI checks; no real shell actions executed\n",checks);
}catch(winrt::hresult_error const& error){printf("XAML error %08X\n",unsigned(error.code()));return 1;}
