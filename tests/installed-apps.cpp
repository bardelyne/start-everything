#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <appmodel.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <xmllite.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
void Wh_Log(const wchar_t*,...){}
size_t Wh_GetStringValue(const wchar_t*,wchar_t*,size_t){return 0;}
void Wh_SetStringValue(const wchar_t*,const wchar_t*){}
namespace settings {
struct Item {const wchar_t* name;const wchar_t* command;const wchar_t* area;const wchar_t* pageGroup;const wchar_t* altNames;};
struct DestinationCheck {bool Available(const Item&){return true;}};
struct NativeItem {std::wstring name,command,arguments;std::vector<std::wstring> aliases;};
struct NativeCatalog {std::vector<NativeItem> entries;const std::vector<NativeItem>& Get(DestinationCheck&){return entries;}};
const std::vector<Item>& GetSettingsList(){static std::vector<Item> entries;return entries;}
namespace native {void AddAliases(std::vector<std::wstring>& values,const std::wstring& value){values.push_back(value);}}
}
// PRODUCTION_CATALOG
// PRODUCTION_UNINSTALL
DWORD catalogAttributeProbes = 0;
DWORD CatalogAttributes(LPCWSTR path) { ++catalogAttributeProbes; return GetFileAttributesW(path); }
bool blockAppsFolder=false;
HRESULT CatalogShellItem(PCWSTR path,IBindCtx* context,REFIID iid,void** result) {
    if(blockAppsFolder && std::wstring_view(path)==L"shell:AppsFolder"){*result=nullptr;return E_ACCESSDENIED;}
    return SHCreateItemFromParsingName(path,context,iid,result);
}
#define GetFileAttributesW CatalogAttributes
#define SHCreateItemFromParsingName CatalogShellItem
// PRODUCTION_APPS
#undef SHCreateItemFromParsingName
#undef GetFileAttributesW
// PRODUCTION_PACKAGE_ICON
int checks=0;
void Check(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}++checks;}
apps::App Launchable(const wchar_t* name,const wchar_t* target){apps::App app;app.name=name;app.nameLower=apps::ToLower(name);app.targetPath=target;app.targetPathLower=apps::ToLower(target);return app;}
std::wstring VisualName(const std::string& xml){
    auto stream=SHCreateMemStream(reinterpret_cast<const BYTE*>(xml.data()),static_cast<UINT>(xml.size()));
    if(!stream)return {};
    auto name=installed::VisualName(stream,L"Test.Package",L"Test.Package_1.0.0.0_x64__1234567890123");stream->Release();return name;
}
int main(){
    // Package-icon decoding must remain safe when its caller is an STA.
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    Check(!apps::LocalTargetExists(L"\\\\offline.invalid\\share\\app.exe") &&
          !apps::LocalTargetExists(L"ms-settings:display") &&
          !apps::LocalTargetExists(L"shell:AppsFolder") && catalogAttributeProbes == 0,
          "catalog rejects remote and shell targets before any file-existence probe");
    std::atomic<bool> cancelled{true};
    apps::Index cancelledIndex;
    Check(!cancelledIndex.Rebuild(&cancelled) && !cancelledIndex.Refreshed(true, &cancelled),
          "cancelled catalog rebuild and replacement return before enumerating the shell");
    // An isolated volatile test key, outside Windows' real Uninstall catalog.
    // Only parsing/availability is tested; no removal command is executed.
    {
        const auto keyName=L"Software\\StartEverythingTests\\UninstallActions-"+std::to_wstring(GetCurrentProcessId());
        HKEY key=nullptr;
        Check(RegCreateKeyExW(HKEY_CURRENT_USER,keyName.c_str(),0,nullptr,REG_OPTION_VOLATILE,KEY_ALL_ACCESS,nullptr,&key,nullptr)==ERROR_SUCCESS,"private uninstall metadata fixture opens");
        auto string=[&](const wchar_t* name,const std::wstring& value){RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));};
        auto flag=[&](const wchar_t* name,DWORD value){RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));};
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,std::size(exe));
        string(L"DisplayName",L"Test Suite");string(L"UninstallString",L"\""+std::wstring(exe)+L"\" --interactive");
        string(L"QuietUninstallString",L"unwanted-quiet-command.exe /silent");
        auto command=uninstall::RegisteredCommand(key,L"Test Suite");
        Check(command && command->file==exe && command->params==L"--interactive","registered interactive command is preserved without running it");
        Check(!uninstall::RegisteredCommand(key,L"Different Suite"),"same-looking products cannot select a differently named registration");
        for(auto name:{L"NoRemove",L"SystemComponent"}) {flag(name,1);Check(!uninstall::RegisteredCommand(key,L"Test Suite"),"protected registrations have no direct uninstall action");flag(name,0);}
        string(L"ParentKeyName",L"Parent");Check(!uninstall::RegisteredCommand(key,L"Test Suite"),"child registrations are not removable through this menu");RegDeleteValueW(key,L"ParentKeyName");
        string(L"ReleaseType",L"Security Update");Check(!uninstall::RegisteredCommand(key,L"Test Suite"),"Windows updates keep their own removal UI");RegDeleteValueW(key,L"ReleaseType");
        for(auto bad:{L"",L"https://example.invalid/remove",L"relative-uninstaller.exe",L"\\\\server\\share\\remove.exe",L"C:\\missing-test-uninstaller-48277.exe"}) {
            string(L"UninstallString",bad);Check(!uninstall::RegisteredCommand(key,L"Test Suite"),"missing, remote and ambiguous removal executables are rejected");
        }
        string(L"UninstallString",L"MsiExec.exe /I{00000000-0000-0000-0000-000000000000}");
        command=uninstall::RegisteredCommand(key,L"Test Suite");
        Check(command && command->params.starts_with(L"/X{") && command->file.find(L"\\msiexec.exe")!=command->file.npos,"MSI gets the system executable and interactive remove switch");
        Check(!uninstall::FindRegisteredUninstaller(HKEY_CLASSES_ROOT,0,L"suite",L"Test Suite") &&
            !uninstall::FindRegisteredUninstaller(HKEY_CURRENT_USER,KEY_ALL_ACCESS,L"suite",L"Test Suite") &&
            !uninstall::FindRegisteredUninstaller(HKEY_CURRENT_USER,0,L"suite\\child",L"Test Suite"),"registration lookup rejects unrelated hives, views and nested keys");
        RegCloseKey(key);RegDeleteKeyW(HKEY_CURRENT_USER,keyName.c_str());
        auto folder=uninstall::Folder(exe)+L"\\folder,123";CreateDirectoryW(folder.c_str(),nullptr);
        Check(installed::Location(L"\""+folder+L"\"")==folder,"installation paths retain valid commas and accept enclosing quotes");RemoveDirectoryW(folder.c_str());
        Check(installed::Location(exe).empty() && installed::Location(L"https://example.invalid").empty() && installed::Location(L"\\\\server\\share").empty(),"path actions require an existing local directory");
    }
    Check(installed::Visible(L"Office",0,L"",L""),"normal installed programs are visible without requiring an uninstaller");
    Check(!installed::Visible(L"",0,L"",L"") && !installed::Visible(L"hidden",1,L"",L""),"unnamed and hidden system-component registrations are excluded");
    Check(!installed::Visible(L"patch",0,L"parent",L"") && !installed::Visible(L"patch",0,L"",L"Security Update"),"child update registrations do not masquerade as installed apps");
    Check(installed::CleanIcon(L"\"C:\\Program Files\\Product\\icon.exe\",0")==L"C:\\Program Files\\Product\\icon.exe","quoted executable icon metadata is parsed without executing it");
    Check(installed::CleanIcon(L"C:\\Program Files\\Product\\icons.dll,-15")==L"C:\\Program Files\\Product\\icons.dll","resource index is separated from an icon filename");
    Check(installed::CleanIcon(L"https://example.com/icon.exe").empty() && installed::CleanIcon(L"\\\\server\\share\\app.exe").empty(),"remote icon locations are not fetched");
    {
        wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,std::size(module));
        const auto output=installed::ParentFolder(module),root=output+L"\\launcher-discovery-"+std::to_wstring(GetCurrentProcessId());
        const auto folder=root+L"\\Fixture Tool",target=folder+L"\\FixtureTool.exe";
        Check(CreateDirectoryW(root.c_str(),nullptr) && CreateDirectoryW(folder.c_str(),nullptr),"private launcher fixture directories created");
        Check(CopyFileW((output+L"\\launcher-gui.exe").c_str(),target.c_str(),FALSE),"own GUI fixture copied without executing it");
        installed::Record product;product.id=L"fixture-launcher";product.name=L"Fixture Tool 1.0";product.version=L"1.0";
        product.icon=folder+L"\\Uninstall.exe";product.registryKey=L"fixture-exact-registration";product.registryHive=HKEY_CURRENT_USER;product.registryView=KEY_WOW64_64KEY;
        Check(installed::LauncherName(product.name,product.version)==L"fixturetool" &&
            installed::LauncherName(L"Fixture Tool 1.0 (x64)",L"1.0")==L"fixturetool","only registered version/architecture suffixes are removed for product identity");
        Check(installed::LauncherName(L"Tool 2026",L"1.0")==L"tool2026","meaningful product digits are not discarded");
        Check(installed::GuiExecutable(target) && installed::LauncherProductMatches(target,L"fixturetool",L"1.0"),"GUI PE header and matching file-version metadata are required");
        Check(installed::FindLauncher(product)==target,"missing shortcut recovers a uniquely matching application beside its registered uninstaller icon");
        product.launcher=installed::FindLauncher(product);product.location=folder;
        std::vector<apps::App> recovered;apps::AppendInstalled(recovered,{product});
        Check(recovered.size()==1 && !recovered[0].installedOnly && recovered[0].targetExists && recovered[0].targetPath==target && recovered[0].pidl,"recovered launcher becomes a real executable app with a shell icon identity");
        Check(apps::ScoreApp(recovered[0],L"fixturetool")>=0 && recovered[0].installedKey==product.registryKey,"recovered executable name is searchable and keeps the exact uninstall registration");
        std::vector<apps::App> withShortcut;withShortcut.push_back(Launchable(L"Fixture Tool",target.c_str()));
        apps::AppendInstalled(withShortcut,{product});
        Check(withShortcut.size()==1 && std::find(withShortcut[0].aliases.begin(),withShortcut[0].aliases.end(),L"fixture tool 1.0")!=withShortcut[0].aliases.end(),"an existing launcher to the same executable stays preferred and receives the versioned product alias");
        const auto duplicate=folder+L"\\Fixture Tool.exe";CopyFileW(target.c_str(),duplicate.c_str(),FALSE);
        Check(installed::FindLauncher(product).empty(),"multiple equally valid launch candidates remain management-only");DeleteFileW(duplicate.c_str());
        product.name=L"Other Tool 1.0";const auto wrong=folder+L"\\OtherTool.exe";CopyFileW(target.c_str(),wrong.c_str(),FALSE);
        Check(installed::FindLauncher(product).empty(),"matching filenames cannot override different embedded product metadata");DeleteFileW(wrong.c_str());product.name=L"Fixture Tool 1.0";
        CopyFileW((output+L"\\launcher-console.exe").c_str(),target.c_str(),FALSE);
        Check(!installed::GuiExecutable(target) && installed::LauncherProductMatches(target,L"fixturetool",L"1.0") && installed::FindLauncher(product).empty(),"matching console/service-style executables do not become guessed GUI launchers");
        Check(installed::AuxiliaryExecutable(L"FixtureToolSetup.exe") && installed::AuxiliaryExecutable(L"Uninstall.exe") &&
            installed::AuxiliaryExecutable(L"FixtureToolHelper.exe") && !installed::AuxiliaryExecutable(L"FixtureTool.exe"),"installer, remover and helper executables are excluded");
        CopyFileW((output+L"\\launcher-gui.exe").c_str(),target.c_str(),FALSE);
        product.location.clear();product.icon=root+L"\\Uninstall.exe";CopyFileW(target.c_str(),(root+L"\\FixtureTool.exe").c_str(),FALSE);
        Check(installed::FindLauncher(product).empty(),"an icon in a shared or differently named folder is insufficient to discover a launcher");
        product.location=L"C:\\";Check(installed::FindLauncher(product).empty(),"a drive root is never enumerated for guessed launchers");
        product.location=folder;product.family=L"Package_1234567890123";
        Check(installed::FindLauncher(product).empty(),"package activation cannot be replaced with an arbitrary package executable");
        DeleteFileW(target.c_str());DeleteFileW((root+L"\\FixtureTool.exe").c_str());RemoveDirectoryW(folder.c_str());RemoveDirectoryW(root.c_str());
    }
    Check(VisualName("<Package><Applications><Application><VisualElements DisplayName='Windows App Runtime Main' AppListEntry='none'/></Application></Applications></Package>")==L"Windows App Runtime Main","hidden application uses its friendly name without making it launchable");
    Check(VisualName("<Package><Applications><Application><VisualElements DisplayName='Tool A'/></Application><Application><VisualElements DisplayName='Tool B'/></Application></Applications></Package>").empty(),"suite does not inherit the name of an arbitrary child application");
    Check(VisualName("<Package><Applications><Application><VisualElements DisplayName='Tools &amp; Utilities'/></Application></Applications></Package>")==L"Tools & Utilities","manifest entities in normal display names decode correctly");
    Check(VisualName("<!DOCTYPE Package [<!ENTITY name SYSTEM 'file:///C:/Windows/win.ini'>]><Package><Applications><Application><VisualElements DisplayName='&name;'/></Application></Applications></Package>").empty(),"manifest DTD and external entities are prohibited");
    Check(VisualName("<Package><Applications><Application><VisualElements DisplayName='broken'/>").empty(),"truncated manifests do not publish partial friendly names");
    std::vector<apps::App> entries;
    entries.push_back(Launchable(L"Discord",L"C:\\Apps\\Discord.exe"));
    entries.push_back(Launchable(L"Telegram",L"C:\\Apps\\Telegram.exe"));
    entries.push_back(Launchable(L"Store app",L"Family_1234567890123!App"));
    std::vector<installed::Record> products{{L"discord",L"Discord",L"Vendor",L"1.0",L"",L""},
        {L"telegram",L"Telegram Desktop",L"Vendor",L"2.0",L"C:\\Apps\\Telegram.exe",L""},
        {L"office",L"Microsoft 365 Apps for enterprise - en-us",L"Microsoft",L"16.0",L"C:\\Installer\\OfficeClickToRun.exe",L""},
        {L"package",L"Store app package",L"Publisher",L"1.0",L"",L"Family_1234567890123"},
        {L"runtime1",L"Runtime",L"Vendor",L"1.0",L"",L""},
        {L"runtime2",L"Runtime",L"Vendor",L"2.0",L"",L""}};
    apps::AppendInstalled(entries,products);
    Check(entries.size()==3,"ordinary catalog stores launchers without a second installed-only inventory");
    Check(!entries[0].installedOnly && !entries[1].installedOnly && !entries[2].installedOnly,"merging metadata does not turn launchable apps into management entries");
    Check(std::find(entries[1].aliases.begin(),entries[1].aliases.end(),L"telegram desktop")!=entries[1].aliases.end(),"Installed apps naming becomes a searchable alias on the real launcher");
    auto inventoryEntries=apps::InstalledInventory(products);
    const auto& office=inventoryEntries[2];
    Check(office.installedOnly && office.targetPath==L"ms-settings:appsfeatures" && !office.pidl,"suite without a launch entry has only a safe management destination");
    Check(office.installedIcon==L"C:\\Installer\\OfficeClickToRun.exe" && office.exeNameLower.empty() && office.targetPathLower.empty(),"an icon or installer executable never becomes a launch target or executable-name match");
    Check(apps::ScoreApp(office,L"en-us")>=0 && apps::ScoreApp(office,L"en-us")<80,"suite language suffix is discoverable as a real name match");
    Check(!apps::SameResult(inventoryEntries[4],inventoryEntries[5]) && apps::SameResult(inventoryEntries[4],inventoryEntries[4]),"distinct installed registrations retain separate result identities");
    Check(apps::IconIdentity(inventoryEntries[4])!=apps::IconIdentity(inventoryEntries[5]),"versioned products do not share cached icons just because their captions match");
    {
        auto settings=Launchable(L"Runtime settings",L"ms-settings:appsfeatures");settings.isSetting=true;
        entries.push_back(std::move(settings));
        installed::Record component;component.id=L"hidden-component";component.name=L"Hidden Runtime";component.inventoryOnly=true;
        component.launcher=L"C:\\Apps\\NeverRunThis.exe";
        apps::AppendInstalled(entries,{component});
        Check(entries.size()==4 && std::none_of(entries.begin(),entries.end(),[](const auto& app){return app.inventoryOnly;}),
              "hidden components cannot acquire a guessed launch target");
        auto normal=apps::MatchCatalog(entries,L"runtime",100,nullptr,apps::SearchScope::Normal);
        Check(normal.size()==1 && normal[0].app->isSetting,"normal search keeps Settings and excludes installed-only and hidden components");
        auto records=products;records.push_back(component);
        auto direct=apps::InstalledInventory(records);
        auto inventory=apps::MatchCatalog(direct,L"runtime",100,nullptr,apps::SearchScope::Installed);
        Check(inventory.size()==3 && std::none_of(inventory.begin(),inventory.end(),[](const auto& hit){return hit.app->isSetting;}),
              "installed inventory includes hidden components and both runtime versions without Settings pages");
        const auto all=apps::MatchCatalog(direct,L"",100,nullptr,apps::SearchScope::Installed);
        Check(all.size()==records.size(),"empty installed inventory comes directly from all registrations and packages without AppsFolder entries");
        Check(std::all_of(direct.begin(),direct.end(),[](const auto& app){
            return app.installedOnly && !app.pidl && !app.targetExists && app.linkTarget.empty() && app.targetPath==L"ms-settings:appsfeatures";
        }),"even launchable products are management-only in the installed inventory");
        Check(apps::InstalledInventory({products[0],products[0]}).size()==1,"repeated inventory records do not create duplicate management cards");
        Check(apps::MatchCatalog(entries,L"",100,nullptr,apps::SearchScope::Normal).empty(),"normal empty queries keep their existing suggestion path");
        Check(apps::MatchCatalog(direct,L"en-us",100,nullptr,apps::SearchScope::Installed).size()==1,
              "unlaunchable suite metadata is searchable in inventory");
        Check(apps::MatchCatalog(direct,L"runtime",1,nullptr,apps::SearchScope::Installed).size()==1,
              "inventory rendering respects the result limit without discarding catalog entries");
        entries.pop_back();
    }
    std::vector<apps::App> packaged;
    auto executable=Launchable(L"Executable launcher",L"C:\\Apps\\Store.exe");
    executable.shellIdentity=L"Family_1234567890123!App";
    packaged.push_back(std::move(executable));
    apps::AppendInstalled(packaged,{products[3]});
    Check(packaged.size()==1 && !packaged[0].installedOnly,"package identity survives resolving a shell launcher to an executable");
    packaged.push_back(Launchable(L"Second launcher",L"Family_1234567890123!Other"));
    apps::AppendInstalled(packaged,{products[3]});
    Check(packaged.size()==2 && apps::InstalledInventory({products[3]}).size()==1,"multi-app package remains in inventory without choosing an arbitrary launcher");
    installed::Record friendly;friendly.id=L"package:friendly";friendly.name=L"Windows App Runtime Main";friendly.family=L"Runtime_1234567890123";
    friendly.packageFullName=L"Runtime_1.0.0.0_x64__1234567890123";friendly.aliases={L"WinAppRuntime.Main.2"};
    friendly.location=L"C:\\Package Location";friendly.registryKey=L"exact-record";friendly.registryHive=HKEY_CURRENT_USER;friendly.registryView=KEY_WOW64_64KEY;
    auto friendlyEntries=apps::InstalledInventory({friendly});
    Check(friendlyEntries[0].installedOnly && friendlyEntries[0].name==friendly.name && friendlyEntries[0].installedPackage==friendly.packageFullName,"friendly title and package icon identity retain the management-only behavior");
    Check(apps::ScoreApp(friendlyEntries[0],L"winappruntime.main.2")>=0,"old technical package name remains searchable as an alias");
    Check(friendlyEntries[0].installedLocation==friendly.location && friendlyEntries[0].installedFamily==friendly.family &&
        friendlyEntries[0].installedKey==friendly.registryKey && friendlyEntries[0].installedHive==friendly.registryHive && friendlyEntries[0].installedView==friendly.registryView,"installed-only results retain the exact folder, package family and registry registration");
    {
        installed::Record product;product.id=L"package:calc";product.name=L"Calculator";product.family=L"Calc_1234567890123";
        product.architecture=L"x64";product.packageFullName=L"Microsoft.Calculator_1.2.3.4_x64__1234567890123";
        product.inventoryAliases={L"Microsoft.Calculator",product.packageFullName};
        std::vector<apps::App> launchers;launchers.push_back(Launchable(L"Calculator",L"Calc_1234567890123!App"));
        apps::AppendInstalled(launchers,{product});
        for(const auto query:{L"x64",L"1.2.3.4",L"microsoft.calculator"})
            Check(apps::MatchCatalog(launchers,query,20,nullptr,apps::SearchScope::Normal).empty(),
                  "technical package identities cannot pollute ordinary app matching");
        auto inventory=apps::InstalledInventory({product});
        Check(apps::MatchCatalog(inventory,L"x64",20,nullptr,apps::SearchScope::Installed).size()==1 &&
              inventory[0].installedArchitecture==L"x64","inventory retains architecture search and exact architecture metadata");
    }
    {
        installed::Record product;product.id=L"different-caption";product.name=L"playit";product.version=L"1.0.10";product.location=L"C:\\Apps\\Playit\\";
        const auto launcher=[](const wchar_t* path){auto app=Launchable(L"Playit.gg",path);app.shellIdentity=L"Original.Shortcut.Identity";return app;};
        std::vector<apps::App> merged;merged.push_back(launcher(L"C:\\Apps\\Playit\\bin\\playit.exe"));
        apps::AppendInstalled(merged,{product});
        Check(merged.size()==1 && !merged[0].installedOnly && merged[0].name==L"Playit.gg","registered product and differently named shortcut merge through the executable under its installation folder");
        Check(merged[0].targetPath==L"C:\\Apps\\Playit\\bin\\playit.exe" && merged[0].shellIdentity==L"Original.Shortcut.Identity" && apps::ScoreApp(merged[0],L"playit")==4,"merging adds the product alias while preserving the original shortcut identity and target");
        std::vector<apps::App> sibling;sibling.push_back(launcher(L"C:\\Apps\\Playit-other\\bin\\playit.exe"));apps::AppendInstalled(sibling,{product});
        Check(sibling.size()==1 && apps::ScoreApp(sibling[0],L"playit")!=4,"a sibling folder with the same executable basename is a different installation");
        std::vector<apps::App> different;different.push_back(launcher(L"D:\\Other\\playit.exe"));apps::AppendInstalled(different,{product});
        Check(different.size()==1 && apps::ScoreApp(different[0],L"playit")!=4,"a same-named executable on another drive cannot hide the registered product");
        std::vector<apps::App> ambiguous;ambiguous.push_back(launcher(L"C:\\Apps\\Playit\\v1\\playit.exe"));ambiguous.push_back(launcher(L"C:\\Apps\\Playit\\v2\\playit.exe"));apps::AppendInstalled(ambiguous,{product});
        Check(ambiguous.size()==2 && std::none_of(ambiguous.begin(),ambiguous.end(),[](const auto& app){return apps::ScoreApp(app,L"playit")==4;}),"different matching executables within one product folder remain ambiguous");
        std::vector<apps::App> sameFile;sameFile.push_back(launcher(L"C:\\Apps\\Playit\\bin\\playit.exe"));sameFile.push_back(Launchable(L"Playit alternate shortcut",L"C:\\Apps\\Playit\\bin\\playit.exe"));apps::AppendInstalled(sameFile,{product});
        Check(sameFile.size()==2 && std::all_of(sameFile.begin(),sameFile.end(),[](const auto& app){return !app.installedOnly && apps::ScoreApp(app,L"playit")==4;}),"multiple real shortcuts to the same file retain their actions without an extra management result");
        std::vector<apps::App> resolved;auto app=launcher(L"App.Shortcut.ID");app.linkTarget=L"C:/Apps/Playit/./bin/../bin/PLAYIT.EXE";resolved.push_back(std::move(app));apps::AppendInstalled(resolved,{product});
        Check(resolved.size()==1 && resolved[0].targetPath==L"App.Shortcut.ID","case, separators and dot segments normalize while app-ID activation stays intact");
        product.location=L"C:\\";std::vector<apps::App> root;root.push_back(launcher(L"C:\\Apps\\Playit\\bin\\playit.exe"));apps::AppendInstalled(root,{product});
        Check(root.size()==1 && apps::ScoreApp(root[0],L"playit")!=4,"drive-root installation metadata cannot merge unrelated programs");
        PWSTR common=nullptr;Check(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&common)) && common,"the shared Program Files folder is available for a boundary check");
        product.location=common;std::vector<apps::App> shared;const auto sharedTarget=product.location+L"\\Playit\\bin\\playit.exe";shared.push_back(launcher(sharedTarget.c_str()));apps::AppendInstalled(shared,{product});CoTaskMemFree(common);
        Check(shared.size()==1 && apps::ScoreApp(shared[0],L"playit")!=4,"a shared system installation root is not sufficient product identity");
        product.location=L"C:\\Apps\\Playit";product.family=L"SomePackage_1234567890123";std::vector<apps::App> package;package.push_back(launcher(L"C:\\Apps\\Playit\\bin\\playit.exe"));apps::AppendInstalled(package,{product});
        Check(package.size()==1 && apps::ScoreApp(package[0],L"playit")!=4,"package registration still requires its package family identity");
        product.family.clear();product.name=L"Play It";std::vector<apps::App> punctuation;punctuation.push_back(launcher(L"C:\\Apps\\Playit\\bin\\Play_It.exe"));apps::AppendInstalled(punctuation,{product});
        Check(punctuation.size()==1,"filename punctuation is compared as part of the complete product name");
        product.name=L"Suite";std::vector<apps::App> suite;suite.push_back(Launchable(L"Suite editor",L"C:\\Apps\\Playit\\bin\\editor.exe"));apps::AppendInstalled(suite,{product});
        Check(suite.size()==1 && apps::ScoreApp(suite[0],L"suite")!=4,"a suite record is not replaced by an arbitrary program in its folder");
    }
    const auto programs=installed::ReadPrograms(),packages=installed::ReadPackages();
    Check(!programs.empty(),"read-only native enumeration includes registered installed programs");
    Check(std::all_of(programs.begin(),programs.end(),[](const auto& product){return !product.id.empty() && !product.name.empty();}),"native program records carry stable names and identities");
    size_t checkedLogos=0;
    Check(std::any_of(packages.begin(),packages.end(),[](const auto& record){return record.inventoryOnly;}),
          "native current-user package enumeration includes framework or system components");
    std::set<std::wstring> packageIds;
    for(const auto& record:packages)Check(packageIds.insert(record.packageFullName).second && record.id==L"package:"+record.packageFullName,
          "package inventory keeps distinct full package identities instead of collapsing versions by family");
    for(const auto& record:packages)if(!record.inventoryOnly && record.family.find(L"WinAppRuntime")!=record.family.npos){
        Check(record.name.find(L"Windows App Runtime")!=record.name.npos,"this PC's runtime package uses the same friendly application name as Windows Settings");
        Check(!record.aliases.empty(),"technical runtime name is kept as a search alias");
        std::vector<BYTE> pixels;
        Check(FetchInstalledPackageIcon(record.packageFullName,24,&pixels) && pixels.size()==24*24*4,"the actual runtime package logo decodes at row size");
        Check(FetchInstalledPackageIcon(record.packageFullName,36,&pixels) && pixels.size()==36*36*4,"runtime logo resolves at a higher display scale too");
        ++checkedLogos;
    }
    if(checkedLogos)std::printf("Verified %zu friendly runtime names and package logos\n",checkedLogos);
    apps::Index index;const auto began=GetTickCount64();
    if(!index.Rebuild()){std::fprintf(stderr,"Native AppsFolder enumeration needs normal host access\n");return 2;}
    Check(index.Count()>0,"combined app index builds using real shell and installation metadata");
    {
        blockAppsFolder=true;apps::Index withoutShell;
        const bool built=withoutShell.Rebuild();blockAppsFolder=false;
        Check(built && !withoutShell.Search(L"",1000,nullptr,apps::SearchScope::Installed).empty(),
              "installed inventory still builds when AppsFolder is unavailable");
    }
    {
        const auto count=index.Count();blockAppsFolder=true;
        const auto failed=index.Refreshed(true);const bool replaced=index.Rebuild();blockAppsFolder=false;
        Check(!failed && !replaced && index.Count()==count,
              "failed AppsFolder refresh retains the complete published launcher catalog");
    }
    for(size_t i=0;i<index.Count();++i) {
        const auto* app=index.At(i);
        if(!app->installedOnly && !app->installedId.empty())
            std::printf("Recovered local launcher: %ls -> %ls\n",app->name.c_str(),app->targetPath.c_str());
    }
    for(const auto& record:programs)if(record.name.starts_with(L"MSI Afterburner")) {
        const std::wstring expected=L"C:\\Program Files (x86)\\MSI Afterburner\\MSIAfterburner.exe";
        if(GetFileAttributesW(expected.c_str())!=INVALID_FILE_ATTRIBUTES) {
            Check(record.launcher==expected,"this PC's Afterburner main executable is recovered without a shortcut");
            const auto matches=index.Search(L"afterburner",100,nullptr);
            Check(std::any_of(matches.begin(),matches.end(),[&](const auto& match){return !match.app->installedOnly && match.app->targetExists && match.app->targetPath==expected && match.app->pidl;}),"Afterburner appears as an actual launchable application in native search");
            Check(uninstall::FindRegisteredUninstaller(record.registryHive,record.registryView,record.registryKey,record.name).has_value(),"Afterburner keeps its exact interactive removal action, without executing it");
            std::puts("Verified MSI Afterburner launcher without running it");
        }
    }
    for(const auto& record:programs)if(_wcsicmp(record.name.c_str(),L"playit")==0) {
        const auto target=record.location+(record.location.ends_with(L"\\")?L"":L"\\")+L"bin\\playit.exe";
        if(GetFileAttributesW(target.c_str())!=INVALID_FILE_ATTRIBUTES) {
            const auto matches=index.Search(L"playit",100,nullptr);
            const auto main=std::find_if(matches.begin(),matches.end(),[&](const auto& match){return !match.app->installedOnly && _wcsicmp((match.app->linkTarget.empty()?match.app->targetPath:match.app->linkTarget).c_str(),target.c_str())==0;});
            Check(main!=matches.end() && main->app->name==L"Playit.gg" && main->app->pidl,"this PC's real Playit shortcut remains the launchable result with its shell icon");
            Check(std::none_of(matches.begin(),matches.end(),[&](const auto& match){return match.app->installedOnly && match.app->installedId==record.id;}),"the same Playit installation no longer has a second management-only search result");
            Check(main!=matches.end() && std::find(main->app->aliases.begin(),main->app->aliases.end(),L"playit")!=main->app->aliases.end(),"Windows' Installed apps name remains searchable on the Playit.gg launcher");
            Check(main!=matches.end() && uninstall::FindProgramUninstaller(target,main->app->name).has_value(),"the preserved Playit launcher still resolves its removal action without executing it");
            std::puts("Verified Playit registration and differently named shortcut merge into the real launcher");
        }
    }
    std::vector<double> letterTimes;
    for(int repeat=0;repeat<5;++repeat)for(const auto* letter:{L"a",L"c",L"s",L"t",L"w"}){
        const auto start=std::chrono::steady_clock::now();const auto matches=index.Search(letter,20,nullptr);
        letterTimes.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        Check(!matches.empty(),"native single-letter matching uses the already built catalog");
    }
    std::sort(letterTimes.begin(),letterTimes.end());
    std::printf("Native single-letter matching: median %.2f ms, max %.2f ms (%zu samples; excludes UI presentation)\n",letterTimes[letterTimes.size()/2],letterTimes.back(),letterTimes.size());
    apps::WarmIndex warmer;
    Check(warmer.Start(),"real native app catalog can warm without creating the search UI");
    auto warmed=warmer.Take();
    Check(warmed.index && warmed.builtAt && warmed.index->Count()>0,"native PIDL and Settings data transfer from the warmup apartment to the search owner");
    Check(!warmed.index->Search(L"en-us",100,nullptr,apps::SearchScope::Installed).empty(),"transferred inventory remains searchable without a second rebuild");
    auto refreshed=warmed.index->Refreshed(true);
    Check(refreshed && refreshed!=warmed.index && refreshed->Count()>0,"a native catalog replacement builds independently with an explicit installed-metadata invalidation");
    Check(!warmed.index->Search(L"en-us",100,nullptr,apps::SearchScope::Installed).empty() && !refreshed->Search(L"en-us",100,nullptr,apps::SearchScope::Installed).empty(),"both old and refreshed native snapshots keep valid inventory identities and PIDLs");
    warmer.Stop();
    size_t installedCount=0;
    for(size_t i=0;i<index.Count();++i){const auto* app=index.At(i);if(app->installedOnly)++installedCount;}
    Check(installedCount==0,"ordinary snapshots do not retain a duplicate installed-only catalog");
    for(const auto& record:programs)if(record.name.find(L"Microsoft 365 Apps for enterprise")!=std::wstring::npos){
        const auto matches=index.Search(L"en-us",100,nullptr,apps::SearchScope::Installed);
        Check(std::any_of(matches.begin(),matches.end(),[&](const auto& match){return match.app->name==record.name && match.app->installedOnly;}),"this PC's Microsoft 365 en-us suite is present in search");
        const auto ordinary=index.Search(L"en-us",100,nullptr);
        Check(std::none_of(ordinary.begin(),ordinary.end(),[&](const auto& match){return match.app->installedId==record.id;}),"this PC's installed-only Office suite no longer clutters normal search");
        std::puts("Verified Microsoft 365 en-us suite as an installed-only search result");
    }
    for(const auto& match:index.Suggest(1000,nullptr))Check(!match.app->installedOnly,"launch suggestions remain actual launch entries");
    std::printf("PASS: %d installed-app checks; %zu program records, %zu package records, %zu installed-only results; native rebuild %llu ms\n",checks,programs.size(),packages.size(),installedCount,static_cast<unsigned long long>(GetTickCount64()-began));
    winrt::uninit_apartment();
}
