#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <dxgi.h>
#include <roapi.h>
#include <winrt/base.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cwctype>
#include <map>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
namespace tools {
std::wstring ToLower(std::wstring value){for(auto& c:value)c=towlower(c);return value;}
std::wstring Trim(std::wstring value){auto a=value.find_first_not_of(L" \t\r\n"),b=value.find_last_not_of(L" \t\r\n");return a==std::wstring::npos?L"":value.substr(a,b-a+1);}
}
namespace i18n {
enum class Key{HardwareUsage,MemoryUsage,DiskUsage,NetworkTraffic,MetricsUnavailable,OnlineLoading};
const wchar_t* Text(Key key){const wchar_t* values[]={L"Hardware usage",L"Memory",L"Disk",L"Network",L"Unavailable",L"Loading"};return values[static_cast<int>(key)];}
}
enum class CustomCommandShell{Cmd};
// PRODUCTION_MODEL
std::atomic<bool> g_searchQuit{false};
std::mutex g_queryMutex,observedMutex;
std::condition_variable observed;
uint64_t g_queryRevision=1;
std::wstring g_pendingQuery=L"/usage";
std::vector<Row> lastRows;
int publications=0;
std::vector<std::chrono::steady_clock::time_point> timestamps;
bool PublishAppResults(uint64_t revision,const std::wstring& query,std::vector<Row> rows){
 std::lock_guard lock(g_queryMutex);if(g_searchQuit || revision!=g_queryRevision || query!=g_pendingQuery)return false;
 {std::lock_guard guard(observedMutex);lastRows=std::move(rows);++publications;timestamps.push_back(std::chrono::steady_clock::now());}
 observed.notify_all();return true;
}
void RequestRender(){}
void Wh_Log(const wchar_t*,...){}
// PRODUCTION_HARDWARE
int checks=0;
void Check(bool value,const char* label){if(!value){std::fprintf(stderr,"FAIL: %s\n",label);ExitProcess(1);}++checks;}
int main(int argc,char** argv){
 const auto apartment=RoInitialize(RO_INIT_MULTITHREADED);
 bool cpuCounter=false,gpuCounter=false,vramCounter=false;
 {hardware::Counters diagnostic;cpuCounter=diagnostic.cpu!=nullptr;gpuCounter=diagnostic.gpu!=nullptr;vramCounter=diagnostic.vram!=nullptr;
  std::printf("Native PDH: CPU=%d disk=%d GPU=%d VRAM=%d\n",cpuCounter,diagnostic.idle!=nullptr,gpuCounter,vramCounter);}
 if(argc>1 && std::string(argv[1])=="--require-counters")Check(cpuCounter && gpuCounter && vramCounter,"real CPU, GPU and adapter-memory counter providers are available");
 std::vector<double> history;
 for(int i=0;i<100;++i)hardware::Append(history,i);
 Check(history.size()==60 && history.front()==40 && history.back()==99,"history keeps exactly the last 60 samples");
 hardware::Append(history,std::numeric_limits<double>::infinity());
 Check(history.size()==60 && history.back()==-1,"invalid samples remain gaps instead of fake zeroes");
 Check(hardware::Percent(150)==100 && hardware::Percent(std::nan(""))==-1,"percentages are finite and bounded");
 const auto gpu=hardware::GpuUsage({
  {L"pid_1_luid_0x00000000_0x00000001_phys_0_eng_0_engtype_3D",40},
  {L"pid_2_luid_0x00000000_0x00000001_phys_0_eng_0_engtype_3D",35},
  {L"pid_3_luid_0x00000000_0x00000001_phys_0_eng_1_engtype_Copy",80},
  {L"pid_4_luid_0x00000000_0x00000002_phys_0_eng_0_engtype_3D",22},
  {L"luid_bad_eng_0_engtype_3D",30},{L"invalid",99},
  {L"pid_5_luid_0x00000000_0x00000002_phys_0_eng_0_engtype_3D",std::nan("")}});
 Check(gpu.size()==2 && gpu.at(L"0x00000000_0x00000001")==80,"GPU aggregates processes by engine and chooses busiest engine");
 Check(gpu.at(L"0x00000000_0x00000002")==22,"separate GPUs retain separate readings");
 Check(hardware::GpuUsage({{L"pid_1_luid_0x0_0x1_phys_0_eng_0_engtype_3D",150}}).at(L"0x0_0x1")==100,"overlapping GPU process counters are bounded");
 const double gib=1073741824.0;
 const auto memory=hardware::GpuMemory({
  {L"luid_0x00000000_0x00000001_phys_0",gib},
  {L"luid_0x00000000_0x00000001_phys_0#1",gib},
  {L"luid_0x0_0x1_phys_1",gib/2},
  {L"LUID_0x00000000_0x00000002_PHYS_0",gib/4},
  {L"pid_1_luid_0x00000000_0x00000001_phys_0",gib*20},
  {L"luid_bad_phys_0",99},{L"luid_0x0_0x3_phys_0_extra",99},
  {L"luid_0x0_0x4_phys_0",-1},{L"luid_0x0_0x5_phys_0",std::nan("")}});
 Check(memory.size()==2 && memory.at(L"0x00000000_0x00000001")==gib*1.5,"dedicated memory adds physical nodes without double-counting repeated instances or process counters");
 Check(memory.at(L"0x00000000_0x00000002")==gib/4,"VRAM remains separate for each adapter and accepts case-insensitive counter names");
 Check(hardware::GpuMemory({{L"luid_0x0_0x1_phys_0",0}}).at(L"0x00000000_0x00000001")==0,"zero dedicated memory usage is a valid reading");
 const auto huge=hardware::GpuMemory({{L"luid_0x0_0x1_phys_0",std::numeric_limits<double>::max()},{L"luid_0x0_0x1_phys_1",std::numeric_limits<double>::max()}});
 Check(huge.at(L"0x00000000_0x00000001")==-1,"overflowing adapter totals become unavailable");
 Check(hardware::VramText(gib*1.5,8ull<<30)==L"VRAM 1.5 / 8.0 GB","GPU detail shows used and total dedicated memory");
 Check(hardware::VramText(-1,8ull<<30)==L"VRAM \u2014 / 8.0 GB","missing readings never display false zero usage");
 Check(hardware::VramText(0,8ull<<30)==L"VRAM 0.0 / 8.0 GB","zero usage retains its known capacity");
 Check(hardware::VramText(32ull<<20,128ull<<20)==L"VRAM 32.0 / 128.0 MB","small integrated-GPU allocations use readable megabytes");
 Check(hardware::VramText(-1,0)==L"VRAM \u2014" && hardware::VramText(gib,0)==L"VRAM 1.0 GB","unknown dedicated capacity does not invent a total or division by zero");
 Check(hardware::Initial(L"/usage",true).front().infoOnly,"initial card cannot launch an application");
 Check(hardware::Initial(L"/usage",true).front().customIconText.empty() && hardware::Initial(L"/usage",true).front().customGlyph==L"\uE9D2","initial hardware card uses an icon instead of an emoji");
 hardware::Visible(true);hardware::Schedule(L"/usage",1);
 {
  std::unique_lock lock(observedMutex);
  Check(observed.wait_for(lock,10s,[]{return publications>=3;}),"native monitor produces initial and one-second updates");
  Check(lastRows.size()>=5,"CPU RAM GPU disk and network each receive cards");
  std::set<std::wstring> ids;bool cpu=false,ram=false,gpuCard=false,measuredGpu=false,measuredVram=false,network=false,disk=false;
  for(const auto& row:lastRows){
   Check(row.usage.has_value() && ids.insert(row.usage->id).second,"every metric card has a unique stable identity");
    const auto& value=*row.usage;
   Check(value.history.size()>=2 && value.history.size()<=60,"native readings have bounded history");
   Check(std::isfinite(value.value) && std::isfinite(value.maximum) && value.maximum>0,"native graph values and limits are finite");
   Check(row.sectionEmoji.empty() && row.customIconIsGlyph && row.customIconText.size()==1 &&
       row.customIconText.front()>=0xE700 && row.customIconText.front()<=0xF8FF,"each hardware card and its section use monochrome font icons");
   if(value.id==L"cpu"){cpu=true;Check(cpuCounter?(value.value>=0 && value.value<=100):value.value==-1,"CPU is measured or honestly marked unavailable");}
   if(value.id==L"ram"){ram=true;Check(value.value>0 && value.value<=100,"native physical memory counter is usable");}
   if(value.id.starts_with(L"gpu")){gpuCard=true;measuredGpu|=value.value>=0;
       Check(value.detail.find(L"VRAM ")!=value.detail.npos,"every detected GPU includes a VRAM reading or an explicit unavailable marker");
       measuredVram|=value.detail.find(L"VRAM \u2014")==value.detail.npos;
       std::wprintf(L"GPU memory: %ls - %ls\n",row.subtitle.c_str(),value.detail.c_str());}
   network|=value.id==L"network";disk|=value.id==L"disk";
  }
  Check(cpu&&ram&&gpuCard&&network&&disk,"all requested resource families are present");
  if(argc>1 && std::string(argv[1])=="--require-counters")Check(measuredGpu,"native GPU engine readings map to the detected adapters");
  if(argc>1 && std::string(argv[1])=="--require-counters")Check(measuredVram,"native adapter-memory readings map to a detected GPU");
  const double interval=std::chrono::duration<double>(timestamps[2]-timestamps[1]).count();
  Check(interval>=0.9 && interval<1.8,"sampling cadence is approximately one second");
 }
 hardware::Visible(false);std::this_thread::sleep_for(200ms);
 int hidden;{std::lock_guard lock(observedMutex);hidden=publications;}
 std::this_thread::sleep_for(1200ms);
 {std::lock_guard lock(observedMutex);Check(publications==hidden,"hidden search stops monitoring and publications");}
 hardware::Visible(true);
 {std::unique_lock lock(observedMutex);Check(observed.wait_for(lock,6s,[&]{return publications>hidden;}),"visible usage command resumes with fresh baselines");}
 {std::lock_guard lock(g_queryMutex);++g_queryRevision;g_pendingQuery=L"Alpha";}hardware::Wake();
 std::this_thread::sleep_for(200ms);int left;{std::lock_guard lock(observedMutex);left=publications;}
 std::this_thread::sleep_for(1200ms);
 {std::lock_guard lock(observedMutex);Check(publications==left,"leaving the command cancels further sampling");}
 {std::lock_guard lock(g_queryMutex);g_searchQuit=true;}hardware::Wake();hardware::g_thread->join();hardware::g_thread.reset();
 Check(!hardware::g_thread,"monitor thread joins cleanly at shutdown");
 if(SUCCEEDED(apartment))RoUninitialize();std::printf("PASS: %d hardware card checks\n",checks);
}
