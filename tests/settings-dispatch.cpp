// Run the production settings callback and helper WndProc on real threads.
// Only a private message-only window and a catalog lifetime fixture are used.
#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <thread>
static std::atomic<unsigned> checks{0},loads{0},refreshes{0},uiUpdates{0};
static void Check(bool ok,const char* label){if(!ok){std::fprintf(stderr,"FAIL: %s\n",label);std::_Exit(1);}++checks;}
template<class... T>void Wh_Log(T&&...){}
static DWORD owner=0;
struct Warmup {
    ULONGLONG BuiltAt(){Check(GetCurrentThreadId()==owner,"catalog reads stay on the helper's owning thread");return GetTickCount64()-6000;}
    void Refresh(bool=false){Check(GetCurrentThreadId()==owner,"catalog refresh stays on its owning thread");++refreshes;}
    ~Warmup(){Check(GetCurrentThreadId()==owner,"catalog destruction stays on its owning thread");}
};
std::optional<Warmup> g_appsWarmup;
// PRODUCTION_REFRESH
enum class TargetProcess{Explorer,StartMenu,SearchHost};
TargetProcess g_targetProcess=TargetProcess::Explorer;
std::atomic<HWND> g_hExplorerHelperWnd{nullptr};
void LoadSettings(){++loads;}
constexpr UINT kExplorerUpdateShortcut=WM_APP+0x34,kExplorerOpenSearch=WM_APP+0x35,
 kSearchWindowInputMessage=WM_APP+0x36,kExplorerSearchShown=WM_APP+0x37;
constexpr UINT_PTR kAlternativeShortcutId=3,kExplorerSearchStartupTimer=4,kSearchAloneTimerId=5,kSearchAloneTicks=6;
std::atomic<unsigned> g_alternativeShortcutActive{0};
bool g_searchAloneArmed=false;
struct AppCatalogWatch {
 static constexpr UINT ChangedMessage=WM_APP+0x40,ShellMessage=WM_APP+0x41;
 static constexpr UINT_PTR Timer=7;
 void Changed(bool){}void Shell(WPARAM,LPARAM){}void Tick(){}
 static void ReleaseShellMessage(WPARAM,LPARAM){}
};
AppCatalogWatch* g_appCatalogWatch=nullptr;
void UpdateAlternativeShortcut(HWND){}void RefreshSearchUiSettings(){++uiUpdates;}
unsigned PackAlternativeShortcut(unsigned,unsigned){return 0;}
void DismissSearchContextMenu(){}void RequestExplorerSearch(HWND){}
bool HasBufferedSearchWindowInput(){return false;}bool IsTaskbarProcess(){return false;}
bool SearchActivationSettling(){return false;}void ArmSearchAloneCheck(unsigned){}
void TryInitializeExplorerSearch(HWND){}bool CheckSearchShownWithoutStart(){return false;}
void StopAppCatalogWatch(){}void ReleaseAlternativeShortcut(HWND){}
// PRODUCTION_HELPER
// PRODUCTION_SETTINGS
constexpr UINT kFailStartup=WM_APP+0x60,kRecreate=WM_APP+0x61;
HANDLE ready=nullptr,teardownEntered=nullptr,allowTeardown=nullptr,recreated=nullptr;
LRESULT CALLBACK TestWndProc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(message==kFailStartup){
  SetEvent(teardownEntered);Check(WaitForSingleObject(allowTeardown,2000)==WAIT_OBJECT_0,"failed-startup fixture can finish teardown");
  g_appsWarmup.reset();return 0;
 }
 if(message==kRecreate){g_appsWarmup.emplace();SetEvent(recreated);return 0;}
 return ExplorerHelperWndProc(window,message,w,l);
}
static void WaitFor(const std::atomic<unsigned>& value,unsigned minimum,const char* label){
 const auto end=GetTickCount64()+2000;while(value<minimum&&GetTickCount64()<end)Sleep(1);Check(value>=minimum,label);
}
int main(){
 ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);teardownEntered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 allowTeardown=CreateEventW(nullptr,TRUE,FALSE,nullptr);recreated=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 Check(ready&&teardownEntered&&allowTeardown&&recreated,"create private synchronization fixtures");
 std::thread helper([]{
  owner=GetCurrentThreadId();WNDCLASSW cls{};cls.lpfnWndProc=TestWndProc;cls.hInstance=GetModuleHandleW(nullptr);cls.lpszClassName=L"StartEverythingSettingsDispatchTest";
  Check(RegisterClassW(&cls)!=0,"register the private helper fixture");
  const auto window=CreateWindowExW(0,cls.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,cls.hInstance,nullptr);
  Check(window!=nullptr,"create a hidden message-only helper");g_appsWarmup.emplace();g_hExplorerHelperWnd=window;SetEvent(ready);
  MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
  g_appsWarmup.reset();UnregisterClassW(cls.lpszClassName,cls.hInstance);
 });
 Check(WaitForSingleObject(ready,2000)==WAIT_OBJECT_0,"helper becomes ready on its own thread");
 const auto window=g_hExplorerHelperWnd.load();
 Wh_ModSettingsChanged();WaitFor(uiUpdates,1,"normal settings updates reach the helper");
 Check(refreshes==1&&loads==1,"normal settings refresh the catalog once");
 PostMessageW(window,kFailStartup,0,0);
 Check(WaitForSingleObject(teardownEntered,2000)==WAIT_OBJECT_0,"helper enters a paused failed-startup teardown");
 const auto began=GetTickCount64();Wh_ModSettingsChanged();
 Check(GetTickCount64()-began<100,"settings callback stays nonblocking during teardown");
 Check(refreshes==1,"settings callback does not read the warmup while its owner is tearing it down");
 SetEvent(allowTeardown);WaitFor(uiUpdates,2,"queued settings update runs after destruction");
 Check(refreshes==1,"queued refresh safely ignores the destroyed optional catalog");
 for(int i=0;i<100;++i)Wh_ModSettingsChanged();WaitFor(uiUpdates,102,"a burst of queued settings updates drains safely without a catalog");
 Check(refreshes==1,"no destroyed worker is used by queued settings changes");
 PostMessageW(window,kRecreate,0,0);Check(WaitForSingleObject(recreated,2000)==WAIT_OBJECT_0,"catalog ownership can be recreated after failed startup");
 Wh_ModSettingsChanged();WaitFor(uiUpdates,103,"settings work again after catalog recreation");Check(refreshes==2,"new catalog refreshes on its owner thread");
 g_targetProcess=TargetProcess::StartMenu;const auto loaded=loads.load();Wh_ModSettingsChanged();
 Check(loads==loaded+1&&refreshes==2,"Start host reloads settings without touching Explorer's catalog");
 g_targetProcess=TargetProcess::SearchHost;Wh_ModSettingsChanged();Check(loads==loaded+1,"unrelated search host ignores this callback");
 g_targetProcess=TargetProcess::Explorer;g_hExplorerHelperWnd=nullptr;Wh_ModSettingsChanged();Check(refreshes==2,"missing helper never falls back to an unsafe direct catalog call");
 PostMessageW(window,WM_CLOSE,0,0);helper.join();Check(!g_appsWarmup,"fixture releases catalog ownership before exiting");
 for(auto event:{ready,teardownEntered,allowTeardown,recreated})CloseHandle(event);
 std::printf("PASS: %u settings dispatch, owner-thread and failed-startup race checks\n",checks.load());
}
