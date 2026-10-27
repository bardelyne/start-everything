#define NOMINMAX
#include <windows.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cmath>
#include <memory>
#include <optional>
#include <array>
#include <algorithm>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
namespace wuxm=wux::Media;
namespace wut=winrt::Windows::UI::Text;
wuxc::TextBlock SearchTextBlock(){return wuxc::TextBlock();}
wuxc::FontIcon SearchFontIcon(){return wuxc::FontIcon();}
void ScaleFont(wuxc::FontIcon const& icon,double size){icon.FontSize(size);}
void ScaleFont(wuxc::TextBlock const& text,double size){text.FontSize(size);}
wuxm::SolidColorBrush MakeBrush(BYTE a,BYTE r,BYTE g,BYTE b){return wuxm::SolidColorBrush(winrt::Windows::UI::Color{a,r,g,b});}
std::vector<std::shared_ptr<void>> handlers;
template<class T,class R> void KeepHandler(const T&,R revoke){handlers.push_back(std::make_shared<R>(std::move(revoke)));}
void StartInternetSpeedTest(){}
void StopInternetSpeedTest(){}
namespace i18n {
enum class Key{TestReady,Latency,Download,Upload,TestComplete,TestStopped,Error,StopTest,Run,NotLaunchable,InstalledApp};
const wchar_t* Text(Key key){const wchar_t* names[]={L"Ready",L"Latency",L"Download",L"Upload",L"Complete",L"Stopped",L"Error",L"Stop",L"Run",L"No launcher found",L"Installed app"};return names[static_cast<int>(key)];}
}
wuxm::Brush g_searchTextBrush{nullptr};
bool animateSpeedMeter=false;
bool StartSpeedMeterAnimationTimer(){return animateSpeedMeter;}
// PRODUCTION_MODEL
// PRODUCTION_SPEED_UI
// PRODUCTION_TIMELINE
int checks=0;
void Check(bool good,const char* label){if(!good){printf("FAIL %s\n",label);ExitProcess(1);}++checks;}
int main(){
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        HWND hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Isolated weather card layout test",WS_POPUP,0,0,400,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        auto source=wux::Hosting::DesktopWindowXamlSource();
        source.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(hwnd);
        g_searchTextBrush=MakeBrush(255,255,255,255);
        {
            std::vector<WeatherDayRow> days;
            for(int i=0;i<8;++i) days.push_back({i==2?L"Today":L"Wednesday 28",L"\u2600",L"\u2193 19\u00B0",L"\u2191 31\u00B0",i==2,i<2});
            for(float width:{108.f,180.f,260.f}){
                auto timeline=BuildWeatherTimeline(days);
                auto root=wuxc::Grid();root.Width(width);root.Children().Append(timeline);source.Content(root);
                root.Measure({width,1000});root.Arrange({0,0,width,1000});root.UpdateLayout();
                Check(timeline.Children().Size()==8,"all forecast days share one timeline");
                Check(timeline.DesiredSize().Width<=width+1,"timeline fits narrow app panel");
                auto today=timeline.Children().GetAt(2).as<wuxc::Border>();
                auto past=timeline.Children().GetAt(0).as<wuxc::Border>();
                Check(today.Background() && today.BorderBrush(),"today receives accent fill and border");
                Check(today.Opacity()>past.Opacity(),"past days are visually distinct");
                auto line=today.Child().as<wuxc::Grid>();
                Check(line.Children().GetAt(0).as<wuxc::TextBlock>().Text()==L"Today","today label retained");
                Check(line.Children().GetAt(0).as<wuxc::TextBlock>().FontWeight().Weight==wut::FontWeights::SemiBold().Weight,"today uses stronger text");
                Check(timeline.DesiredSize().Height>120 && timeline.DesiredSize().Height<400,"forecast has bounded card height");
                source.Content(nullptr);
            }
            for(float width:{180.f,260.f}){
                auto badge=BuildInstalledBadge();badge.Measure({width,100});
                Check(badge.DesiredSize().Width<=width && badge.DesiredSize().Height<40,"installed-only marker wraps within a narrow app card");
                Check(badge.Children().Size()==2 && badge.Children().GetAt(0).as<wuxc::FontIcon>().Glyph()==L"\uE946" && badge.Children().GetAt(1).as<wuxc::TextBlock>().Text()==L"Installed app \u00B7 No launcher found","installed-only results show an explicit app category and launchability label");
                auto root=wuxc::Grid();root.Width(width);
                auto ui=BuildSpeedCard(SpeedView{});root.Children().Append(ui.root);source.Content(root);
                auto arc=ui.arc;
                for(auto phase:{SpeedView::Phase::Ready,SpeedView::Phase::Latency,SpeedView::Phase::Download,SpeedView::Phase::Upload,SpeedView::Phase::Done,SpeedView::Phase::Stopped}){
                    SpeedView value;value.phase=phase;value.progress=phase==SpeedView::Phase::Done?100:phase==SpeedView::Phase::Ready?0:40;
                    value.latency=12.3;value.download=80.1;value.upload=20.2;UpdateSpeedCard(ui,value);
                    root.Measure({width,1000});root.Arrange({0,0,width,1000});root.UpdateLayout();
                    Check(ui.root.DesiredSize().Width<=width+1,"speed card fits normal app panel");
                    Check(ui.root.DesiredSize().Height>150 && ui.root.DesiredSize().Height<400,"speed card has bounded height");
                    Check(ui.arc==arc && ui.root.Children().Size()==4,"progress updates reuse gauge controls");
                    Check(std::wstring(ui.latency.Text()).find(L"12.3")!=std::wstring::npos,"latency is displayed");
                    const bool running=phase==SpeedView::Phase::Latency||phase==SpeedView::Phase::Download||phase==SpeedView::Phase::Upload;
                    Check(winrt::unbox_value<bool>(ui.action.Tag())==running,"action switches between run and stop");
                }
                SpeedView measured;measured.phase=SpeedView::Phase::Download;measured.download=80.1;measured.progress=10;
                UpdateSpeedCard(ui,measured);
                Check(ui.reading.Text()==L"80.1" && ui.unit.Text()==L"\u2193 Mbps","meter shows download speed in Mbps");
                const auto endpoint=[&]{return ui.arc.Data().as<wuxm::PathGeometry>().Figures().GetAt(0).Segments().GetAt(0).as<wuxm::ArcSegment>().Point();};
                const auto before=endpoint();measured.progress=99;UpdateSpeedCard(ui,measured);
                Check(endpoint().X==before.X && endpoint().Y==before.Y,"completion progress cannot move the speed meter");
                measured.download=40;UpdateSpeedCard(ui,measured);
                Check(ui.reading.Text()==L"40.0" && (endpoint().X!=before.X || endpoint().Y!=before.Y),"meter falls when speed falls");
                const auto scale=ui.meterMaximum;
                measured.phase=SpeedView::Phase::Upload;measured.upload=-1;UpdateSpeedCard(ui,measured);
                Check(ui.arc.Visibility()==wux::Visibility::Collapsed && ui.reading.Text()==L"\u2014","upload starts with an empty meter rather than the download reading");
                measured.upload=20.2;UpdateSpeedCard(ui,measured);
                Check(ui.reading.Text()==L"20.2" && ui.unit.Text()==L"\u2191 Mbps" && ui.meterMaximum==scale,"upload uses its speed on the same scale");
                Check(std::wstring(ui.download.Text()).find(L"40.0")!=std::wstring::npos,"completed download remains visible during upload");
                measured.phase=SpeedView::Phase::Done;measured.progress=100;UpdateSpeedCard(ui,measured);
                Check(ui.reading.Text()==L"20.2" && !ui.arc.Data().as<wuxm::PathGeometry>().Figures().GetAt(0).Segments().GetAt(0).as<wuxm::ArcSegment>().IsLargeArc(),"completion keeps the measured upload instead of filling to 100 percent");
                measured=SpeedView{};measured.phase=SpeedView::Phase::Latency;UpdateSpeedCard(ui,measured);
                Check(ui.meterMaximum==1000 && ui.arc.Visibility()==wux::Visibility::Collapsed && ui.unit.Text()==L"ms","a new run resets the meter and shows latency units");
                Check(ui.ticks.front().Text()==L"0" && ui.ticks.back().Text()==L"1000","speedometer shows the reference scale labels");
                Check(ui.needle && ui.needleRotation.Angle()==-135,"needle rests at the start of the open arc");
                measured.phase=SpeedView::Phase::Download;measured.download=100;UpdateSpeedCard(ui,measured);
                Check(std::abs(ui.needleRotation.Angle())<0.001,"100 Mbps points straight up on the reference scale");
                measured.download=2500;UpdateSpeedCard(ui,measured);
                Check(ui.meterMaximum==10000 && ui.ticks.back().Text()==L"10000","very fast connections expand the labeled scale");
                UpdateSpeedCard(ui,SpeedView{});animateSpeedMeter=true;
                measured=SpeedView{};measured.phase=SpeedView::Phase::Download;measured.download=100;
                const auto geometry=ui.arc.Data();const auto segment=ui.meterArc;
                UpdateSpeedCard(ui,measured);const auto began=ui.meterStarted;
                Check(ui.meterAnimating && ui.meterSpeed==0 && ui.needleRotation.Angle()==-135,"a new speed eases out from the displayed value rather than jumping");
                AdvanceSpeedMeter(ui,began+125);
                Check(ui.meterSpeed>0 && ui.meterSpeed<100 && ui.reading.Text()==L"87.5","the numeric reading animates with the gauge");
                Check(std::abs(ui.needleRotation.Angle()-(-135+270*SpeedMeterFraction(ui.meterSpeed,ui.meterMaximum)))<0.001,"needle and animated reading agree on the nonlinear scale");
                const auto point=SpeedMeterPoint(ui.meterFraction,118),drawn=ui.meterArc.Point();
                Check(std::abs(point.X-drawn.X)<0.001 && std::abs(point.Y-drawn.Y)<0.001,"colored arc follows the needle on each animation frame");
                Check(ui.arc.Data()==geometry && ui.meterArc==segment,"animation frames reuse the path geometry and arc segment");
                // Pause the logical clock at a partial frame, then retarget in
                // both directions as a new network sample would.
                ui.meterStarted=GetTickCount64();ui.meterFromSpeed=ui.meterSpeed;ui.meterFromFraction=ui.meterFraction;
                const auto displayed=ui.meterSpeed;
                measured.download=20;UpdateSpeedCard(ui,measured);const auto fall=ui.meterStarted;
                Check(ui.meterAnimating && ui.meterSpeed>=displayed-0.01 && ui.meterSpeed<100,"an interrupted animation retargets from the currently displayed speed");
                const auto fallingFrom=ui.meterSpeed;AdvanceSpeedMeter(ui,fall+125);
                Check(ui.meterSpeed>20 && ui.meterSpeed<fallingFrom,"the meter smoothly falls as speed decreases");
                AdvanceSpeedMeter(ui,fall+250);
                Check(!ui.meterAnimating && ui.reading.Text()==L"20.0" && ui.meterSpeed==20,"animation settles on the exact measured value");
                measured.download=80;UpdateSpeedCard(ui,measured);const auto repeated=ui.meterStarted;
                measured.progress=90;UpdateSpeedCard(ui,measured);
                Check(ui.meterStarted==repeated,"completion-only updates cannot continually restart the animation");
                measured.phase=SpeedView::Phase::Upload;measured.upload=-1;UpdateSpeedCard(ui,measured);
                AdvanceSpeedMeter(ui,ui.meterStarted+250);
                Check(ui.arc.Visibility()==wux::Visibility::Collapsed && ui.reading.Text()==L"\u2014","upload transition clears the previous direction after easing to zero");
                measured.upload=35;UpdateSpeedCard(ui,measured);FinishSpeedMeterAnimation(ui);
                Check(!ui.meterAnimating && ui.meterSpeed==35,"closing or reducing motion can finish and stop animation immediately");
                measured.phase=SpeedView::Phase::Done;UpdateSpeedCard(ui,measured);
                Check(!ui.meterAnimating && ui.reading.Text()==L"35.0","unchanged final results keep the animation idle");
                animateSpeedMeter=false;measured.upload=60;UpdateSpeedCard(ui,measured);
                Check(!ui.meterAnimating && ui.reading.Text()==L"60.0","disabled animations use the exact reading without a frame timer");
                animateSpeedMeter=true;const auto scaleAngle=ui.needleRotation.Angle();
                measured.upload=2500;UpdateSpeedCard(ui,measured);
                Check(ui.meterMaximum==10000 && std::abs(ui.needleRotation.Angle()-scaleAngle)<0.001,"expanding the scale does not snap the needle to another position");
                AdvanceSpeedMeter(ui,ui.meterStarted+250);
                Check(ui.reading.Text()==L"2500" && std::abs(ui.needleRotation.Angle()-(-135+270*SpeedMeterFraction(2500,10000)))<0.001,"an expanded scale settles on the measured value and matching needle position");
                animateSpeedMeter=false;
                source.Content(nullptr);
            }
            for(float width:{108.f,180.f,260.f}){
                UsageView value;value.display=L"57.3%";value.detail=L"12.5 / 32.0 GB";value.maximum=100;value.color=0xFF4CB9EF;
                auto root=wuxc::Grid();root.Width(width);
                auto ui=BuildUsageCard(value);root.Children().Append(ui.root);source.Content(root);
                auto line=ui.line;
                for(int sample=0;sample<65;++sample){
                    if(value.history.size()==60)value.history.erase(value.history.begin());
                    value.history.push_back(sample==20?-1:sample%100);
                    UpdateUsageCard(ui,value);root.Measure({width,1000});root.Arrange({0,0,width,1000});root.UpdateLayout();
                }
                Check(ui.root.DesiredSize().Width<=width+1,"usage graph fits narrow and normal app panels");
                Check(ui.root.DesiredSize().Height>90 && ui.root.DesiredSize().Height<210,"usage graph card has bounded height");
                Check(ui.line==line && ui.root.Children().Size()==4,"one-second updates reuse resource controls");
                Check(ui.value.Text()==L"57.3%" && ui.detail.Text()==L"12.5 / 32.0 GB","live resource labels retain measurements");
                Check(ui.line.Data().as<wuxm::PathGeometry>().Figures().Size()==2,"missing counter samples break the graph instead of drawing fake zeros");
                source.Content(nullptr);
            }
        }
        handlers.clear();g_searchTextBrush=nullptr;
        source.Close();source=nullptr;manager.Close();manager=nullptr;DestroyWindow(hwnd);
        MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        winrt::uninit_apartment();
        printf("PASS: %d weather layout checks\n",checks);return 0;
    }catch(winrt::hresult_error const& error){printf("XAML error %08X\n",unsigned(error.code()));return 1;}
}
