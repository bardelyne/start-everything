#define NOMINMAX
#include <windows.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <functional>
#include <vector>
#include <map>
#include <optional>
#include <string>
#include <algorithm>
using namespace std::chrono_literals;
template<class... T>void Wh_Log(T&&...){}
namespace apps {
std::mutex gate;
std::condition_variable gateWake;
bool blocked=false,entered=false,fail=false,throws=false;
std::atomic<int> rebuilds{0};
struct App {std::wstring name=L"test";bool isSetting=false;void* pidl=nullptr;};
const std::wstring& IconIdentity(const App& app){return app.name;}
struct Index {
    bool Rebuild(){++rebuilds;std::unique_lock lock(gate);entered=true;gateWake.notify_all();gateWake.wait(lock,[]{return !blocked;});if(throws)throw 1;return !fail;}
    size_t Count()const{return 42;}
    const App* At(size_t)const{return nullptr;}
    std::shared_ptr<Index> Refreshed(bool)const{auto next=std::make_shared<Index>();return next->Rebuild()?next:nullptr;}
};
// PRODUCTION_WARMUP
}
int checks=0;
void Check(bool ok,const char* what){if(!ok){printf("FAIL: %s\n",what);std::exit(1);}++checks;}
void Entered(){std::unique_lock lock(apps::gate);Check(apps::gateWake.wait_for(lock,2s,[]{return apps::entered;}),"background build reaches its controlled provider");}
void Reset(bool blocked=false,bool fail=false,bool throws=false){std::lock_guard lock(apps::gate);apps::blocked=blocked;apps::fail=fail;apps::throws=throws;apps::entered=false;apps::rebuilds=0;}
void Release(){std::lock_guard lock(apps::gate);apps::blocked=false;apps::gateWake.notify_all();}

std::atomic<bool> g_quit{false};
constexpr UINT_PTR kExplorerSearchStartupTimer=2;
unsigned g_explorerTaskbarWaitTicks=0,g_explorerSearchStartupAttempts=0;
bool g_explorerSearchRequested=false,taskbarPresent=true,taskbarOwned=true,xamlReady=false;
std::atomic<HWND> g_searchWindowWindow{nullptr};
int warmCalls=0,uiCreates=0,shows=0,appliedInput=0;
apps::WarmIndex startupWarmup;
HWND FakeFindWindow(LPCWSTR,LPCWSTR){return taskbarPresent?reinterpret_cast<HWND>(1):nullptr;}
DWORD FakeThread(HWND,DWORD* pid){if(pid)*pid=taskbarOwned?GetCurrentProcessId():GetCurrentProcessId()+1;return 1;}
UINT_PTR FakeTimer(HWND,UINT_PTR,UINT,TIMERPROC){return 1;}
BOOL FakeKillTimer(HWND,UINT_PTR){return TRUE;}
void StartAppsWarmup(){++warmCalls;startupWarmup.Start();}
void StartAppCatalogWatch(HWND){}
void UpdateAlternativeShortcut(HWND){}
void SyncSearchWindowInputForForeground(HWND){}
bool TaskbarXamlReady(HWND){return xamlReady;}
bool SearchWindowInitialize(){++uiCreates;g_searchWindowWindow=reinterpret_cast<HWND>(2);return true;}
void StopSearchWindowInput(){}
bool HasBufferedSearchWindowInput(){return true;}
void ApplySearchWindowInput(){++appliedInput;}
void SearchWindowShow(){++shows;}
#define FindWindowW FakeFindWindow
#define GetWindowThreadProcessId FakeThread
#define SetTimer FakeTimer
#define KillTimer FakeKillTimer
// PRODUCTION_STARTUP
#undef FindWindowW
#undef GetWindowThreadProcessId
#undef SetTimer
#undef KillTimer

int main(){
    apps::WarmIndex warm;
    Check(!warm.Take().index,"unstarted warmup returns immediately so ordinary search can fall back");
    warm.Stop();
    Reset(true);const auto began=GetTickCount64();
    Check(warm.Start() && GetTickCount64()-began<500,"starting warmup does not wait for catalog I/O");Entered();
    Check(warm.Start() && apps::rebuilds==1,"repeated startup checks do not create duplicate catalog workers");
    std::atomic<bool> taken{false};apps::WarmIndex::Ready result;
    std::thread consumer([&]{result=warm.Take();taken=true;});
    Sleep(10);Check(!taken,"only the apps worker waits when search opens before warmup completes");
    Release();consumer.join();
    Check(result.index && result.index->Count()==42 && result.builtAt,"search receives the fully built catalog and its timestamp");
    Check(apps::rebuilds==1,"taking the warm catalog never rebuilds it");
    Check(warm.Take().index==result.index,"matching receives an immutable shared snapshot without rebuilding");warm.Stop();
    Check(!warm.BuiltAt(),"unload clears retained startup state");
    Reset();Check(warm.Start(),"headless refresh fixture starts");
    auto previous=warm.Take();const auto version=previous.version;
    Reset(true);warm.Refresh(true);Entered();
    const auto readStarted=GetTickCount64();
    Check(warm.Take().index==previous.index && GetTickCount64()-readStarted<100,"an in-flight refresh never blocks access to an already built snapshot");
    Check(!warm.Newer(version).index,"an incomplete replacement is never published");
    Release();
    const auto refreshStarted=GetTickCount64();while(warm.Version()==version && GetTickCount64()-refreshStarted<2000)Sleep(1);
    auto replacement=warm.Newer(version);
    Check(replacement.index && replacement.index!=previous.index && replacement.version>version,"a catalog change publishes a complete replacement even before any search UI exists");
    Check(previous.index->Count()==42,"an old snapshot stays alive for outstanding readers");
    warm.Stop();
    Reset(false,true);Check(warm.Start(),"failed-provider warmup starts");
    Check(!warm.Take().index,"provider failure leaves the normal first-search rebuild available");warm.Stop();
    Reset(false,false,true);Check(warm.Start(),"throwing-provider warmup starts");
    Check(!warm.Take().index,"provider exception is contained and wakes the waiting search worker");warm.Stop();
    Reset(true);Check(warm.Start(),"unload test starts background work without creating a UI");Entered();
    std::atomic<bool> stopped{false};std::thread stop([&]{warm.Stop();stopped=true;});
    Sleep(10);Check(!stopped,"unload joins in-flight native catalog work instead of abandoning its thread");
    Release();stop.join();Check(stopped && !warm.Take().index,"unload discards unfinished catalog ownership cleanly");
    Reset();
    std::mutex iconMutex;std::condition_variable iconWake;
    bool iconEntered=false,releaseIcon=false;std::atomic<int> iconPublished=0;
    apps::WarmIndex::Handlers handlers;
    handlers.appIcon=[&](const apps::App*,int){std::unique_lock lock(iconMutex);iconEntered=true;iconWake.notify_all();iconWake.wait(lock,[&]{return releaseIcon;});return std::vector<BYTE>(4,255);};
    handlers.iconReady=[&](const std::wstring&,const std::vector<BYTE>&,uint64_t){++iconPublished;};
    Check(warm.Start(std::move(handlers)),"native icon worker fixture starts");
    auto iconOwner=warm.Take().index;apps::App iconApp;
    warm.Prioritize({{iconOwner,&iconApp,L"24:app:test",L"",24,0}});
    {std::unique_lock lock(iconMutex);Check(iconWake.wait_for(lock,2s,[&]{return iconEntered;}),"icon provider is deliberately blocked");}
    const auto cacheStarted=GetTickCount64();
    Check(!warm.Cached(L"24:app:test") && GetTickCount64()-cacheStarted<100,"a slow icon provider never holds the cache lock used by matching");
    stopped=false;std::thread iconStop([&]{warm.Stop();stopped=true;});
    Sleep(10);Check(!stopped,"unload joins an in-flight icon provider");
    {std::lock_guard lock(iconMutex);releaseIcon=true;}iconWake.notify_all();iconStop.join();
    Check(stopped && iconPublished==0,"a finishing icon cannot call back into UI during shutdown");
    Check(!warm.Cached(L"24:app:test"),"shutdown clears retained icon data");
    Reset();const auto helper=reinterpret_cast<HWND>(3);
    taskbarPresent=false;TryInitializeExplorerSearch(helper);
    Check(!warmCalls && !uiCreates,"warmup waits for the taskbar process to exist");
    taskbarPresent=true;taskbarOwned=false;TryInitializeExplorerSearch(helper);
    Check(!warmCalls && !uiCreates,"extra File Explorer processes do not warm their own catalogs");
    taskbarOwned=true;TryInitializeExplorerSearch(helper);
    Check(warmCalls==1 && !uiCreates && !shows,"the taskbar process warms native data while leaving XAML and the search window deferred");
    g_explorerSearchRequested=true;TryInitializeExplorerSearch(helper);
    Check(!uiCreates,"a pending search still waits for Windows taskbar XAML readiness");
    xamlReady=true;TryInitializeExplorerSearch(helper);
    Check(uiCreates==1 && appliedInput==1 && !shows,"first explicit search creates the UI and preserves buffered opening input");
    startupWarmup.Stop();
    printf("PASS: %d app warmup checks\n",checks);
}
