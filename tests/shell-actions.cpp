// Native menu discovery and package metadata only. Real pin commands are never
// invoked; IContextMenu invocation is exercised through an isolated fake.
#define NOMINMAX
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <appmodel.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winrt/base.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
bool mockPackages=true,hasApps=true,oversize=false,failOpen=false;
int opened=0,closed=0;
const std::wstring family=L"Example.App_abcdefghijklm",full=L"Example.App_1.0.0.0_x64__abcdefghijklm",app=family+L"!App";
LONG WINAPI TestPackages(PCWSTR value,UINT32* count,PWSTR* names,UINT32* length,PWSTR buffer){
    if(!mockPackages)return GetPackagesByPackageFamily(value,count,names,length,buffer);
    if(value!=family){*count=0;return ERROR_NOT_FOUND;}
    *count=1;*length=oversize?2*1024*1024:static_cast<UINT32>(full.size()+1);
    if(!names||!buffer)return ERROR_INSUFFICIENT_BUFFER;
    wcscpy(buffer,full.c_str());names[0]=buffer;return ERROR_SUCCESS;
}
LONG WINAPI TestOpen(PCWSTR value,UINT32 flags,PACKAGE_INFO_REFERENCE* reference){
    if(!mockPackages)return OpenPackageInfoByFullName(value,flags,reference);
    if(failOpen)return ERROR_NOT_FOUND;
    *reference={};++opened;return ERROR_SUCCESS;
}
LONG WINAPI TestClose(PACKAGE_INFO_REFERENCE reference){
    if(!mockPackages)return ClosePackageInfo(reference);++closed;return ERROR_SUCCESS;
}
LONG WINAPI TestIds(PACKAGE_INFO_REFERENCE reference,UINT32* bytes,BYTE* buffer,UINT32* count){
    if(!mockPackages)return GetPackageApplicationIds(reference,bytes,buffer,count);
    *count=hasApps?1:0;
    if(!hasApps){*bytes=0;return ERROR_SUCCESS;}
    *bytes=static_cast<UINT32>(sizeof(PCWSTR)+(app.size()+1)*sizeof(wchar_t));
    if(!buffer)return ERROR_INSUFFICIENT_BUFFER;
    auto text=reinterpret_cast<PWSTR>(buffer+sizeof(PCWSTR));wcscpy(text,app.c_str());
    *reinterpret_cast<PCWSTR*>(buffer)=text;return ERROR_SUCCESS;
}
#define GetPackagesByPackageFamily TestPackages
#define OpenPackageInfoByFullName TestOpen
#define ClosePackageInfo TestClose
#define GetPackageApplicationIds TestIds
int settingsActivations=0,settingsManagers=0;
bool failSettingsCreate=false;
HRESULT settingsResult=S_OK;
std::wstring settingsHost,settingsArguments;
ACTIVATEOPTIONS settingsOptions=AO_NONE;
class Activation final:public IApplicationActivationManager {
    std::atomic<ULONG> refs_{1};
public:
    Activation(){++settingsManagers;}
    ~Activation(){--settingsManagers;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** value)override{
        if(iid!=IID_IUnknown&&iid!=IID_IApplicationActivationManager){*value=nullptr;return E_NOINTERFACE;}
        *value=static_cast<IApplicationActivationManager*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{const auto count=--refs_;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE ActivateApplication(LPCWSTR host,LPCWSTR arguments,ACTIVATEOPTIONS options,DWORD* pid)override{
        ++settingsActivations;settingsHost=host;settingsArguments=arguments;settingsOptions=options;*pid=1;return settingsResult;
    }
    HRESULT STDMETHODCALLTYPE ActivateForFile(LPCWSTR,IShellItemArray*,LPCWSTR,DWORD*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ActivateForProtocol(LPCWSTR,IShellItemArray*,DWORD*)override{return E_NOTIMPL;}
};
HRESULT WINAPI TestCoCreateInstance(REFCLSID clsid,LPUNKNOWN outer,DWORD context,REFIID iid,LPVOID* object){
    if(clsid!=CLSID_ApplicationActivationManager)return CoCreateInstance(clsid,outer,context,iid,object);
    *object=nullptr;
    if(failSettingsCreate)return E_ACCESSDENIED;
    if(iid!=IID_IApplicationActivationManager)return E_NOINTERFACE;
    *object=static_cast<IApplicationActivationManager*>(new Activation);return S_OK;
}
#define CoCreateInstance TestCoCreateInstance
// PRODUCTION_SHELL_ACTIONS
struct Entry {std::wstring label,verb;UINT state=0;};
struct State {std::vector<Entry> entries;int queries=0,invocations=0;UINT chosen=0;HWND owner=nullptr;bool queryFail=false;};
class Context final:public IContextMenu {
    std::atomic<ULONG> refs_{1};std::shared_ptr<State> state_;
public:
    explicit Context(std::shared_ptr<State> state):state_(std::move(state)){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** result)override {
        if(iid!=IID_IUnknown&&iid!=IID_IContextMenu){*result=nullptr;return E_NOINTERFACE;}
        *result=static_cast<IContextMenu*>(this);AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release()override{auto count=--refs_;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE QueryContextMenu(HMENU menu,UINT at,UINT first,UINT,UINT)override {
        ++state_->queries;if(state_->queryFail)return E_FAIL;
        for(size_t i=0;i<state_->entries.size();++i){
            auto const& entry=state_->entries[i];MENUITEMINFOW info{sizeof(info)};
            info.fMask=MIIM_ID|MIIM_STRING|MIIM_STATE;info.wID=first+static_cast<UINT>(i);
            info.dwTypeData=const_cast<PWSTR>(entry.label.c_str());info.fState=entry.state;
            InsertMenuItemW(menu,at+static_cast<UINT>(i),TRUE,&info);
        }
        return MAKE_HRESULT(SEVERITY_SUCCESS,0,state_->entries.size());
    }
    HRESULT STDMETHODCALLTYPE InvokeCommand(LPCMINVOKECOMMANDINFO info)override {
        ++state_->invocations;state_->chosen=LOWORD(info->lpVerb);state_->owner=info->hwnd;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCommandString(UINT_PTR id,UINT flags,UINT*,LPSTR output,UINT length)override {
        if(flags!=GCS_VERBW||id>=state_->entries.size())return E_FAIL;
        wcsncpy(reinterpret_cast<PWSTR>(output),state_->entries[id].verb.c_str(),length-1);
        reinterpret_cast<PWSTR>(output)[length-1]=0;return S_OK;
    }
};
int checks=0;
void Check(bool ok,const char* message){++checks;if(!ok){printf("FAIL %s\n",message);ExitProcess(1);}}
std::unique_ptr<shellactions::Menu> Create(std::shared_ptr<State> state){
    winrt::com_ptr<IContextMenu> context{new Context(state),winrt::take_ownership_from_abi};
    return std::make_unique<shellactions::Menu>(std::move(context));
}
void ShortcutFixture(){
    wchar_t temp[32768]{};Check(GetTempPathW(std::size(temp),temp)>0,"temporary shortcut test directory is available");
    const auto root=std::wstring(temp)+L"start-pin-fixture-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    Check(CreateDirectoryW(root.c_str(),nullptr),"isolated shortcut fixture is created");
    const std::wstring program=L"C:\\Windows\\System32\\notepad.exe",identity=L"Example.Desktop";
    auto save=[&](std::wstring const& name,std::wstring const& id){
        winrt::com_ptr<IShellLinkW> link;winrt::check_hresult(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(link.put())));
        winrt::check_hresult(link->SetPath(program.c_str()));winrt::check_hresult(link->SetArguments(L"--preserve-this"));
        if(!id.empty()){
            constexpr PROPERTYKEY key={{0x9f4c2855,0x9f79,0x4b39,{0xa8,0xd0,0xe1,0xd4,0x2d,0xe1,0xd5,0xf3}},5};
            PROPVARIANT value{};value.vt=VT_LPWSTR;value.pwszVal=const_cast<PWSTR>(id.c_str());
            auto properties=link.as<IPropertyStore>();winrt::check_hresult(properties->SetValue(key,value));winrt::check_hresult(properties->Commit());
        }
        const auto path=root+L"\\"+name;winrt::check_hresult(link.as<IPersistFile>()->Save(path.c_str(),TRUE));return path;
    };
    const auto noId=save(L"a-no-id.lnk",{});const auto wrongId=save(L"b-other-app.lnk",L"Other.Desktop");
    size_t remaining=100;std::wstring fallback;
    Check(shellactions::FindShortcutIn(root,identity,program,remaining,fallback).empty()&&fallback==noId,
        "an exact executable match is retained only when its shortcut has no conflicting app ID");
    const auto exact=save(L"c-original.lnk",identity);remaining=100;fallback.clear();
    Check(shellactions::FindShortcutIn(root,identity,program,remaining,fallback)==exact,
        "an exact app-ID shortcut takes priority over executable-only matches");
    winrt::com_ptr<IShellLinkW> loaded;winrt::check_hresult(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(loaded.put())));
    winrt::check_hresult(loaded.as<IPersistFile>()->Load(exact.c_str(),STGM_READ));wchar_t arguments[100]{};
    Check(SUCCEEDED(loaded->GetArguments(arguments,std::size(arguments)))&&!wcscmp(arguments,L"--preserve-this"),
        "shortcut discovery preserves the original launch arguments");
    remaining=0;fallback.clear();
    Check(shellactions::FindShortcutIn(root,identity,program,remaining,fallback).empty()&&remaining==0&&fallback.empty(),
        "the shortcut scan honors its entry budget without underflow");
    remaining=100;
    Check(shellactions::FindShortcutIn(root,identity,L"C:\\Unrelated.exe",remaining,fallback,17).empty()&&remaining==100,
        "the shortcut scan honors its directory-depth bound");
    // A small reserved stack catches the former 64 KB-per-frame recursion.
    std::vector<std::wstring> folders;
    std::wstring deep = root;
    for (int i = 0; i < 16; ++i) {
        deep += L"\\d";
        Check(CreateDirectoryW(deep.c_str(), nullptr), "create a private deep shortcut-tree fixture");
        folders.push_back(deep);
    }
    struct DeepScan { std::wstring root; bool complete = false; } scan{folders.front()};
    HANDLE thread = CreateThread(nullptr, 128 * 1024, [](void* data) -> DWORD {
        auto& scan = *static_cast<DeepScan*>(data);
        size_t remaining = 4096;
        std::wstring fallback;
        shellactions::FindShortcutIn(scan.root, L"missing", L"C:\\Missing.exe", remaining, fallback);
        scan.complete = true;
        return 0;
    }, &scan, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    Check(thread != nullptr, "create a private scan with a 128 KB stack reserve");
    Check(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0 && scan.complete,
          "sixteen recursive folder levels finish safely on a small stack");
    CloseHandle(thread);
    for (auto it = folders.rbegin(); it != folders.rend(); ++it)
        Check(RemoveDirectoryW(it->c_str()), "remove only the private nested scan directories");
}
int main(int argc,char**) {
    setvbuf(stdout,nullptr,_IONBF,0);winrt::init_apartment(winrt::apartment_type::single_threaded);
    auto state=std::make_shared<State>();state->entries={{L"Open",L"open"},{L"&Pin to Start",L"PinToStartScreen"},
        {L"Pin to tas&kbar\tShortcut",L"taskbarpin"},{L"Delete",L"delete"}};
    auto menu=Create(state);auto pins=menu->Pins();
    Check(state->queries==1&&state->invocations==0,"discovery never invokes a shell command");
    Check(pins.size()==2&&pins[0].kind==shellactions::Pin::Start&&pins[1].kind==shellactions::Pin::Taskbar,"only native pin commands are selected");
    Check(pins[0].label==L"Pin to Start"&&pins[1].label==L"Pin to taskbar","Windows labels lose menu mnemonics and shortcut suffixes");
    Check(shellactions::MenuLabel(L"Pin && keep &this")==L"Pin & keep this","literal ampersands remain in localized Windows labels");
    HWND owner=reinterpret_cast<HWND>(42);
    Check(menu->Invoke(L"taskbarpin",owner)==S_OK&&state->chosen==2&&state->owner==owner,"selection invokes this menu's numeric offset with the correct owner");
    Check(menu->Invoke(L"delete",owner)==E_ACCESSDENIED&&state->invocations==1,"unrelated commands cannot pass the pin invocation gate");
    Check(FAILED(menu->Invoke(L"taskbarunpin",owner))&&state->invocations==1,"missing verbs have no blind fallback");
    state->entries[2].verb=L"taskbarunpin";
    Check(FAILED(menu->Invoke(L"taskbarpin",owner))&&state->invocations==1,"an outdated selected verb cannot invoke a changed command");
    Check(menu->Invoke(L"taskbarunpin",owner)==S_OK&&state->invocations==2,"the current native unpin verb can be invoked");
    state=std::make_shared<State>();state->entries={{L"Unpin from Start",L"UnpinFromStartScreen",MFS_DISABLED},{L"Unpin from taskbar",L"taskbarunpin"}};
    menu=Create(state);pins=menu->Pins();
    Check(pins.size()==2&&!pins[0].enabled&&pins[0].kind==shellactions::Pin::Unstart,"disabled native actions retain their state");
    Check(menu->Invoke(L"UnpinFromStartScreen",owner)==E_ACCESSDENIED&&state->invocations==0,"disabled actions cannot be invoked");
    state->queryFail=true;menu=Create(state);
    Check(menu->Pins().empty()&&FAILED(menu->Invoke(L"taskbarunpin",owner)),"failed shell queries expose and execute nothing");
    Check(shellactions::Target(L"C:\\App.exe",false)==L"C:\\App.exe"&&shellactions::Target(app,false)==L"shell:AppsFolder\\"+app,"file paths and app identities use the correct shell namespace");
    Check(shellactions::Target(L"shell:AppsFolder\\"+app,false)==L"shell:AppsFolder\\"+app,"an existing namespace is not prefixed twice");
    using shellactions::Pin;
    std::vector<shellactions::Command> primary={{Pin::Taskbar,L"taskbarpin",L"Pin to taskbar",true,L"app-id"}};
    std::vector<shellactions::Command> fallback={{Pin::Start,L"PinToStartScreen",L"Pin to Start",true,L"shortcut.lnk"},
        {Pin::Untaskbar,L"taskbarunpin",L"Unpin from taskbar",true,L"shortcut.lnk"}};
    auto merged=shellactions::MergePins(primary,fallback);
    Check(merged.size()==2&&merged[0].target==L"app-id"&&merged[1].target==L"shortcut.lnk","only missing pin destinations use the shortcut fallback");
    Check(merged[0].kind==Pin::Taskbar&&merged[1].kind==Pin::Start,"fallback commands cannot contradict existing native Pin/Unpin state");
    primary.push_back({Pin::Unstart,L"UnpinFromStartScreen",L"Unpin from Start",false,L"app-id"});
    merged=shellactions::MergePins(primary,fallback);
    Check(merged.size()==2&&merged[1].kind==Pin::Unstart&&!merged[1].enabled,"a disabled native Start action is not replaced by the fallback");
    for(auto const& unsupported:{L"C:\\App.exe",L"shell:AppsFolder\\family!App",L"shell:AppsFolder\\",L"shell:AppsFolder\\bad/identity"})
        Check(shellactions::ShortcutTarget(unsupported).empty(),"shortcut fallback is limited to desktop app IDs");
    Check(shellactions::SettingsArguments(app)==L"page=SettingsPageInstalledApps&target=SystemSettings_StorageSense_HiddenAppAdvancedPageLink&parameter="+family+L"&invoke=true","supported app IDs select their exact family and invoke the native advanced-page link");
    Check(shellactions::SettingsArguments(family,full)==L"page=SettingsPageInstalledApps&target=SystemSettings_StorageSense_HiddenAppAdvancedPageLink&parameter="+family+L"&invoke=true","supported inventory packages use the same native advanced-page activation");
    Check(shellactions::SettingsArguments(family,L"different-version").empty(),"inventory metadata cannot silently select another package version");
    Check(shellactions::SettingsArguments(family+L"!Missing").empty(),"a family alone cannot validate an invented app ID");
    for(const auto& invalid:{L"",L"app.exe",L"ms-settings:appsfeatures",L"Example_App?x=1",L"Example_App#fragment",L"C:\\app.exe"})
        Check(shellactions::SettingsArguments(invalid).empty(),"desktop paths and malformed identities have no App settings action");
    hasApps=false;Check(shellactions::SettingsArguments(family,full).empty(),"framework and resource packages without app IDs have no App settings action");
    hasApps=true;oversize=true;Check(shellactions::SettingsArguments(app).empty(),"oversized package metadata is rejected");oversize=false;
    failOpen=true;Check(shellactions::SettingsArguments(app).empty(),"unavailable package metadata hides App settings");failOpen=false;
    Check(opened==closed,"every opened native package reference is closed");
    Check(shellactions::LaunchSettings({})==E_INVALIDARG&&settingsActivations==0,"an empty Settings target never activates an application");
    const auto arguments=shellactions::SettingsArguments(app);
    Check(shellactions::LaunchSettings(arguments)==S_OK&&settingsActivations==1&&settingsArguments==arguments&&
        settingsHost==L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel"&&settingsOptions==AO_NONE,
        "Advanced options activates Windows Settings with the validated page and selected family");
    settingsResult=E_FAIL;
    Check(shellactions::LaunchSettings(arguments)==E_FAIL,"native Settings activation failure is reported to the caller");
    failSettingsCreate=true;
    Check(shellactions::LaunchSettings(arguments)==E_ACCESSDENIED&&settingsActivations==2,"a missing activation manager cannot launch another target");
    Check(settingsManagers==0,"Settings activation managers are released after success and failure");
    failSettingsCreate=false;settingsResult=S_OK;
    ShortcutFixture();
    if(argc>1){
        mockPackages=false;
        const auto uri=shellactions::SettingsArguments(L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App");
        printf("Live Calculator settings arguments: %ls\n",uri.c_str());
        Check(uri==L"page=SettingsPageInstalledApps&target=SystemSettings_StorageSense_HiddenAppAdvancedPageLink&parameter=Microsoft.WindowsCalculator_8wekyb3d8bbwe&invoke=true","real current-user Calculator metadata supports App settings");
        auto native=shellactions::Open(L"C:\\Windows\\System32\\notepad.exe",nullptr);
        Check(bool(native),"public shell APIs bind a real file context menu");
        for(const auto& command:native->Pins())printf("Native discovery only: %ls | %ls\n",command.label.c_str(),command.verb.c_str());
    }
    printf("PASS: %d shell-action checks; no real pin/unpin commands executed\n",checks);
}
