// The real search workers and publication code run against deliberately slow
// providers. No Explorer, Everything, browser or user files are touched.
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cwctype>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>
using namespace std::chrono_literals;
// PRODUCTION_COMMANDS
struct CustomCommand { std::wstring name, command; };
enum class CustomCommandShell { Cmd };
std::mutex g_resultsMutex, g_queryMutex, g_settingsMutex;
std::condition_variable g_queryWake, g_fileQueryWake;
std::wstring g_pendingQuery;
uint64_t g_queryRevision = 0;
std::atomic<bool> g_isOverlayVisible{true}, g_quit{false}, g_searchQuit{false}, g_queryDirty{false}, g_forgetUsage{false};
std::atomic<uint64_t> g_settingsRevision{1};
std::atomic<DWORD> g_totalMatches{0};
struct {
    bool learnFavorites = true, filterNoisyPaths = true;
    int searchDebounceMs = 0, maxFileResults = 12, maxAppResults = 6;
    std::vector<std::wstring> excludedPaths;
    std::vector<CustomCommand> customCommands;
    bool onlineCards = true;
    std::wstring weatherLocation = L"auto", weatherUnits = L"auto", quoteCurrency = L"USD";
    commands::Names commandNames;
    std::vector<std::wstring> weatherLocations, currencyList, cryptoList;
    int weatherPastDays = 2, weatherFutureDays = 5;
} g_settings;
int IconPixels() { return 24; }
template<class... T> void Wh_Log(T&&...) {}
// PRODUCTION_MODEL

struct Seen { uint64_t revision; std::vector<Row> rows; };
std::mutex observedMutex;
std::condition_variable observedWake;
std::vector<Seen> seenApps, seenFiles;
std::wstring launched;
std::atomic<int> iconFetches{0};
std::mutex providerMutex;
std::condition_variable providerWake;
bool fileEntered = false, releaseFiles = false;
bool blockIcons=true,iconEntered=false,blockRebuild=false,rebuildEntered=false;
std::atomic<bool> includeGamma{false};
std::vector<std::wstring> fileQueries;
void RequestRender() {
    std::scoped_lock lock(observedMutex, g_resultsMutex);
    seenApps.push_back({g_appsRevision, g_appRows});
    observedWake.notify_all();
}
void RequestFileRender() {
    std::scoped_lock lock(observedMutex, g_resultsMutex);
    seenFiles.push_back({g_filesRevision, g_fileRows});
    observedWake.notify_all();
}
void PublishLateAppIcon(const std::wstring&, const std::vector<BYTE>&) {}
void PublishLateFileIcons(std::vector<std::pair<std::wstring, std::vector<BYTE>>>, uint64_t) {}

namespace tools {
std::wstring Trim(const std::wstring& text) { return text; }
std::wstring ToLower(std::wstring text) { for (auto& c : text) c = towlower(c); return text; }
struct Interface { std::wstring ip, typeLabel, adapterName, mask, gateway, glyph; };
std::vector<Interface> GetNetworkInterfaces() { return {}; }
}
namespace apps {
using UsageCounts = std::map<std::wstring, int>;
std::atomic<int> rebuilds{0};
struct PidlFree { void operator()(ITEMIDLIST* value) const { ILFree(value); } };
struct App {
    std::wstring name, nameLower, targetPath, settingArguments, linkTarget, area;
    bool isSetting = false;
    bool installedOnly = false,inventoryOnly=false;
    bool targetExists=false,linkExists=false;
    std::wstring installedId,installedName,installedPublisher,installedVersion,installedPackage;
    std::wstring installedLocation,installedFamily,installedKey,installedArchitecture;HKEY installedHive=nullptr;REGSAM installedView=0;
    std::unique_ptr<ITEMIDLIST, PidlFree> pidl;
};
const std::wstring& IconIdentity(const App& app){return app.installedOnly?app.installedId:app.name;}
struct Match { const App* app; int score = 0, rank = 0; };
enum class SearchScope { Normal, Installed };
class Index {
    std::vector<App> apps;
public:
    bool Rebuild(const std::atomic<bool>* = nullptr) {
        const auto build=++rebuilds;
        if(build>1){std::unique_lock lock(providerMutex);rebuildEntered=true;providerWake.notify_all();providerWake.wait(lock,[]{return !blockRebuild;});}
        apps.clear();
        for (const auto* name : {L"Alpha", L"Beta"}) {
            App app; app.name = name; app.nameLower = tools::ToLower(name);
            app.targetPath = app.name + L".exe";
            apps.push_back(std::move(app));
        }
        App suite;suite.name=L"Suite en-us";suite.nameLower=L"suite en-us";suite.installedOnly=true;
        suite.targetPath=L"ms-settings:appsfeatures";suite.installedId=L"product:test";suite.installedPublisher=L"Publisher";suite.installedVersion=L"1.0";suite.installedLocation=L"C:\\Program Files\\Suite";suite.installedKey=L"suite-record";suite.installedHive=HKEY_CURRENT_USER;suite.installedView=KEY_WOW64_64KEY;
        apps.push_back(std::move(suite));
        for(int i=0;i<70;++i){
            App app;app.name=L"Package "+std::to_wstring(100+i);app.nameLower=tools::ToLower(app.name);
            app.installedName=app.name;app.installedId=app.name;app.installedOnly=true;app.inventoryOnly=true;
            app.targetPath=L"ms-settings:appsfeatures";app.installedPackage=app.name+L"_1.0.0.0_x64__1234567890123";
            apps.push_back(std::move(app));
        }
        if(includeGamma){App added;added.name=L"Gamma";added.nameLower=L"gamma";added.targetPath=L"Gamma.exe";apps.push_back(std::move(added));}
        return true;
    }
    std::shared_ptr<Index> Refreshed(bool, const std::atomic<bool>* = nullptr)const{auto next=std::make_shared<Index>();return next->Rebuild()?next:nullptr;}
    size_t Count() const { return apps.size(); }
    const App* At(size_t i) const { return i < apps.size() ? &apps[i] : nullptr; }
    std::vector<Match> Suggest(size_t, const UsageCounts*) { return {{&apps[0]}, {&apps[1]}}; }
    std::vector<Match> Search(const std::wstring& query, size_t, const UsageCounts*,SearchScope scope=SearchScope::Normal) {
        std::vector<Match> matches;
        for (const auto& app : apps) if ((scope==SearchScope::Installed?!app.installedId.empty():!app.installedOnly) &&
            app.nameLower.find(tools::ToLower(query)) != std::wstring::npos) matches.push_back({&app});
        return matches;
    }
};
// PRODUCTION_WARMUP
UsageCounts LoadUsage() { return {}; }
void SaveUsage(const UsageCounts&) {}
void RecordUsage(UsageCounts*, const std::wstring&) {}
}
std::optional<apps::WarmIndex> g_appsWarmup;
std::optional<std::thread> g_searchThread;
bool FetchAppIcon(const apps::App*, int, std::vector<BYTE>* pixels) {
    ++iconFetches;
    {std::unique_lock lock(providerMutex);iconEntered=true;providerWake.notify_all();providerWake.wait(lock,[]{return !blockIcons;});}
    std::this_thread::sleep_for(8ms);
    pixels->assign(24 * 24 * 4, 255);
    return true;
}
std::vector<BYTE> FetchCustomCommandIcon(const std::wstring&, int) { return {}; }
std::wstring CustomCommandIconPath(const std::wstring& text) { return text; }
void AddCustomCommandRows(std::vector<Row>*, const std::vector<CustomCommand>&, const std::wstring&, size_t,
    uint64_t, int, std::map<std::wstring, std::vector<BYTE>>*, const apps::UsageCounts*) {}
void SpawnTrackedLaunch(std::function<void()> launch) { launch(); }
void LaunchAppAsync(std::wstring name, std::wstring, ITEMIDLIST*, bool, std::wstring) {
    std::lock_guard lock(observedMutex);
    launched = std::move(name);
    observedWake.notify_all();
}
namespace calc {
std::vector<Row> Rows(const std::wstring& query) {
    if (query != L"1+1") return {};
    Row row; row.title = L"2"; row.copyText = L"2"; return {row};
}
}
namespace online {
bool g_speedBusy=false;
uint64_t g_speedRevision=0;
enum class Kind { None };
struct Request { Kind kind = Kind::None; uint64_t revision = 0; };
using Batch = std::vector<Request>;
struct Initial { std::vector<Row> rows; bool fetch = false; };
Request Parse(const std::wstring&, const std::wstring&, const std::wstring&, const std::wstring&, bool,
    const commands::Names&, int, int) { return {}; }
Batch Expand(Request request, const commands::Names&, const std::vector<std::wstring>&,
    const std::vector<std::wstring>&, const std::vector<std::wstring>&) { return {request}; }
Initial Begin(const Request&) { return {}; }
Initial BeginBatch(const Batch&) { return {}; }
void Schedule(Batch) {}
void Wake() {}
Row ConflictRow(){Row row;row.infoOnly=row.isError=true;return row;}
}
namespace hardware {
std::atomic<int> requests{0};
std::vector<Row> Initial(const std::wstring&,bool){Row row;row.title=L"hardware";return {row};}
void Schedule(std::wstring,uint64_t){++requests;}
void Wake(){}
}
namespace services {
std::atomic<int> requests{0};
std::vector<Row> Initial(const std::wstring&,size_t){Row row;row.title=L"services";return {row};}
void Schedule(std::wstring,std::wstring,uint64_t,size_t){++requests;}
void Wake(){}
}
namespace i18n {
enum class Key { NoNetwork, CheckNetwork, Subnet, Gateway, CopyHint, OpenService, SearchWeb, SearchService, InstalledApps, NoApps, Next, Shown };
const wchar_t* Text(Key) { return L"test"; }
std::wstring Format(Key, std::initializer_list<std::wstring>) { return L"test"; }
}
struct ResolvedWebQuery { std::wstring queryTerm, serviceName, homeUrl, searchUrl; bool isShortcut = false; };
ResolvedWebQuery ResolveWebSearch(std::wstring text) { ResolvedWebQuery r; r.queryTerm = std::move(text); return r; }
namespace everything {
struct Result { std::wstring name, path; bool isFolder = false; };
class Client {
public:
    bool Init() { return true; }
    bool Query(const std::wstring& query, DWORD, std::vector<Result>* rows, DWORD* total) {
        std::unique_lock lock(providerMutex);
        fileQueries.push_back(query);
        fileEntered = true;
        providerWake.notify_all();
        providerWake.wait(lock, [] { return releaseFiles; });
        rows->push_back({query, L"test", false}); *total = 1;
        return true;
    }
};
}
namespace ranker {
constexpr DWORD kDefaultPool = 100;
size_t Rank(std::vector<everything::Result>* rows, const std::wstring&, size_t, const std::vector<std::wstring>*) { return rows->size(); }
std::wstring WithExclusions(const std::wstring& q, const std::vector<std::wstring>*) { return q; }
}
namespace icons {
class FileIconCache {
public:
    explicit FileIconCache(int) {}
    bool HasPending() const { return false; }
    bool FetchPending() { return false; }
    void SetSize(int) {}
    void NewResults(std::chrono::milliseconds) {}
    std::vector<std::pair<std::wstring, std::vector<BYTE>>> TakeLate() { return {}; }
    const std::vector<BYTE>* Get(const std::wstring&, bool) { return nullptr; }
};
}
std::wstring g_shownQuery;
uint64_t g_shownAppsRevision = 0, g_shownFilesRevision = 0;
uint64_t g_shownAppsContentRevision = 0;
DWORD g_shownFilesTotal = 0;
std::vector<Row> g_currentAppRows;
std::vector<Row> g_currentFileRows;
bool g_filesColumnActive = false;
int g_selectedApp = 0;
std::atomic<bool> filesVisible{true};
bool FilesPanelVisible() { return filesVisible.load(); }
// PRODUCTION_PENDING
// PRODUCTION_RENDER_SNAPSHOT
// PRODUCTION_QUEUE
// PRODUCTION_WORKERS

int checks = 0;
void Check(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", label); std::exit(1); }
    ++checks;
}
bool Apps(uint64_t revision) {
    std::unique_lock lock(observedMutex);
    return observedWake.wait_for(lock, 2s, [&] { return !seenApps.empty() && seenApps.back().revision >= revision; });
}
bool Files(uint64_t revision) {
    std::unique_lock lock(observedMutex);
    return observedWake.wait_for(lock, 2s, [&] { return !seenFiles.empty() && seenFiles.back().revision >= revision; });
}
uint64_t Queue(const wchar_t* query) {
    QueueQuery(query);
    std::lock_guard lock(g_queryMutex);
    return g_queryRevision;
}
int main() {
    Row alphaSelection;alphaSelection.title=L"Alpha";alphaSelection.openPath=L"Alpha.exe";
    Row betaSelection;betaSelection.title=L"Beta";betaSelection.openPath=L"Beta.exe";
    Check(RefreshedAppSelection(L"a",L"a",{alphaSelection,betaSelection},{betaSelection,alphaSelection},1,false)==0,"silent catalog updates preserve the selected app by action identity after reorder");
    Check(RefreshedAppSelection(L"a",L"b",{alphaSelection,betaSelection},{betaSelection},1,false)==-1,"new text resets selection normally");
    Check(RefreshedAppSelection(L"a",L"a",{alphaSelection},{alphaSelection},0,true)==-1,"forced UI rebuilds retain their explicit reset behavior");
    Row otherSelection=alphaSelection;otherSelection.openPath=L"Different.exe";
    Check(RefreshedAppSelection(L"a",L"a",{alphaSelection},{otherSelection},0,false)==-1,"a same-name app at another target cannot inherit selection");
    Row installedSelection=alphaSelection;installedSelection.installedOnly=true;installedSelection.installedKey=L"precise-product";installedSelection.installedHive=HKEY_CURRENT_USER;
    Row updatedSelection=installedSelection;updatedSelection.title=L"Alpha 2.0";updatedSelection.cardSignature=L"new-version";
    Check(RefreshedAppSelection(L"a",L"a",{installedSelection},{updatedSelection},0,false)==0,"installed products retain selection across a version/title change at the exact same registration");
    Row richSelection;richSelection.sectionTitle=L"Weather";
    Check(RefreshedAppSelection(L"/weather",L"/weather",{richSelection,richSelection},{richSelection},1,false)==0,"live utility rows retain their bounded selection");
    StartAppsWarmup();
    Check(g_appsWarmup->Start(),"catalog warmup starts independently of any search query");
    const auto warmDeadline=GetTickCount64()+2000;
    while(!g_appsWarmup->BuiltAt() && GetTickCount64()<warmDeadline)Sleep(1);
    Check(g_appsWarmup->BuiltAt()!=0 && apps::rebuilds==1,"catalog is ready in the background before search starts");
    Row installed;installed.installedOnly=true;installed.openPath=L"ms-settings:appsfeatures";
    Check(!IsSettingsResult(installed),"installed-only management destination must never choose a Settings gear icon");
    Row setting;setting.isSetting=true;setting.openPath=L"ms-settings:display";
    Check(IsSettingsResult(setting),"actual Windows Settings result retains its gear icon");
    Row app;app.openPath=L"program.exe";
    Check(!IsSettingsResult(app),"ordinary application keeps an application icon");
    const commands::Names names;
    std::vector<CustomCommand> custom{{L"/weather", L"echo hello"}};
    Check(commands::Parse(L"/WEATHER London", names, custom).kind==commands::Kind::Conflict,"custom slash name conflicts with a tool case-insensitively, including arguments");
    custom={{L"/fx",L"echo hello"}};
    Check(commands::Parse(L"/fx", names, custom).kind==commands::Kind::Conflict,"compatibility aliases also participate in custom slash conflicts");
    custom={{L"weather",L"echo hello"},{L"/backup",L"echo backup"}};
    Check(commands::Parse(L"/weather", names, custom).kind==commands::Kind::Weather,"ordinary custom names do not reserve slash tool names");
    Check(commands::Parse(L"/backup", names, custom).kind==commands::Kind::None,"unique custom slash names remain normal searchable custom commands");
    custom.push_back({L" /BACKUP ",L"echo other"});
    Check(commands::Parse(L"/backup", names, custom).kind==commands::Kind::Conflict,"duplicate custom slash names show a conflict even without a built-in tool");
    custom={{L"/weather",L" "},{L"/weather today",L"echo today"}};
    Check(commands::Parse(L"/weather", names, custom).kind==commands::Kind::Weather,"disabled commands and multiword display names do not reserve a tool token");
    // Exercise the actual UI snapshot decision before starting any workers.
    // Pending files must never turn an apps update into an empty files pass.
    const auto old = Queue(L"Alpha");
    Row oldFile; oldFile.title = L"old file"; oldFile.openPath = L"old.txt";
    Check(PublishAppResults(old, L"Alpha", {}) && PublishFileResults(old, {oldFile}, 45), "seed displayed file results");
    g_currentFileRows = {oldFile};
    g_shownAppsRevision = g_shownFilesRevision = old;
    g_shownAppsContentRevision = g_appsContentRevision;
    g_shownFilesTotal = 45;
    const auto next = Queue(L"Beta");
    Row newApp; newApp.title = L"Beta";
    Check(PublishAppResults(next, L"Beta", {newApp}), "new apps publish ahead of replacement files");
    const auto appPass = ReadSearchRenderSnapshot(true, false);
    Check(appPass && appPass->appsChanged && !appPass->filesChanged, "apps pass does not invalidate displayed files");
    Check(appPass->fileCount == 1 && appPass->total == 45, "pending file badge keeps its displayed count");
    Check(appPass->files.empty() && g_currentFileRows[0].openPath == L"old.txt", "apps pass neither copies file icons nor clears visible rows");
    const auto pending = ReadSearchRenderSnapshot(false, false);
    Check(pending && !pending->filesChanged && pending->filesRevision == old, "normal pass also retains old files while waiting");
    const auto restyle = ReadSearchRenderSnapshot(false, true);
    Check(restyle && restyle->filesChanged && restyle->files[0].openPath == L"old.txt", "theme refresh restyles retained files without blanking them");
    g_shownAppsRevision = next;
    g_shownAppsContentRevision = g_appsContentRevision;
    Check(!ReadSearchRenderSnapshot(false, false), "no intermediate empty file redraw after apps commit");
    g_currentAppRows = {newApp};
    Check(!ResultsPending(), "app activation stays ready while previous files remain visible");
    g_filesColumnActive = true;
    Check(ResultsPending(), "retained old files are still pending for keyboard activation");
    g_filesColumnActive = false;
    Row newFile; newFile.title = L"new file"; newFile.openPath = L"new.txt";
    Check(PublishFileResults(next, {newFile}, 82), "current file replacement publishes");
    const auto replacement = ReadSearchRenderSnapshot(false, false);
    Check(replacement && replacement->filesChanged && !replacement->appsChanged && replacement->files[0].openPath == L"new.txt",
          "ready files replace old rows in a single file-only pass");
    Check(replacement->total == 82 && replacement->filesRevision == next, "replacement count and revision belong to new files");
    Check(PublishFileResults(next, {}, 0), "genuine empty file results publish");
    const auto noFiles = ReadSearchRenderSnapshot(false, false);
    Check(noFiles && noFiles->filesChanged && noFiles->files.empty() && noFiles->fileCount == 0,
          "genuine no-match results can still clear previous files");
    g_currentFileRows.clear(); g_currentAppRows.clear();
    g_shownAppsRevision = g_shownFilesRevision = 0;
    g_shownFilesTotal = 0; g_totalMatches = 0;

    // Queue before starting so the first result has genuinely cold icons.
    const auto first = Queue(L"Alpha");
    std::thread files(FileSearchThreadMain), apps(SearchThreadMain);
    Check(Apps(first), "cold app results arrive");
    Check(apps::rebuilds==1,"first search takes the warmed catalog without rebuilding it");
    {
        std::lock_guard lock(observedMutex);
        Check(seenApps.back().rows.size() == 1 && seenApps.back().rows[0].title == L"Alpha", "app match is correct");
        Check(seenApps.back().rows[0].icon.empty(), "cold icons do not block publication");
    }
    {
        std::unique_lock lock(providerMutex);
        Check(providerWake.wait_for(lock, 2s, [] { return fileEntered; }), "file provider is deliberately blocked");
    }
    {std::unique_lock lock(providerMutex);Check(providerWake.wait_for(lock,2s,[]{return iconEntered;}),"an icon extension is deliberately blocked");}
    const auto inputStarted=GetTickCount64();
    const auto second = Queue(L"Beta");
    {std::unique_lock lock(observedMutex);Check(observedWake.wait_for(lock,100ms,[&]{return seenApps.back().revision>=second;}),"a first letter publishes within 100 ms even while an icon provider and files are blocked");}
    Check(GetTickCount64()-inputStarted<100,"matching does not wait for icon extraction");
    {std::lock_guard lock(providerMutex);blockIcons=false;}providerWake.notify_all();
    Check(Apps(second), "another app query completes while files are blocked");
    const auto third = Queue(L"Alpha");
    Check(Apps(third), "A -> B -> A completes while old files are blocked");
    Check(!PublishAppResults(first, L"Alpha", {}), "old app revision cannot replace newer identical text");
    Check(!PublishFileResults(first, {}, 999), "old files cannot replace newer identical text");
    {
        std::scoped_lock lock(observedMutex, g_resultsMutex);
        Check(seenFiles.empty() && g_totalMatches.load() == 0, "no old file results published");
        g_currentAppRows = g_appRows;
        g_shownAppsRevision = g_appsRevision;
    }
    Check(!ResultsPending(), "Enter on apps does not wait for files");
    g_filesColumnActive = true;
    Check(ResultsPending(), "Enter on files still waits for that panel");
    g_filesColumnActive = false;

    Check(Apps(Queue(L"Beta")), "newer app results replace previous match list");
    {
        std::lock_guard lock(g_queryMutex);
        g_launchRequest = AppLaunchRequest{L"Alpha", L"Alpha.exe", false};
    }
    g_queryWake.notify_all();
    {
        std::unique_lock lock(observedMutex);
        Check(observedWake.wait_for(lock, 2s, [] { return !launched.empty(); }), "clicked app launches");
        Check(launched == L"Alpha", "launch identity survives a replacement match list");
    }
    {std::lock_guard lock(providerMutex);blockRebuild=true;rebuildEntered=false;}
    g_appsWarmup->Refresh(true);
    {std::unique_lock lock(providerMutex);Check(providerWake.wait_for(lock,2s,[]{return rebuildEntered;}),"catalog refresh is deliberately blocked");}
    const auto whileRefreshing=Queue(L"Gamma");
    {std::unique_lock lock(observedMutex);Check(observedWake.wait_for(lock,100ms,[&]{return seenApps.back().revision>=whileRefreshing;}),"typing stays responsive while the catalog is rebuilding");Check(seenApps.back().rows.empty(),"the old snapshot stays usable until a complete replacement is ready");}
    includeGamma=true;
    {std::lock_guard lock(providerMutex);blockRebuild=false;}providerWake.notify_all();
    {std::unique_lock lock(observedMutex);Check(observedWake.wait_for(lock,2s,[]{return !seenApps.back().rows.empty() && seenApps.back().rows[0].title==L"Gamma";}),"a new app appears without reopening or typing another character after background refresh");}
    const auto latest = Queue(L"1+1");
    Check(Apps(latest), "calculator is independent of file provider too");
    {
        std::lock_guard lock(providerMutex); releaseFiles = true;
    }
    providerWake.notify_all();
    {
        std::unique_lock lock(observedMutex);
        Check(observedWake.wait_for(lock, 2s, [&] { return !seenFiles.empty() && seenFiles.back().revision == latest; }), "files catch up to the latest request");
        Check(seenFiles.size() == 1 && seenFiles[0].rows[0].title == L"1+1", "stale in-flight and queued files are discarded");
        Check(seenApps.back().rows[0].copyText == L"2", "file completion leaves app results intact");
    }
    {
        std::lock_guard lock(providerMutex);
        Check(fileQueries.size() == 2, "intermediate file requests were coalesced");
    }
    const auto empty = Queue(L"");
    Check(Apps(empty) && Files(empty), "empty query updates both panels");
    {
        std::lock_guard lock(observedMutex);
        Check(seenApps.back().rows.size() == 2 && seenFiles.back().rows.empty(), "empty query has app suggestions without file results");
    }
    const auto web = Queue(L"?cats");
    Check(Apps(web) && Files(web), "explicit web mode updates both panels");
    {
        std::lock_guard lock(providerMutex);
        Check(fileQueries.back() == L"cats", "explicit web mode searches files using only its search term");
    }
    const auto ip = Queue(L"/ip");
    Check(Apps(ip) && Files(ip), "IP command does not wait for Everything");
    {
        std::lock_guard lock(observedMutex);
        Check(seenFiles.back().rows.empty(), "IP command clears file results");
    }
    {
        std::lock_guard lock(g_settingsMutex);
        g_settings.commandNames.ip=L"adresse";
    }
    const auto renamedIp = Queue(L"/adresse");
    Check(Apps(renamedIp) && Files(renamedIp), "renamed IP command reaches the local IP provider");
    {
        std::lock_guard lock(observedMutex);
        Check(seenApps.back().rows[0].customGlyph==L"\uE701" && seenFiles.back().rows.empty(), "renamed IP command shows interface rows without file queries");
    }
    filesVisible = false;
    const auto hardwareRevision=Queue(L"/usage");
    Check(Apps(hardwareRevision) && Files(hardwareRevision),"hardware command publishes without waiting for file results");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows[0].title==L"hardware" && seenFiles.back().rows.empty(),"hardware cards replace apps without querying files");}
    Check(hardware::requests.load()==1,"hardware collection starts once for a valid command");
    const auto utilityCatalogVersion=g_appsWarmup->Version();g_appsWarmup->Refresh();
    const auto utilityWait=GetTickCount64();while(g_appsWarmup->Version()==utilityCatalogVersion && GetTickCount64()-utilityWait<2000)Sleep(1);
    Check(g_appsWarmup->Version()>utilityCatalogVersion,"catalog replacement can complete during a live utility command");
    Sleep(100);
    {std::scoped_lock lock(g_queryMutex,g_resultsMutex);Check(g_queryRevision==hardwareRevision && hardware::requests.load()==1 && g_appRows[0].title==L"hardware","a silent catalog change never restarts the hardware monitor or steals its result");}
    const auto usageArgs=Queue(L"/usage unexpected");
    Check(Apps(usageArgs) && Files(usageArgs) && hardware::requests.load()==1,"invalid hardware arguments do not start collection");
    const auto serviceRevision=Queue(L"/services audio");
    Check(Apps(serviceRevision) && Files(serviceRevision),"services command publishes without waiting for file results");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows[0].title==L"services" && seenFiles.back().rows.empty(),"service cards replace apps and keep file searches off");}
    Check(services::requests.load()==1,"service enumeration is scheduled on its own worker");
    const auto suiteRevision=Queue(L"suite");
    Check(Apps(suiteRevision) && Files(suiteRevision),"normal installed-only matching completes promptly");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows.empty(),"installed-only products are excluded from normal app search");}
    const auto inventoryRevision=Queue(L"/all suite");
    Check(Apps(inventoryRevision) && Files(inventoryRevision),"installed inventory publishes without waiting for file results");
    {std::lock_guard lock(observedMutex);const auto& row=seenApps.back().rows[0];Check(row.installedOnly && !row.canRunAsAdmin && !row.isFile && row.openPath==L"ms-settings:appsfeatures","installed-only products keep the explicit marker and safe management destination");Check(row.installedLocation==L"C:\\Program Files\\Suite" && row.installedKey==L"suite-record" && row.installedHive==HKEY_CURRENT_USER && row.installedView==KEY_WOW64_64KEY,"installation folder and exact registry identity reach the UI without a lookup");}
    {std::lock_guard lock(observedMutex);Check(seenFiles.back().rows.empty(),"installed inventory performs no Everything file query");}
    Check(!RequestMoreInstalled(L"stale inventory",1,inventoryRevision),"stale inventory scroll cannot schedule another query");
    Check(!RequestMoreInstalled(L"/all suite",1,inventoryRevision),"a complete filtered inventory has nothing more to load");
    const auto firstBatch=Queue(L"/all");
    Check(Apps(firstBatch) && Files(firstBatch),"unfiltered inventory publishes its first batch");
    std::vector<Row> initial;
    {std::lock_guard lock(observedMutex);initial=seenApps.back().rows;
     Check(initial.size()==20 && initial[0].installedTotal==71,"inventory starts with exactly 20 entries and the full matching count");
     Check(std::all_of(initial.begin(),initial.end(),[](const auto& row){return row.installedOnly && !row.canRunAsAdmin && !row.isFile;}),"inventory never creates launch or administrator targets");}
    Check(!InstalledNearEnd(0,600,1400) && !InstalledNearEnd(100,600,1400),"scrolling near the top does not eagerly load the full inventory");
    Check(InstalledNearEnd(700,600,1400) && InstalledNearEnd(800,600,1400),"approaching the bottom triggers the next batch");
    Check(InstalledNearEnd(0,1200,900),"a tall viewport can request enough rows to become scrollable");
    Check(!InstalledNearEnd(0,0,1400) && !InstalledNearEnd(0,600,0) && !InstalledNearEnd(-1,600,1400) &&
          !InstalledNearEnd(std::numeric_limits<double>::infinity(),600,1400),"unmeasured and invalid scroll metrics do not request content");
    Check(!RequestMoreInstalled(L"/all",19,firstBatch),"a partial or stale visible-card count cannot advance the inventory");
    Check(RequestMoreInstalled(L"/all",20,firstBatch),"the first downward scroll requests another 20 rows");
    Check(!RequestMoreInstalled(L"/all",20,firstBatch),"repeated scroll events cannot duplicate an in-flight batch");
    uint64_t secondBatch;{std::lock_guard lock(g_queryMutex);secondBatch=g_queryRevision;}
    Check(Apps(secondBatch) && Files(secondBatch),"the second batch publishes through the existing app worker");
    {std::lock_guard lock(observedMutex);const auto& rows=seenApps.back().rows;
     Check(rows.size()==40,"the list grows from 20 to 40 without navigation cards");
     Check(std::equal(initial.begin(),initial.end(),rows.begin(),[](const auto& a,const auto& b){return a.title==b.title && a.cardSignature==b.cardSignature && a.appIndex==b.appIndex && a.iconKey==b.iconKey;}),"already visible card identities stay unchanged when new rows append");
     Check(rows.back().installedProtected && !rows.back().installedPackage.empty(),"new component rows retain protection and exact package identity");}
    Check(RequestMoreInstalled(L"/all",40,secondBatch),"the next scroll requests rows 41 through 60");
    uint64_t thirdBatch;{std::lock_guard lock(g_queryMutex);thirdBatch=g_queryRevision;}
    Check(Apps(thirdBatch) && Files(thirdBatch),"the third batch publishes promptly");
    Check(RequestMoreInstalled(L"/all",60,thirdBatch),"the last scroll requests the remaining rows");
    uint64_t lastBatch;{std::lock_guard lock(g_queryMutex);lastBatch=g_queryRevision;}
    Check(Apps(lastBatch) && Files(lastBatch),"the last partial batch publishes promptly");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows.size()==71 && seenFiles.back().rows.empty(),"all 71 records become visible and no Everything request is made");}
    Check(!RequestMoreInstalled(L"/all",71,lastBatch),"scrolling at the real end stops requesting data");
    const auto resetBatch=Queue(L"/all package");
    {std::lock_guard lock(g_queryMutex);Check(g_installedVisibleLimit==20 && g_installedBatchQuery.empty(),"typing a new filter resets the batch size");}
    Check(Apps(resetBatch) && Files(resetBatch),"a new inventory filter publishes promptly");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows.size()==20,"a changed filter returns to the first 20 results");}
    {std::lock_guard lock(g_settingsMutex);g_settings.commandNames.weather=L"adresse";}
    const auto conflict=Queue(L"/adresse");
    Check(Apps(conflict) && Files(conflict),"ambiguous command is handled locally");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows.size()==1 && seenApps.back().rows[0].isError,"ambiguous command shows error instead of running IP or weather");}
    {std::lock_guard lock(g_settingsMutex);g_settings.commandNames=commands::Names{};g_settings.customCommands={{L"/services",L"echo custom"}};}
    const auto customConflict=Queue(L"/services audio");
    Check(Apps(customConflict) && Files(customConflict),"custom and tool name collision publishes promptly");
    {std::lock_guard lock(observedMutex);Check(seenApps.back().rows.size()==1 && seenApps.back().rows[0].isError && seenFiles.back().rows.empty(),"custom slash collision shows only an error and clears file results");}
    Check(services::requests.load()==1,"conflicting tool does not schedule any service collection");
    {std::lock_guard lock(g_settingsMutex);g_settings.customCommands.clear();}
    const auto hidden = Queue(L"Alpha");
    Check(Apps(hidden) && Files(hidden), "apps work with the file panel disabled");
    {
        std::scoped_lock lock(observedMutex, providerMutex);
        Check(seenFiles.back().rows.empty() && fileQueries.back() == L"cats", "hidden files panel performs no file query");
    }
    for (int i = 0; i < 200; ++i) Queue(i % 2 ? L"Alpha" : L"Beta");
    const auto rapid = Queue(L"Beta");
    Check(Apps(rapid) && Files(rapid), "rapid typing converges to the latest revision");
    {
        std::lock_guard lock(observedMutex);
        Check(seenApps.back().rows.size() == 1 && seenApps.back().rows[0].title == L"Beta", "rapid typing shows the final app match");
    }
    {
        std::lock_guard lock(g_queryMutex);
        g_searchQuit = true;
    }
    g_queryWake.notify_all(); g_fileQueryWake.notify_all();
    apps.join(); files.join();
    g_searchThread.emplace([] {});
    StopAppsWarmup();
    Check(g_appsWarmup.has_value(), "warmup ownership survives until the matching thread is joined");
    g_searchThread->join();
    g_searchThread.reset();
    StopAppsWarmup();
    Check(!g_appsWarmup, "controlled unload destroys retained warmup buffers after the last consumer");
    Check(!PublishAppResults(rapid, L"Beta", {}), "no app publication after shutdown");
    Check(!PublishFileResults(rapid, {}, 0), "no file publication after shutdown");
    std::printf("PASS: %d search pipeline checks; apps stayed responsive with blocked files\n", checks);
}
