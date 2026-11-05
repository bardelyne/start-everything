#define NOMINMAX
#include <windows.h>
#include <winsvc.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cwctype>
#include <functional>
#include <initializer_list>
#include <limits>
#include <locale>
#include <map>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
// PRODUCTION_I18N
// PRODUCTION_COMMANDS
enum class CustomCommandShell { Cmd };
// PRODUCTION_MODEL
// PRODUCTION_BASE64
namespace tools {std::wstring ToLower(std::wstring value){std::transform(value.begin(),value.end(),value.begin(),towlower);return value;}}
std::mutex g_queryMutex,g_resultsMutex;
std::condition_variable g_queryWake;
std::atomic<bool> g_queryDirty{false};
std::wstring g_pendingQuery=L"/services",g_appsQuery;
uint64_t g_appsRevision=0,g_appsContentRevision=0;
std::atomic<bool> g_searchQuit{false};uint64_t g_queryRevision=42;
std::vector<Row> g_appRows;std::atomic<int> renders{0};
// PRODUCTION_BATCH_SIZES
// PRODUCTION_BATCHES
// PRODUCTION_PUBLISH
void RequestRender(){++renders;}
void Wh_Log(const wchar_t*,...){}
namespace services {enum class Action {Start,Stop,Restart};}
// PRODUCTION_SERVICES
int checks=0;
void Check(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}++checks;}
int main(int argc,char** argv){
    i18n::Load(L"en");
    if (argc > 1 && std::string(argv[1]) == "--exit-with-idle-worker") {
        // No SCM request or operation is made: the worker waits while hidden.
        // Returning without controlled unload exercises process-shutdown CRT
        // destruction. A joinable global std::thread without no_destroy aborts.
        services::Schedule(L"/services", L"", 42, 20);
        Check(services::g_thread && services::g_thread->joinable(), "shutdown fixture owns a live worker");
        return 0;
    }
    if(argc>1 && std::string(argv[1])=="--fixture-check"){
        wchar_t path[MAX_PATH]{};GetModuleFileNameW(nullptr,path,MAX_PATH);
        const auto exe=std::wstring(path);const auto binary=exe.substr(0,exe.find_last_of(L"\\/")+1)+L"service-fixture.exe";
        const auto name=L"StartEverythingTest_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64());
        services::ScHandle manager(OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CREATE_SERVICE));
        if(!manager.value){std::fprintf(stderr,"Fixture test needs Windows administrator permission\n");return 2;}
        const auto command=L"\""+binary+L"\"";
        services::ScHandle service(CreateServiceW(manager.value,name.c_str(),name.c_str(),SERVICE_QUERY_STATUS|SERVICE_STOP|DELETE,
            SERVICE_WIN32_OWN_PROCESS,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,command.c_str(),nullptr,nullptr,nullptr,L"NT AUTHORITY\\LocalService",nullptr));
        if(!service.value){std::fprintf(stderr,"Cannot create temporary fixture: %lu\n",GetLastError());return 1;}
        int result=0;DWORD firstPid=0;
        for(auto action:{services::Action::Start,services::Action::Restart,services::Action::Stop}){
            HANDLE process=nullptr;DWORD code=ERROR_GEN_FAILURE;
            try{process=services::Launch({name,action});}catch(DWORD error){code=error;}
            if(process){if(WaitForSingleObject(process,60000)==WAIT_OBJECT_0)GetExitCodeProcess(process,&code);CloseHandle(process);}
            SERVICE_STATUS_PROCESS status{};DWORD needed=0;
            const bool queried=QueryServiceStatusEx(service.value,SC_STATUS_PROCESS_INFO,reinterpret_cast<BYTE*>(&status),sizeof(status),&needed);
            const auto expected=action==services::Action::Stop?SERVICE_STOPPED:SERVICE_RUNNING;
            if(code || !queried || status.dwCurrentState!=expected){std::fprintf(stderr,"Fixture operation failed: action %d, code %lu, state %lu\n",int(action),code,status.dwCurrentState);result=1;break;}
            ++checks;
            if(action==services::Action::Start)firstPid=status.dwProcessId;
            if(action==services::Action::Restart){if(!status.dwProcessId || status.dwProcessId==firstPid){result=1;break;}++checks;}
        }
        // Cleanup is restricted to the handle returned by this CreateService.
        SERVICE_STATUS stopped{};ControlService(service.value,SERVICE_CONTROL_STOP,&stopped);
        const auto until=GetTickCount64()+5000;
        do{QueryServiceStatus(service.value,&stopped);if(stopped.dwCurrentState==SERVICE_STOPPED)break;Sleep(50);}while(GetTickCount64()<until);
        if(!DeleteService(service.value)){std::fprintf(stderr,"Cannot remove temporary fixture: %lu\n",GetLastError());result=1;}
        if(!result)std::printf("PASS: %d start/stop/restart checks against a temporary LocalService fixture; fixture removed\n",checks);
        return result;
    }
    if(argc>1 && std::string(argv[1])=="--helper-check"){
        const services::Operation operation{L"StartEverything_Nonexistent_Service_87a13ca8425d",services::Action::Start};
        HANDLE process=services::Launch(operation);
        Check(WaitForSingleObject(process,25000)==WAIT_OBJECT_0,"hidden helper finishes without a service modification or elevation prompt");
        DWORD code=0;GetExitCodeProcess(process,&code);CloseHandle(process);
        Check(code==ERROR_SERVICE_DOES_NOT_EXIST,"helper parses correctly and returns the actual Windows error for a nonexistent service");
        std::printf("PASS: %d read-only helper checks\n",checks);return 0;
    }
    const auto parsed=commands::Parse(L"/services audio",commands::Names{});
    Check(commands::Parse(L"/all",commands::Names{}).kind==commands::Kind::Installed,
          "the complete installed inventory has its own command");
    Check(commands::Parse(L"/ALL\toffice",commands::Names{}).args==L"office",
          "installed inventory accepts case-insensitive mode and a product filter");
    Check(commands::Parse(L"/all installed tools",commands::Names{}).args==L"installed tools",
          "all words after the inventory command remain literal search terms");
    {auto localized=commands::Names{};localized.installed=L"全部";
     Check(commands::Parse(L"/全部 office",localized).kind==commands::Kind::Installed,
           "the installed inventory command can use a localized single-word name");
     localized.services=localized.installed;
     Check(commands::Parse(L"/全部",localized).kind==commands::Kind::Conflict,
           "inventory command names share the existing tool collision protection");}
    {struct Custom{std::wstring name,command;};
     Check(commands::Parse(L"/all",commands::Names{},std::vector<Custom>{{L"/all",L"echo"}}).kind==commands::Kind::Conflict,
           "a custom command cannot override the installed-inventory command");}
    Check(parsed.kind==commands::Kind::Services && parsed.args==L"audio","services command splits its search argument");
    auto names=commands::Names{};names.services=L"服務";
    Check(commands::Parse(L"/服務 音訊",names).kind==commands::Kind::Services,"custom command name supports Unicode");
    names.ip=names.services;
    Check(commands::Parse(L"/服務",names).kind==commands::Kind::Conflict,"duplicate services command names are rejected before scheduling");
    std::vector<services::Entry> entries{{L"Audiosrv",L"Windows Audio",L"Sound playback",SERVICE_RUNNING,SERVICE_ACCEPT_STOP,123},
        {L"AudioEndpointBuilder",L"Endpoint Builder",L"Sound device endpoints",SERVICE_STOPPED,0,0},
        {L"Protected",L"Protected service",L"Cannot be stopped",SERVICE_RUNNING,0,1},
        {L"Paused",L"Paused service",L"",SERVICE_PAUSED,SERVICE_ACCEPT_STOP,2}};
    auto rows=services::Rows(L"audio",20,entries,true,0,false,{});
    Check(rows.size()==2 && std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.serviceName==L"Audiosrv";}),"service names are searched as well as display names");
    Check(services::Rows(L"audiosrv",20,entries,true,0,false,{})[0].serviceName==L"Audiosrv","exact internal-name search selects the matching service");
    rows=services::Rows(L"sound playback",20,entries,true,0,false,{});
    Check(rows.size()==1 && rows[0].serviceName==L"Audiosrv","multiple words can match a service description");
    rows=services::Rows(L"running",20,entries,true,0,false,{});
    Check(rows.size()==2 && rows[0].serviceState==SERVICE_RUNNING,"running filter excludes stopped and paused services");
    Check(services::Rows(L"stopped",20,entries,true,0,false,{}).size()==1,"stopped filter works locally");
    Check(services::Rows(L"",1,entries,true,0,false,{}).size()==1,"service results obey the requested batch size");
    std::vector<services::Entry> many;
    for(int i=0;i<165;++i) {
        const auto name=L"Fixture"+std::to_wstring(1000+i);
        many.push_back({name,name,L"Fixture description",static_cast<DWORD>(i%2?SERVICE_RUNNING:SERVICE_STOPPED),
                        SERVICE_ACCEPT_STOP,static_cast<DWORD>(i)});
    }
    g_appsBatch.Begin(42,20);
    size_t prior=0;
    std::vector<std::wstring> identities;
    while(true) {
        const auto requested=g_appsBatch.requested;
        auto batch=services::Rows(L"",requested+1,many,true,0,false,{});
        const bool more=batch.size()>requested;
        if(more)batch.resize(requested);
        for(size_t i=0;i<identities.size();++i)
            Check(batch[i].serviceName==identities[i],"appending services preserves the existing prefix and targets");
        identities.clear();
        for(const auto& row:batch)identities.push_back(row.serviceName);
        Check(PublishAppResults(42,L"/services",std::move(batch),more,requested),
              "current service batch publishes through the production guard");
        Check(g_appRows.size()==std::min(requested,many.size()),"service batch grows beyond the former 100-row cap");
        if(!more)break;
        prior=g_appRows.size();
        Check(RequestMoreApps(L"/services",prior,42),"near-end scrolling requests another service batch");
        Check(g_appsBatch.requested==requested+40 && g_queryDirty,"each scroll adds 40 on the worker without changing the query revision");
        Check(!RequestMoreApps(L"/services",prior,42),"duplicate scroll events do not stack pending requests");
        Check(!PublishAppResults(42,L"/services",g_appRows,true,requested),
              "a live refresh of the previous smaller batch cannot overwrite the new scrolling request");
        Check(g_appsBatch.pending && g_appRows.size()==prior,"rejecting a stale refresh preserves visible rows and the pending request");
    }
    Check(g_appRows.size()==165 && !g_appsBatch.more,"every matching service can be reached and the last partial batch marks the end");
    Check(!RequestMoreApps(L"/services",165,42),"scrolling at the end does not schedule more service work");
    auto runningRows=services::Rows(L"running",200,many,true,0,false,{});
    Check(runningRows.size()==82 && std::all_of(runningRows.begin(),runningRows.end(),[](const auto& row){return row.serviceState==SERVICE_RUNNING;}),
          "service state filters apply before pagination");
    Check(services::Rows(L"fixture1164",20,many,true,0,false,{})[0].serviceName==L"Fixture1164",
          "filtering can find a service beyond the initially visible pages");
    Check(!PublishAppResults(41,L"/services",{},false,20),"an old query cannot publish service results");
    g_appsBatch={};g_appsBatch.Begin(42,20);g_appRows.clear();
    const auto noMatch=services::Rows(L"not found",20,entries,true,0,false,{})[0];
    Check(noMatch.infoOnly && noMatch.serviceName.empty() && noMatch.title==L"No matching services","no-match row has a service-specific message and no operation target");
    Check(services::Rows(L"",20,{},false,0,false,{})[0].title==L"Loading…","initial query can render before enumeration");
    Check(services::Rows(L"",20,{},true,ERROR_ACCESS_DENIED,false,{})[0].isError,"enumeration errors are shown rather than a misleading empty list");
    rows=services::Rows(L"audiosrv",20,entries,true,0,true,{{L"Audiosrv",ERROR_DEPENDENT_SERVICES_RUNNING}});
    Check(rows[0].serviceBusy && rows[0].isError && rows[0].subtitle.find(L"1051")!=std::wstring::npos,"busy status and Windows dependency errors remain visible");
    Check(rows[0].copyText.find(L"Audiosrv")!=std::wstring::npos && rows[0].subtitle.find(L"123")!=std::wstring::npos,"copy details include stable service name and process ID");
    for(auto state:{SERVICE_STOPPED,SERVICE_RUNNING,SERVICE_PAUSED,SERVICE_START_PENDING,SERVICE_STOP_PENDING}){
        Check(services::Allowed(services::Action::Start,state,SERVICE_ACCEPT_STOP)==(state==SERVICE_STOPPED),"start availability follows current state");
        const bool stoppable=state==SERVICE_RUNNING || state==SERVICE_PAUSED;
        Check(services::Allowed(services::Action::Stop,state,SERVICE_ACCEPT_STOP)==stoppable,"stop requires a stable active state");
        Check(services::Allowed(services::Action::Restart,state,SERVICE_ACCEPT_STOP)==stoppable,"restart requires an active service that accepts stop");
    }
    Check(!services::Allowed(services::Action::Stop,SERVICE_RUNNING,0),"services that do not accept stop cannot be stopped from the menu");
    const std::wstring hostile=L"Test'; Write-Output 'injected'; #服務";
    const auto script=services::OperationScript({hostile,services::Action::Restart});
    Check(script.find(hostile)==script.npos && script.find(EncodePowerShellCommand(hostile))!=script.npos,"service names stay encoded as data rather than shell instructions");
    Check(script.find(L"Join-Path ([Environment]::SystemDirectory) 'sc.exe'")!=script.npos && script.find(L"& $sc stop $n")!=script.npos,"helper uses the absolute system sc executable and a separate name argument");
    Check(script.find(L"-Verb RunAs")!=script.npos && script.find(L"if($c -eq 5)")!=script.npos,"elevation is requested only after Windows denies an explicit operation");
    Check(script.find(L"-Force")==script.npos && script.find(L"Stop-Service")==script.npos && script.find(L" config ")==script.npos,"operations never force dependent services down or change startup configuration");
    bool rejected=false;try{services::OperationScript({L"x",static_cast<services::Action>(99)});}catch(DWORD){rejected=true;}
    Check(rejected,"unrecognized operation values cannot become commands");
    rejected=false;try{services::OperationScript({L"",services::Action::Start});}catch(DWORD){rejected=true;}
    Check(rejected,"empty service names are rejected");
    std::vector<services::Entry> live;
    try{live=services::Enumerate();}catch(DWORD error){std::fprintf(stderr,"Read-only SCM enumeration needs host access (Windows error %lu)\n",error);return 2;}
    Check(!live.empty(),"native read-only enumeration returns local Windows services");
    Check(std::all_of(live.begin(),live.end(),[](const auto& e){return !e.name.empty() && !e.display.empty() && e.state>=1 && e.state<=7;}),"enumerated services have valid names and native states");
    services::Visible(true);services::Schedule(L"/services",L"",42,20);
    for(int i=0;i<200 && !renders.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Check(renders.load()>0,"service enumeration publishes asynchronously");
    {std::scoped_lock lock(g_queryMutex,g_resultsMutex);Check(!g_appRows.empty() && !g_appRows.front().serviceName.empty(),"worker publishes actionable service cards");}
    const auto visible=std::min(size_t(20),live.size());
    if(live.size()>visible) {
        Check(RequestMoreApps(L"/services",visible,42),"a live service list can request its next scroll batch");
        const auto expanded=std::min(size_t(60),live.size());
        services::Schedule(L"/services",L"",42,60);
        bool ready=false;
        for(int i=0;i<200 && !ready;++i) {
            {std::scoped_lock lock(g_queryMutex,g_resultsMutex);ready=g_appRows.size()==expanded;}
            if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Check(ready,"the actual service worker publishes the expanded batch without typing again");
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        {std::scoped_lock lock(g_queryMutex,g_resultsMutex);
         Check(g_appRows.size()==expanded && g_appsBatch.published==expanded && !g_appsBatch.pending,
               "the periodic live service refresh preserves all loaded rows");}
    }
    {std::lock_guard lock(g_queryMutex);++g_queryRevision;}
    services::Visible(false);services::Wake();const auto old=renders.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    Check(renders.load()==old,"hidden or stale requests cannot overwrite the app panel");
    const auto begin=GetTickCount64();g_searchQuit=true;services::Wake();services::g_thread->join();services::g_thread.reset();
    Check(GetTickCount64()-begin<1000,"idle service worker stops promptly when the mod unloads");
    std::printf("PASS: %d service checks; %zu local services enumerated read-only\n",checks,live.size());
}
