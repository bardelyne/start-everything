// Offscreen, isolated XAML host. Does not touch Explorer or inject user input.
#define NOMINMAX
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <optional>
#include <limits>
#include <string>
#include <vector>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
enum class CustomCommandShell { Cmd };
// PRODUCTION_MODEL
std::mutex g_queryMutex;
std::condition_variable g_queryWake,g_fileQueryWake;
std::atomic<bool> g_searchQuit{false},g_queryDirty{false},g_isOverlayVisible{true};
std::wstring g_pendingQuery=L"/all",g_shownQuery=g_pendingQuery;
uint64_t g_queryRevision=1,g_shownAppsRevision=1;
// PRODUCTION_BATCH
// PRODUCTION_RESULT_BATCH
std::vector<Row> g_currentAppRows,g_currentFileRows;
uint64_t g_shownFilesRevision=1;
bool FilesPanelVisible(){return true;}
wuxc::ScrollViewer g_appsScroll{nullptr},g_filesScroll{nullptr};
double testFontScale=1;
double Fs(double value){return value*testFontScale;}
constexpr int kIconDisplay=24;
wuxc::StackPanel g_resultsList{nullptr};
std::optional<std::vector<wuxc::Button>> g_fileButtonsOpt{std::in_place};
// PRODUCTION_FILE_CARDS
// PRODUCTION_INITIAL_VIEWPORT
// PRODUCTION_SCROLL
int checks=0;
void Check(bool value,const char* label){++checks;if(!value){printf("FAIL: %s\n",label);ExitProcess(1);}}
void Pump(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}}
int main(){
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Isolated inventory scrolling test",WS_POPUP,-32000,-32000,360,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        auto source=wux::Hosting::DesktopWindowXamlSource();
        auto native=source.as<IDesktopWindowXamlSourceNative>();native->AttachToWindow(hwnd);
        HWND child=nullptr;native->get_WindowHandle(&child);SetWindowPos(child,nullptr,0,0,360,400,SWP_NOZORDER|SWP_NOACTIVATE);
        wuxc::StackPanel rows;g_appsScroll=wuxc::ScrollViewer();g_appsScroll.Width(360);g_appsScroll.Height(400);
        g_appsScroll.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);g_appsScroll.Content(rows);source.Content(g_appsScroll);
        std::vector<wuxc::Button> originals;
        auto append=[&](unsigned count){for(unsigned i=0;i<count;++i){wuxc::Button row;row.Height(50);row.Content(winrt::box_value(L"Installed product"));rows.Children().Append(row);originals.push_back(row);}};
        float viewport=400;
        ShowWindow(hwnd,SW_SHOWNOACTIVATE);
        auto layout=[&]{
            SetWindowPos(child,nullptr,0,0,360,static_cast<int>(viewport),SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
            g_appsScroll.Width(360);g_appsScroll.Height(viewport);g_appsScroll.ApplyTemplate();
            g_appsScroll.Measure({360,viewport});g_appsScroll.Arrange({0,0,360,viewport});g_appsScroll.UpdateLayout();Pump();
        };
        append(20);layout();
        const auto initialDeadline=GetTickCount64()+3000;
        while(g_appsScroll.ViewportHeight()<=0 && GetTickCount64()<initialDeadline){Pump();Sleep(5);layout();}
        if(g_appsScroll.ExtentHeight()<999 || g_appsScroll.ViewportHeight()<=0)
            printf("Initial scroll metrics: extent %.3f, viewport %.3f\n",g_appsScroll.ExtentHeight(),g_appsScroll.ViewportHeight());
        Check(g_appsScroll.ExtentHeight()>=999 && g_appsScroll.ViewportHeight()>0,"real ScrollViewer measures the first 20 cards");
        UpdateInitialResultCounts();
        const auto automatic=g_initialAppResults.load();
        Check(automatic==InitialResultCount(g_appsScroll.ViewportHeight(),48),"real Apps viewport chooses its first batch without a setting");
        testFontScale=2;UpdateInitialResultCounts();
        Check(g_initialAppResults<automatic,"larger text reduces the automatically requested first batch");
        testFontScale=1;UpdateInitialResultCounts();
        g_currentAppRows.resize(20);for(auto& row:g_currentAppRows){row.installedOnly=true;row.installedTotal=60;}
        g_installedBatchQuery=g_pendingQuery;g_installedTotal=60;g_installedVisibleLimit=20;
        auto event=g_appsScroll.ViewChanged(winrt::auto_revoke,[](wf::IInspectable const& sender,auto const&){CheckInstalledScroll(sender.as<wuxc::ScrollViewer>());});
        CheckInstalledScroll(g_appsScroll);Check(g_queryRevision==1,"showing the top of a scrollable list does not eagerly load everything");
        g_appsScroll.ChangeView(nullptr,g_appsScroll.ScrollableHeight(),nullptr,true);
        const auto until=GetTickCount64()+3000;
        while(g_queryRevision==1 && GetTickCount64()<until){layout();Sleep(5);}
        Check(g_queryRevision==2 && g_installedVisibleLimit==60,"native ViewChanged requests exactly the next 40 entries");
        CheckInstalledScroll(g_appsScroll);Check(g_queryRevision==2,"an unrendered batch cannot be requested twice");
        const auto offset=g_appsScroll.VerticalOffset();
        append(40);g_currentAppRows.resize(60);
        for(auto& row:g_currentAppRows){row.installedOnly=true;row.installedTotal=60;}
        g_shownAppsRevision=g_queryRevision;layout();
        Check(rows.Children().Size()==60 && g_appsScroll.ExtentHeight()>=2999,"appending extends the existing content instead of replacing it");
        Check(std::abs(g_appsScroll.VerticalOffset()-offset)<1,"native scroll offset stays stable when the next batch appends");
        for(unsigned i=0;i<20;++i)Check(rows.Children().GetAt(i)==originals[i],"previously visible native card controls keep their identity");
        Check(g_queryRevision==2,"content growth does not automatically request all remaining rows");
        g_appsScroll.ChangeView(nullptr,0.0,nullptr,true);layout();
        Check(g_appsScroll.VerticalOffset()==0,"changing a filter can return the list to its top without animation");
        event.revoke();rows.Children().Clear();originals.clear();
        g_queryRevision=g_shownAppsRevision=100;g_installedVisibleLimit=20;g_installedTotal=100;
        g_currentAppRows.resize(20);for(auto& row:g_currentAppRows){row.installedOnly=true;row.installedTotal=100;}
        viewport=3500;
        auto contentEvent=g_appsScroll.LayoutUpdated(winrt::auto_revoke,[](auto const&,auto const&){CheckInstalledScroll(g_appsScroll);});
        auto sizeEvent=g_appsScroll.SizeChanged(winrt::auto_revoke,[](auto const&,auto const&){CheckInstalledScroll(g_appsScroll);});
        append(20);layout();
        Check(g_queryRevision==101 && g_installedVisibleLimit==60,
              "a tall viewport requests its next batch after native layout without scrolling");
        g_currentAppRows.resize(60);for(auto& row:g_currentAppRows){row.installedOnly=true;row.installedTotal=100;}
        g_shownAppsRevision=g_queryRevision;append(40);layout();
        Check(g_queryRevision==102 && g_installedVisibleLimit==100,
              "completed layout fills remaining tall viewport space after the next batch");
        g_currentAppRows.resize(100);for(auto& row:g_currentAppRows){row.installedOnly=true;row.installedTotal=100;}
        g_shownAppsRevision=g_queryRevision;append(40);layout();
        Check(g_queryRevision==102,"automatic filling stops at the actual inventory end");
        contentEvent.revoke();sizeEvent.revoke();g_currentAppRows.clear();rows.Children().Clear();originals.clear();
        viewport=400;g_queryRevision=g_shownAppsRevision=g_shownFilesRevision=200;
        g_pendingQuery=g_shownQuery=L"ordinary";g_queryDirty=false;g_fileQuery.reset();
        g_appsBatch.Begin(200,20);g_appsBatch.Complete(20,true);g_currentAppRows.resize(20);
        append(20);layout();g_appsScroll.ChangeView(nullptr,0.0,nullptr,true);layout();
        auto appEvent=g_appsScroll.ViewChanged(winrt::auto_revoke,[](auto const& sender,auto const&){CheckInstalledScroll(sender.template as<wuxc::ScrollViewer>());});
        g_appsScroll.ChangeView(nullptr,g_appsScroll.ScrollableHeight(),nullptr,true);
        auto wait=GetTickCount64()+3000;
        while(!g_queryDirty&&GetTickCount64()<wait){layout();Sleep(5);}
        Check(g_queryDirty&&g_appsBatch.requested==60&&g_queryRevision==200&&!g_fileQuery,
              "ordinary app scrolling requests forty more without changing query or querying files");
        CheckInstalledScroll(g_appsScroll);Check(g_appsBatch.requested==60,"ordinary app scroll events coalesce while the batch is pending");
        const auto appOffset=g_appsScroll.VerticalOffset();
        g_currentAppRows.resize(60);g_appsBatch.Complete(60,false);append(40);layout();
        Check(std::abs(g_appsScroll.VerticalOffset()-appOffset)<1,"ordinary app scrolling keeps its offset during append");
        appEvent.revoke();

        g_queryDirty=false;g_resultsList=wuxc::StackPanel();g_filesScroll=wuxc::ScrollViewer();
        g_filesScroll.Content(g_resultsList);source.Content(g_filesScroll);
        auto fileLayout=[&]{
            SetWindowPos(child,nullptr,0,0,360,400,SWP_NOACTIVATE|SWP_NOZORDER|SWP_SHOWWINDOW);
            g_filesScroll.Width(360);g_filesScroll.Height(400);g_filesScroll.ApplyTemplate();
            g_filesScroll.Measure({360,400});g_filesScroll.Arrange({0,0,360,400});g_filesScroll.UpdateLayout();Pump();
        };
        const auto fileRows=[](unsigned count){std::vector<Row> result(count);for(unsigned i=0;i<count;++i){
            result[i].title=L"File "+std::to_wstring(i);result[i].openPath=L"C:\\Files\\"+std::to_wstring(i);
        }return result;};
        auto makeFile=[](const Row& item){wuxc::Button button;button.Height(50);button.Content(winrt::box_value(item.title));return button;};
        g_currentFileRows=fileRows(20);AppendFileCards(g_currentFileRows,0,makeFile);fileLayout();
        const auto firstFiles=*g_fileButtonsOpt;
        UpdateInitialResultCounts();
        Check(g_initialFileResults==InitialResultCount(g_filesScroll.ViewportHeight(),48),
              "real Files viewport supplies its own automatic first batch");
        g_filesBatch.Begin(200,20);g_filesBatch.Complete(20,true);
        g_fileTemplate=FileSearchRequest{200,L"ordinary",true,20,false,{}};
        auto fileEvent=g_filesScroll.ViewChanged(winrt::auto_revoke,[](auto const& sender,auto const&){CheckFileScroll(sender.template as<wuxc::ScrollViewer>());});
        g_filesScroll.ChangeView(nullptr,g_filesScroll.ScrollableHeight(),nullptr,true);
        wait=GetTickCount64()+3000;
        while(!g_fileQuery&&GetTickCount64()<wait){fileLayout();Sleep(5);}
        Check(g_fileQuery&&g_fileQuery->limit==60&&!g_queryDirty&&g_queryRevision==200,
              "ordinary file scrolling requests only its own next forty rows");
        const auto fileOffset=g_filesScroll.VerticalOffset();auto moreFiles=fileRows(60);
        const auto begin=FileAppendStart(g_currentFileRows,moreFiles,200,200,false);
        Check(begin==20,"file append identifies its unchanged rendered prefix");
        AppendFileCards(moreFiles,begin,makeFile);g_currentFileRows=moreFiles;g_filesBatch.Complete(60,false);fileLayout();
        Check(g_fileButtonsOpt->size()==60&&std::abs(g_filesScroll.VerticalOffset()-fileOffset)<1,
              "file batch appends forty controls while retaining native scroll offset");
        for(unsigned i=0;i<20;++i)Check((*g_fileButtonsOpt)[i]==firstFiles[i],
              "appending files preserves existing controls and their event bindings");
        Check(FileAppendStart(g_currentFileRows,moreFiles,200,201,false)==0&&
              FileAppendStart(g_currentFileRows,moreFiles,200,200,true)==0,
              "new queries and forced theme rebuilds cannot reuse a stale file prefix");
        moreFiles[0].openPath=L"Different.exe";
        Check(FileAppendStart(g_currentFileRows,moreFiles,200,200,false)==0,
              "different action identities force a normal file replacement");
        fileEvent.revoke();g_currentFileRows.clear();g_fileButtonsOpt->clear();g_resultsList.Children().Clear();
        g_filesScroll.Content(nullptr);g_filesScroll=nullptr;g_resultsList=nullptr;
        g_currentAppRows.clear();rows.Children().Clear();originals.clear();
        g_appsScroll.Content(nullptr);source.Content(nullptr);g_appsScroll=nullptr;source.Close();source=nullptr;manager.Close();manager=nullptr;DestroyWindow(hwnd);Pump();
        winrt::uninit_apartment();printf("PASS: %d native inventory-scroll checks\n",checks);return 0;
    }catch(winrt::hresult_error const& e){printf("XAML error %08X\n",unsigned(e.code()));return 1;}
}
