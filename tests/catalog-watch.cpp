#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <roapi.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>
template<class... T>void Wh_Log(T&&...){}
int refreshes=0;bool inventory=false;
void InvalidateAppsCatalog(bool changed){++refreshes;inventory=changed;}
// PRODUCTION_WATCH
AppCatalogWatch* active=nullptr;
LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam){
    if(message==AppCatalogWatch::ChangedMessage){if(active)active->Changed(wParam!=0);return 0;}
    if(message==AppCatalogWatch::ShellMessage){if(active)active->Shell(wParam,lParam);return 0;}
    if(message==WM_TIMER && wParam==AppCatalogWatch::Timer){if(active)active->Tick();return 0;}
    return DefWindowProcW(hwnd,message,wParam,lParam);
}
int checks=0;
void Check(bool value,const char* label){++checks;if(!value){std::printf("FAIL: %s\n",label);ExitProcess(1);}}
bool Pump(const std::function<bool()>& ready,DWORD timeout=5000){
    const auto began=GetTickCount64();
    do{MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(ready())return true;MsgWaitForMultipleObjectsEx(0,nullptr,20,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    }while(GetTickCount64()-began<timeout);return ready();
}
int main(){
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    CatalogChangeBatch batch;bool changed=false;
    batch.Add(1000,false);Check(batch.due==2000 && !batch.inventory,"shortcut changes settle for one second");
    batch.Add(1500,true);Check(batch.due==2500 && batch.inventory,"registry/package changes preserve their inventory-invalidating flag");
    Check(!batch.Take(2499,changed) && batch.Take(2500,changed) && changed && !batch.dirty,"a settled burst emits exactly one refresh");
    for(ULONGLONG now=1000;now<=5900;now+=100)batch.Add(now,false);
    Check(batch.due==6000 && batch.Take(6000,changed),"long installer bursts cannot defer refresh indefinitely");
    WNDCLASSW type{};type.lpfnWndProc=Proc;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"StartEverything.CatalogWatch.Test";
    RegisterClassW(&type);const auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,type.lpszClassName,L"Hidden catalog listener test",WS_POPUP,0,0,0,0,nullptr,nullptr,type.hInstance,nullptr);
    Check(hwnd!=nullptr,"native listener test host starts without XAML");
    wchar_t temp[32768]{},exe[32768]{};GetTempPathW(std::size(temp),temp);GetModuleFileNameW(nullptr,exe,std::size(exe));
    const auto folder=std::wstring(temp)+L"catalog-watch-"+std::to_wstring(GetCurrentProcessId());
    const auto child=folder+L"\\nested";CreateDirectoryW(folder.c_str(),nullptr);CreateDirectoryW(child.c_str(),nullptr);
    const auto keyName=L"Software\\StartEverythingTests\\CatalogWatch-"+std::to_wstring(GetCurrentProcessId());
    HKEY key=nullptr;Check(RegCreateKeyExW(HKEY_CURRENT_USER,keyName.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&key,nullptr)==ERROR_SUCCESS,"isolated volatile registry fixture opens outside the real app catalog");
    {
        AppCatalogWatch watch;active=&watch;watch.Start(hwnd);
        std::printf("Native listeners: registry=%zu shortcuts=%zu packages=%d\n",watch.RegistryCount(),watch.ShortcutCount(),watch.PackagesActive());
        Check(watch.RegistryCount()>=2 && watch.ShortcutCount()==2,"desktop registration and both Start-menu roots are monitored");
        Check(watch.PackagesActive(),"Store/MSIX install, update and uninstall subscriptions are active on this PC");
        const auto count=watch.RegistryCount();watch.Start(hwnd);Check(watch.RegistryCount()==count,"repeated startup never duplicates registry wait operations");
        Check(watch.Watch(HKEY_CURRENT_USER,0,keyName),"production registry watcher can watch the private test key");
        for(DWORD value=0;value<12;++value)RegSetValueExW(key,L"sample",0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));
        const auto before=refreshes;Check(Pump([&]{return refreshes>before;}),"native registry writes trigger a silent debounced catalog refresh");
        Check(inventory,"registry notifications bypass the installed-inventory cache");
        PIDLIST_ABSOLUTE pidl=nullptr;Check(SUCCEEDED(SHParseDisplayName(folder.c_str(),nullptr,&pidl,0,nullptr)),"private shortcut folder resolves");
        SHChangeNotifyEntry entry{pidl,TRUE};const auto subscription=SHChangeNotifyRegister(hwnd,SHCNRF_ShellLevel|SHCNRF_InterruptLevel|SHCNRF_RecursiveInterrupt|SHCNRF_NewDelivery,
            SHCNE_CREATE|SHCNE_DELETE|SHCNE_RENAMEITEM|SHCNE_UPDATEITEM,AppCatalogWatch::ShellMessage,1,&entry);
        CoTaskMemFree(pidl);Check(subscription!=0,"production delivery format registers recursive private shortcut notifications");watch.shell_.push_back(subscription);
        const auto link=child+L"\\fixture.lnk";
        winrt::com_ptr<IShellLinkW> shortcut;Check(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(shortcut.put()))),"shortcut metadata writer starts");
        shortcut->SetPath(exe);auto persist=shortcut.as<IPersistFile>();Check(SUCCEEDED(persist->Save(link.c_str(),TRUE)),"private shortcut is created without launching it");
        const auto beforeLink=refreshes;
        SHChangeNotify(SHCNE_CREATE,SHCNF_PATHW,link.c_str(),nullptr);
        Check(Pump([&]{return refreshes>beforeLink;}),"a nested shortcut addition triggers the Shell listener");
        const auto renamed=child+L"\\renamed.lnk";MoveFileW(link.c_str(),renamed.c_str());
        const auto beforeRename=refreshes;SHChangeNotify(SHCNE_RENAMEITEM,SHCNF_PATHW,link.c_str(),renamed.c_str());
        Check(Pump([&]{return refreshes>beforeRename;}),"shortcut renames update the catalog");
        std::atomic<bool> entered=false;
        std::thread callback([&]{AppCatalogWatch::Lease lease(watch.gate_);entered=true;Sleep(100);});
        while(!entered)Sleep(1);
        const auto stopStarted=GetTickCount64();watch.Stop();callback.join();
        Check(GetTickCount64()-stopStarted>=50,"unload drains an in-flight package callback before releasing listener state");
        Check(watch.RegistryCount()==0 && watch.ShortcutCount()==0 && !watch.PackagesActive(),"shutdown releases registry waits, Shell registrations and package event revokers");
        const auto stoppedAt=refreshes;DWORD value=99;RegSetValueExW(key,L"sample",0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));
        DeleteFileW(renamed.c_str());SHChangeNotify(SHCNE_DELETE,SHCNF_PATHW,renamed.c_str(),nullptr);
        Pump([]{return false;},1300);Check(refreshes==stoppedAt,"stopped listeners never request another catalog refresh");
        for(int cycle=0;cycle<3;++cycle){watch.Start(hwnd);Check(watch.PackagesActive() && watch.RegistryCount()>=2,"listeners survive repeated enable/disable cycles");watch.Stop();}
        active=nullptr;
    }
    RegCloseKey(key);RegDeleteKeyW(HKEY_CURRENT_USER,keyName.c_str());RemoveDirectoryW(child.c_str());RemoveDirectoryW(folder.c_str());
    DestroyWindow(hwnd);UnregisterClassW(type.lpszClassName,type.hInstance);winrt::uninit_apartment();
    std::printf("PASS: %d catalog listener checks\n",checks);
}
