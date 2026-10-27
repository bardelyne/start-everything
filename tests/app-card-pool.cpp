#define NOMINMAX
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Text.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
namespace wuxm=wux::Media;
namespace wut=winrt::Windows::UI::Text;
enum class CustomCommandShell { Cmd };
struct SpeedCardUI {};struct UsageCardUI {};
// PRODUCTION_MODEL
// PRODUCTION_CARD
std::optional<std::vector<AppCardUI>> g_activeAppsOpt{std::in_place};
std::optional<std::vector<wuxc::Button>> g_appButtonsOpt{std::in_place};
wuxc::StackPanel g_appsList{nullptr};
winrt::weak_ref<wuxc::Button> g_paintedSelectedButton;
std::mutex g_settingsMutex;
struct {int maxAppResults=20;} g_settings;
std::atomic<uint64_t> g_settingsRevision{1};
bool g_contextMenuOpen=false;
int dismissals=0,inputAttachments=0,menuAttachments=0;
void DismissSearchContextMenu(){++dismissals;g_contextMenuOpen=false;}
constexpr double kIconDisplay=24;
wuxc::TextBlock SearchTextBlock(){return wuxc::TextBlock();}
wuxc::FontIcon SearchFontIcon(){return wuxc::FontIcon();}
template<class T>void ScaleFont(T const& text,double size){text.FontSize(size);}
void ApplyResultRowButtonTheme(wuxc::Button const&,bool){}
void AttachAppCardInput(wuxc::Button const&){++inputAttachments;}
void AttachAppCardMenu(wuxc::Button const& button,std::shared_ptr<Row> const&){++menuAttachments;button.ContextFlyout(wuxc::MenuFlyout());}
namespace i18n {
enum class Key {InstalledApp,NotLaunchable};
const wchar_t* Text(Key key){return key==Key::InstalledApp?L"Installed app":L"No launcher found";}
std::wstring Subtitle(const std::wstring& text){return text;}
}
namespace icons {const std::vector<BYTE>& DefaultExecutableIcon(int){static const std::vector<BYTE> pixels{1,2,3,4};return pixels;}}
int IconPixels(){return 24;}
wuxm::Imaging::BitmapSource IconBitmap(const std::vector<BYTE>& pixels){return pixels.empty()?nullptr:wuxm::Imaging::WriteableBitmap(1,1);}
// PRODUCTION_BADGE
// PRODUCTION_ICON
// PRODUCTION_POOL
int checks=0;
void Check(bool value,const char* label){++checks;if(!value){printf("FAIL: %s\n",label);ExitProcess(1);}}
std::vector<Row> Rows(int count,int generation=0){
    std::vector<Row> rows;
    for(int i=0;i<count;++i){Row row;row.appIndex=i;row.title=L"App "+std::to_wstring(generation)+L"-"+std::to_wstring(i);
        row.openPath=L"C:\\Apps\\"+row.title+L".exe";row.subtitle=row.openPath;row.iconKey=row.title;rows.push_back(std::move(row));}
    return rows;
}
int main(){
    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Isolated app-card pool test",WS_POPUP,0,0,360,700,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        auto source=wux::Hosting::DesktopWindowXamlSource();source.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(hwnd);
        g_appsList=wuxc::StackPanel();source.Content(g_appsList);
        {
            auto rows=Rows(20);
            Check(RenderPlainApps(rows,false),"ordinary app results use the pool");
            Check(inputAttachments==20 && menuAttachments==20,"each initial slot gets one visual and one set of input/menu bindings");
            std::vector<wuxc::Button> buttons;for(auto& slot:*g_plainAppPool)buttons.push_back(slot.card.button);
            const auto layout=buttons[0].Content();const auto image=(*g_plainAppPool)[0].image;

            const auto began=std::chrono::steady_clock::now();
            for(int generation=1;generation<=100;++generation){
                rows=Rows(20,generation);Check(RenderPlainApps(rows,false),"changed results stay in the ordinary pool");
                for(size_t i=0;i<rows.size();++i){
                    const auto& slot=(*g_plainAppPool)[i];
                    Check(slot.card.button==buttons[i] && slot.name.Text()==rows[i].title && slot.subtitle.Text()==rows[i].subtitle,"changed text keeps the same button and visual tree");
                    Check(slot.binding->openPath==rows[i].openPath && (*g_activeAppsOpt)[i].title==rows[i].title && (*g_appButtonsOpt)[i]==buttons[i],"menu targets and keyboard/click indices follow current rows");
                }
            }
            const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count();
            Check(inputAttachments==20,"repeated typing creates no new ordinary cards or event handlers");
            Check(buttons[0].Content()==layout && (*g_plainAppPool)[0].image==image,"card layout and icon controls are reused");
            printf("100 x 20-row pool updates: %.2f ms total (hidden test, excludes presentation)\n",ms);
            auto small=Rows(2,101);RenderPlainApps(small,false);
            Check(g_appsList.Children().Size()==20 && g_activeAppsOpt->size()==2,"fewer matches hide slots without removing them from the tree");
            Check(buttons[2].Visibility()==wux::Visibility::Collapsed && buttons[0].Visibility()==wux::Visibility::Visible,"hidden slots cannot receive pointer input");
            auto& slot=(*g_plainAppPool)[0];
            auto menu=slot.card.button.ContextFlyout().as<wuxc::MenuFlyout>();menu.Items().Append(wuxc::MenuFlyoutItem());
            g_contextMenuOpen=true;UpdatePlainAppCard(slot,small[0]);
            Check(dismissals==0 && menu.Items().Size()==1 && g_contextMenuOpen,"an unchanged visible menu survives an unrelated update");
            g_contextMenuOpen=true;
            Row installed=small[0];installed.title=L"Runtime";installed.installedOnly=true;installed.isSetting=false;
            installed.installedLocation=L"C:\\Runtime";installed.installedKey=L"precise-key";installed.cardSignature=L"installed:identity";
            UpdatePlainAppCard(slot,installed);
            Check(dismissals==1 && menu.Items().Size()==0 && slot.binding->installedKey==L"precise-key","rebinding closes an old menu and removes its old actions");
            Check(slot.badge.Visibility()==wux::Visibility::Visible && slot.name.TextWrapping()==wux::TextWrapping::Wrap,"installed-only rows retain the nonlaunchable badge and wrapped title");
            Row setting=small[0];setting.isSetting=true;setting.openPath=L"ms-settings:display";setting.subtitle=L"";
            UpdatePlainAppCard(slot,setting);
            Check(slot.image.Visibility()==wux::Visibility::Collapsed && slot.fallback.Glyph()==L"\uE713" && slot.badge.Visibility()==wux::Visibility::Collapsed,"Settings rebinding clears the old application image and installed badge");
            Check(slot.subtitle.Visibility()==wux::Visibility::Collapsed && !slot.binding->installedOnly,"empty subtitles and previous result metadata are cleared");
            ApplyAppIcon(slot.card.button,{1,2,3,4});
            Check(slot.image.Source() && slot.image.Visibility()==wux::Visibility::Visible && slot.fallback.Visibility()==wux::Visibility::Collapsed,"late icons update the nested image without replacing the slot host");
            UpdatePlainAppCard(slot,small[0]);
            Check(slot.image.Source() && slot.badge.Visibility()==wux::Visibility::Collapsed && slot.name.MaxLines()==1,"ordinary apps recover their default executable icon and single-line layout");
            auto rich=small;rich[0].speed=SpeedView{};
            Check(!RenderPlainApps(rich,false),"rich utility cards use their separate layout");
            DetachPlainAppPool();Check(!g_plainPoolAttached && !g_appsList.Children().Size() && g_activeAppsOpt->empty(),"leaving ordinary results detaches the pool and clears selection targets");
            RenderPlainApps(small,false);Check((*g_appButtonsOpt)[0]==buttons[0],"returning from a rich card reuses the original ordinary slots");
            Check(!RenderPlainApps({},false) && !RenderPlainApps(Rows(21),false),"empty and oversized results use the existing fallback renderer");
            g_settings.maxAppResults=4;++g_settingsRevision;
            RenderPlainApps(Rows(4),true);
            Check(g_plainAppPool->size()==4 && g_appsList.Children().Size()==4 && (*g_appButtonsOpt)[0]!=buttons[0],"settings changes rebuild a pool bounded by the new limit");
        }
        DetachPlainAppPool();g_plainAppPool.reset();g_activeAppsOpt.reset();g_appButtonsOpt.reset();g_appsList=nullptr;g_paintedSelectedButton={};
        source.Content(nullptr);source.Close();source=nullptr;manager.Close();manager=nullptr;DestroyWindow(hwnd);
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        winrt::uninit_apartment();printf("PASS: %d reusable app-card checks\n",checks);return 0;
    }catch(winrt::hresult_error const& error){printf("XAML error %08X\n",unsigned(error.code()));return 1;}
}
