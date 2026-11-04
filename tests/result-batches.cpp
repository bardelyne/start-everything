// Private IPC provider and production paging code; no real Everything queries.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>
HMODULE GetCurrentModuleHandle(){return GetModuleHandleW(nullptr);}
// PRODUCTION_EVERYTHING
// PRODUCTION_RANKER
// PRODUCTION_PAGES
std::mutex g_queryMutex;
std::condition_variable g_queryWake,g_fileQueryWake;
std::atomic<bool> g_searchQuit{false},g_queryDirty{false};
uint64_t g_queryRevision=7;
std::wstring g_pendingQuery=L"files";
// PRODUCTION_BATCH_SIZES
// PRODUCTION_BATCHES

static unsigned checks=0;
void Check(bool ok,const char* label){++checks;if(!ok){printf("FAIL: %s\n",label);ExitProcess(1);}}
struct Read {std::wstring query;DWORD limit,offset;};
std::vector<Read> reads;
std::vector<everything::Result> dataset;
bool failRead=false,current=true;
bool Fetch(const std::wstring& query,DWORD limit,DWORD offset,std::vector<everything::Result>* out,DWORD* total) {
    reads.push_back({query,limit,offset});
    if(failRead)return false;
    *total=static_cast<DWORD>(dataset.size());
    const auto end=std::min(dataset.size(),size_t(offset)+limit);
    for(size_t i=offset;i<end;++i)out->push_back(dataset[i]);
    return true;
}
void Dataset(unsigned count) {
    dataset.clear();reads.clear();failRead=false;current=true;
    for(unsigned i=0;i<count;++i)dataset.push_back({L"file "+std::to_wstring(100000+i),L"C:\\Files"});
}

static bool legacyOnly=false;
static DWORD observedOffset=0,observedLimit=0,observedMessage=0;
static DWORD observedSort=0,windowSort=20,historyCalls=0;
static std::wstring openedPath;
LRESULT CALLBACK SortProvider(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_USER && w==510)return windowSort;
    return DefWindowProcW(hwnd,message,w,l);
}
LRESULT CALLBACK Provider(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    if(message!=WM_COPYDATA)return DefWindowProcW(hwnd,message,w,l);
    const auto& cds=*reinterpret_cast<const COPYDATASTRUCT*>(l);
    if(cds.dwData==24) {
        ++historyCalls;
        openedPath.assign(static_cast<const wchar_t*>(cds.lpData),cds.cbData/sizeof(wchar_t)-1);
        return 1;
    }
    if(cds.dwData==everything::kCopyDataQuery2W) {
        const auto& q=*static_cast<const everything::Query2HeaderW*>(cds.lpData);
        observedOffset=q.offset;observedLimit=q.max_results;observedMessage=2;observedSort=q.sort_type;
        if(legacyOnly)return FALSE;
        everything::List2HeaderW reply{1234,0,q.offset,q.request_flags,q.sort_type};
        COPYDATASTRUCT response{q.reply_copydata_message,sizeof(reply),&reply};
        SendMessageW(reinterpret_cast<HWND>(ULONG_PTR(q.reply_hwnd)),WM_COPYDATA,0,reinterpret_cast<LPARAM>(&response));
        return TRUE;
    }
    if(cds.dwData==everything::kCopyDataQueryW) {
        const auto& q=*static_cast<const everything::QueryHeaderW*>(cds.lpData);
        observedOffset=q.offset;observedLimit=q.max_results;observedMessage=1;
        everything::ListHeaderW reply{};reply.totitems=2345;reply.offset=q.offset;
        COPYDATASTRUCT response{q.reply_copydata_message,sizeof(reply),&reply};
        SendMessageW(reinterpret_cast<HWND>(ULONG_PTR(q.reply_hwnd)),WM_COPYDATA,0,reinterpret_cast<LPARAM>(&response));
        return TRUE;
    }
    return FALSE;
}
int main() {
    Check(InitialResultCount(480,48)==12 && InitialResultCount(720,48)==17,
          "initial batches fill independent viewport heights plus a small buffer");
    Check(InitialResultCount(96,48)==4 && InitialResultCount(2400,48)==52,
          "short and tall windows have bounded proportional first batches");
    Check(InitialResultCount(480,80)<InitialResultCount(480,48),
          "larger text requires fewer initial rows in the same viewport");
    for(double invalid:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        Check(InitialResultCount(invalid,48)==20 && InitialResultCount(480,invalid)==20,
              "unmeasured or invalid layout uses a small safe startup batch");
    Check(InitialResultCount(1e300,1e-300)==256,"extreme dimensions cannot request an unbounded initial batch");
    const wchar_t* sortNames[]={L"Name",L"Path",L"Size",L"Extension",L"Type",L"Date Created",
        L"Date Modified",L"Attributes",L"File List Filename",L"Run Count",L"Date Recently Changed",L"Date Accessed",L"Date Run"};
    for(DWORD i=0;i<ARRAYSIZE(sortNames);++i) {
        Check(everything::preferences::ParseSort(sortNames[i],true)==i*2+1,"saved sort property maps to native ascending order");
        Check(everything::preferences::ParseSort(sortNames[i],false)==i*2+2,"saved sort property maps to native descending order");
    }
    Check(everything::preferences::ParseSort(L"RUN-count",false)==20 &&
          everything::preferences::ParseSort(L"9",false)==20,"named and legacy run-count settings are recognized");
    Check(!everything::preferences::ParseSort(L"invalid",true) &&
          !everything::preferences::ParseSort(L"9suffix",true) &&
          !everything::preferences::ParseSort(L"99999999",true),"unknown or malformed saved properties use the safe fallback");
    wchar_t temp[MAX_PATH]{},ini[MAX_PATH]{};
    Check(GetTempPathW(MAX_PATH,temp)&&GetTempFileNameW(temp,L"srt",0,ini),"private sort-settings fixture is created");
    WritePrivateProfileStringW(L"Everything",L"sort",L"Run Count",ini);
    WritePrivateProfileStringW(L"Everything",L"sort_ascending",L"0",ini);
    Check(everything::preferences::ReadIniSort(ini)==20,"actual INI reader honors Everything's run-count preference");
    WritePrivateProfileStringW(L"Everything",L"sort",L"Date Modified",ini);
    WritePrivateProfileStringW(L"Everything",L"sort_ascending",L"1",ini);
    Check(everything::preferences::ReadIniSort(ini)==13,"saved sort changes are picked up without rewriting the user's file");
    WritePrivateProfileStringW(L"Everything",L"sort_ascending",L"bad",ini);
    Check(!everything::preferences::ReadIniSort(ini),"malformed sort direction is rejected");DeleteFileW(ini);

    std::vector<everything::Result> ordered{{L"z-file",L"C:\\Files"},{L"file",L"C:\\Files"},{L"a-file",L"C:\\Files"}};
    ordered[0].runCount=1;ordered[1].runCount=99999;
    Check(filefilter::Filter(&ordered,L"file",3)==3 && ordered[0].name==L"z-file" &&
          ordered[1].name==L"file" && ordered[2].name==L"a-file","file names and run counts never override Everything's supplied order");
    ordered.insert(ordered.begin()+1,{L"excluded",L"C:\\.gradle\\cache"});
    const std::vector<std::wstring> exclusions{L"\\.gradle\\"};
    Check(filefilter::Filter(&ordered,L"file",4,&exclusions)==3 && ordered[0].name==L"z-file" &&
          ordered[1].name==L"file","path filtering retains the relative order of remaining files");
    filepages::Session pages;
    Dataset(1050);pages.Reset(L"file",false,{});
    std::vector<everything::Result> all,added;
    Check(pages.Take(12,&added,Fetch,[]{return current;})&&added.size()==12,"first file batch stays small");
    Check(reads.size()==1&&reads[0].offset==0&&reads[0].limit==200,"only one bounded provider page is fetched initially");
    all=added;
    while(pages.More()) {
        added.clear();Check(pages.Take(kScrollBatchSize,&added,Fetch,[]{return current;}),"later provider pages succeed");
        Check(added.size()<=kScrollBatchSize,"scrolling exposes at most forty new rows");
        all.insert(all.end(),added.begin(),added.end());
    }
    Check(all.size()==1050&&reads.size()==6,"every file beyond fifty and the first provider page remains browsable");
    for(size_t i=0;i<all.size();++i)Check(all[i].name==dataset[i].name,"earlier ranked results never move when pages append");
    for(size_t i=0;i<reads.size();++i)Check(reads[i].offset==i*200&&reads[i].limit==200,"IPC offsets advance without fetching earlier pages again");
    Check(pages.Total()==1050,"full provider count is preserved");
    pages.Reset(L"",false,{});added.clear();
    Check(pages.Take(12,&added,Fetch,[]{return current;})&&added.size()==12 &&
          added[0].name==dataset[0].name,"empty searches expose the first native page in the same provider order");

    Dataset(227);
    for(unsigned i=0;i<46;++i)dataset[i].path=L"C:\\Projects\\node_modules\\cache";
    pages.Reset(L"file",true,{L"\\node_modules\\"});all.clear();
    added.clear();
    Check(pages.Take(20,&added,Fetch,[]{return current;}) && pages.Total()==181 && pages.More(),
          "the counter subtracts known excluded paths before reaching the last page");
    all=added;
    while(pages.More()) {
        added.clear();
        Check(pages.Take(20,&added,Fetch,[]{return current;}),"filtered counter query keeps loading its remaining pages");
        all.insert(all.end(),added.begin(),added.end());
    }
    Check(all.size()==181 && pages.Total()==181,"227 raw matches minus 46 excluded paths ends at 181 shown out of 181");

    Dataset(450);dataset[200]=dataset[199];pages.Reset(L"file",false,{});all.clear();
    while(pages.More()){added.clear();Check(pages.Take(20,&added,Fetch,[]{return current;}),"duplicate-safe continuation succeeds");all.insert(all.end(),added.begin(),added.end());}
    std::set<std::wstring> unique;for(const auto& file:all)unique.insert(file.path+file.name);
    Check(all.size()==449&&unique.size()==all.size()&&pages.Total()==449,
          "duplicate rows cannot remain in the final result counter");
    Dataset(10);pages.Reset(L"file",false,{});failRead=true;added.clear();
    Check(!pages.Take(20,&added,Fetch,[]{return current;})&&added.empty(),"failed next-page fetch publishes no partial batch");
    failRead=false;added.clear();Check(pages.Take(20,&added,Fetch,[]{return current;})&&added.size()==10,"retry starts at the same unconsumed provider offset");
    Check(reads[0].offset==0&&reads[1].offset==0,"failed fetch never advances its cursor");
    Dataset(10);pages.Reset(L"file",false,{});added.clear();
    Check(!pages.Take(20,&added,[&](auto&&... args){const bool ok=Fetch(args...);current=false;return ok;},[]{return current;})&&added.empty(),"typing during IPC discards the completed old page");
    current=true;pages.Reset(L"different",false,{});added.clear();
    Check(pages.Take(20,&added,Fetch,[]{return current;})&&added.size()==10,"a new query resets deduplication and pending-page state");
    Check(pages.Total()==10,"new queries reset the excluded/duplicate count too");

    Dataset(1000);for(auto& item:dataset)item.name=L"file duplicate";
    pages.Reset(L"file",false,{});added.clear();
    Check(pages.Take(20,&added,Fetch,[]{return current;})&&reads.size()==4&&added.size()==1&&pages.More(),"one request has a bounded scan even when pages repeat");
    Dataset(0);pages.Reset(L"file",false,{});added.clear();
    Check(pages.Take(20,&added,Fetch,[]{return current;})&&added.empty()&&!pages.More(),"an empty provider reaches its end without a request loop");
    Dataset(500);pages.Reset(L"file",false,{});added.clear();
    Check(pages.Take(300,&added,[&](const std::wstring& query,DWORD limit,DWORD offset,
          std::vector<everything::Result>* out,DWORD* total){
              if(offset){*total=500;return true;}
              return Fetch(query,limit,offset,out,total);
          },[]{return current;}) && added.size()==200 && !pages.More() && pages.Total()==200,
          "an empty later page finishes at the actual browsable count despite a stale provider total");

    reads.clear();pages.Reset(L"file",true,{L"\\.gradle\\"});all.clear();
    const auto filtered=[&](const std::wstring& query,DWORD limit,DWORD offset,std::vector<everything::Result>* out,DWORD* total) {
        reads.push_back({query,limit,offset});
        const bool clean=query.find(L"!path:")!=query.npos;*total=clean?300:1000;
        for(DWORD i=offset;i<std::min(*total,offset+limit);++i)
            out->push_back({L"file "+std::to_wstring(100000+i),clean?L"C:\\Projects":L"C:\\.gradle\\cache"});
        return true;
    };
    while(pages.More()){added.clear();Check(pages.Take(20,&added,filtered,[]{return current;}),"excluded-path fallback paginates");all.insert(all.end(),added.begin(),added.end());}
    Check(all.size()==300&&reads.size()==3&&reads[1].query.find(L"!path:")!=std::wstring::npos&&
          reads[2].query==reads[1].query&&reads[2].offset==200,"continuation keeps the selected filtered query and its exact cursor");

    g_appsBatch.Begin(7,6);g_appsBatch.Complete(6,true);
    g_filesBatch.Begin(7,12);g_filesBatch.Complete(12,true);
    g_fileTemplate=FileSearchRequest{7,L"files",true,12,false,{}};
    Check(RequestMoreApps(L"files",6,7)&&g_appsBatch.requested==46&&g_queryRevision==7&&g_queryDirty,"app scroll adds forty rows without changing the typed query");
    Check(!RequestMoreApps(L"files",6,7)&&!g_fileQuery,"duplicate app notifications do not query files");
    g_queryDirty=false;
    Check(RequestMoreFiles(L"files",12,7)&&g_fileQuery->limit==52&&!g_queryDirty&&g_queryRevision==7,"file scrolling adds forty rows without recomputing apps");
    Check(!RequestMoreFiles(L"files",12,7),"duplicate in-flight file requests are suppressed");
    g_appsBatch.Complete(46,true);g_filesBatch.Complete(52,false);
    Check(!RequestMoreApps(L"files",6,7)&&RequestMoreApps(L"files",46,7),"an unrendered app batch cannot be requested again");
    Check(!RequestMoreFiles(L"files",52,7),"file paging stops at its actual end");
    g_appsBatch.Complete(86,true);
    Check(!RequestMoreApps(L"old",86,7)&&!RequestMoreApps(L"files",86,6),"old query text and revision cannot request another batch");
    g_appsBatch.requested=std::numeric_limits<size_t>::max();
    Check(!RequestMoreApps(L"files",86,7),"batch growth cannot overflow");

    WNDCLASSW wc{};wc.lpfnWndProc=Provider;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=everything::kIpcWindowClass;
    Check(RegisterClassW(&wc)!=0,"private IPC provider class registers");
    HWND provider=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    Check(provider!=nullptr,"private IPC provider is created without a visible window");
    WNDCLASSW sortClass{};sortClass.lpfnWndProc=SortProvider;sortClass.hInstance=wc.hInstance;sortClass.lpszClassName=L"EVERYTHING";
    Check(RegisterClassW(&sortClass)!=0,"private sort-window class registers");
    HWND sortWindow=CreateWindowExW(0,sortClass.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    Check(sortWindow && everything::preferences::PreferredSort(provider)==20,"public GET_SORT reads the matching process's current sort");
    windowSort=14;
    Check(everything::preferences::PreferredSort(provider)==14,"changing the native window's order is observed on the next query");
    windowSort=99;
    Check(everything::preferences::PreferredSort(provider)==1,"unsupported native sort values fall back safely");
    DestroyWindow(sortWindow);UnregisterClassW(sortClass.lpszClassName,wc.hInstance);
    const auto namedClass=std::wstring(everything::kIpcWindowClass)+L"_(1.5a)";
    WNDCLASSW namedProvider=wc;namedProvider.lpszClassName=namedClass.c_str();
    Check(RegisterClassW(&namedProvider)!=0,"named private Everything instance registers");
    HWND named=CreateWindowExW(0,namedProvider.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    Check(everything::preferences::InstanceSuffix(named)==L"-1.5a","named instances select their own settings filename");
    sortClass.lpszClassName=L"EVERYTHING_(1.5a)";windowSort=6;
    Check(RegisterClassW(&sortClass)!=0,"named sort-window class registers");
    sortWindow=CreateWindowExW(0,sortClass.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
    Check(everything::preferences::PreferredSort(named)==6,"named instance reads its own native sort");
    Check(everything::preferences::PreferredSort(provider)==1,"another instance's window cannot supply the default instance's sort");
    DestroyWindow(sortWindow);DestroyWindow(named);
    UnregisterClassW(sortClass.lpszClassName,wc.hInstance);UnregisterClassW(namedProvider.lpszClassName,wc.hInstance);
    {
        everything::Client client;Check(client.Init(),"production IPC client initializes");
        std::vector<everything::Result> reply;DWORD total=0;
        Check(client.QueryPage(L"files",20,800,&reply,&total)&&observedOffset==800&&observedLimit==20&&total==1234&&observedMessage==2,"QUERY2 transmits the actual continuation offset and limit");
        Check(client.QueryPage(L"",12,0,&reply,&total,20)&&observedSort==20&&observedLimit==12,
              "empty-query IPC requests Everything's selected sort and only the initial batch");
        Check(client.QueryPage(L"files",20,200,&reply,&total,14)&&observedSort==14&&observedOffset==200,
              "later pages carry the frozen query's native sort");
        everything::RecordFileOpen(L"C:\\Files\\unknown-setting.txt");
        Check(historyCalls==0,"unavailable history preferences cannot enable local history implicitly");
        const auto settings=everything::preferences::SettingsFile(provider);
        Check(!settings.empty() && GetFileAttributesW(settings.c_str())==INVALID_FILE_ATTRIBUTES,
              "history preference test is confined to a new private INI beside the test executable");
        WritePrivateProfileStringW(L"Everything",L"run_history_enabled",L"1",settings.c_str());
        everything::RecordFileOpen(L"C:\\Files\\opened.txt");
        Check(historyCalls==1 && openedPath==L"C:\\Files\\opened.txt","file openings forward the official history message to the selected instance");
        everything::RecordFileOpen(L"https://example.com");everything::RecordFileOpen(L"ms-settings:display");
        Check(historyCalls==1,"web and settings launches are never written into Everything run history");
        WritePrivateProfileStringW(L"Everything",L"run_history_enabled",L"0",settings.c_str());
        everything::RecordFileOpen(L"C:\\Files\\disabled.txt");
        Check(historyCalls==1,"disabled Everything run history prevents history writes");
        WritePrivateProfileStringW(L"Everything",L"run_history_enabled",L"1",settings.c_str());
        everything::RecordFileOpen(L"C:\\Files\\enabled.txt");
        Check(historyCalls==2 && openedPath==L"C:\\Files\\enabled.txt","re-enabled Everything history resumes local run counting");
        DeleteFileW(settings.c_str());
        legacyOnly=true;reply.clear();
        Check(client.Query(L"files",17,&reply,&total,5,everything::kDefaultRequestFlags,everything::kSortNameAscending,812)&&
              observedOffset==812&&observedLimit==17&&total==2345&&observedMessage==1,"legacy fallback retains the same continuation offset and limit");
    }
    DestroyWindow(provider);UnregisterClassW(wc.lpszClassName,wc.hInstance);
    printf("PASS: %u progressive-result and private IPC checks\n",checks);
}
