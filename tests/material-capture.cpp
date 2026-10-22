#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <d2d1.h>
#include <d2d1effects.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <array>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <future>
#include <winrt/Windows.System.h>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>
#include <cstring>
#include <type_traits>
namespace wf=winrt::Windows::Foundation;
namespace wux=winrt::Windows::UI::Xaml;
namespace wuxc=wux::Controls;
namespace wuxm=wux::Media;
static std::atomic<bool> g_quit{false};
static int checks=0,failures=0;
#define Wh_Log(...) ((void)0)
bool Wh_SetFunctionHook(void* target,void*,void** original){*original=target;return true;}
namespace WindhawkUtils { template<class Prototype> bool SetFunctionHook(Prototype* target, Prototype* hook, Prototype** original) { return Wh_SetFunctionHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(hook),reinterpret_cast<void**>(original)); } }

static std::unordered_map<std::wstring,std::vector<unsigned char>> testStorage;
static int storageReads=0,storageWrites=0;static bool storageWritable=true;
size_t Wh_GetBinaryValue(PCWSTR key,void* data,size_t capacity){++storageReads;auto found=testStorage.find(key);if(found==testStorage.end()||found->second.size()>capacity)return 0;std::memcpy(data,found->second.data(),found->second.size());return found->second.size();}
BOOL Wh_SetBinaryValue(PCWSTR key,void const* data,size_t size){++storageWrites;if(!storageWritable)return FALSE;auto bytes=static_cast<unsigned char const*>(data);testStorage[key]=std::vector<unsigned char>(bytes,bytes+size);return TRUE;}
BOOL Wh_DeleteValue(PCWSTR key){return testStorage.erase(key)!=0;}

namespace {
// PRODUCTION
static_assert(std::is_trivially_destructible_v<SearchCapturedMaterial>,"factory and brush entries must never own COM resources");
static_assert(std::is_trivially_copyable_v<SearchSavedMaterials>,"saved recovery data must never own COM objects or pointers");
struct TestCachedIdentity : winrt::implements<TestCachedIdentity,wf::IInspectable> {};
struct ThrowingPropertyNames : winrt::implements<ThrowingPropertyNames,wf::Collections::IIterable<winrt::hstring>> {
 wf::Collections::IIterator<winrt::hstring> First(){throw winrt::hresult_error(E_FAIL);}
};
static wf::IUnknown g_testCreationResult{nullptr};
HRESULT TestCreationResult(void** result){if(!result)return E_POINTER;*result=winrt::get_abi(g_testCreationResult);static_cast<IUnknown*>(*result)->AddRef();return S_OK;}
HRESULT WINAPI TestCreateFactory(void*,void*,void** result){return TestCreationResult(result);}
HRESULT WINAPI TestCreateFactoryWithProperties(void*,void*,void*,void** result){return TestCreationResult(result);}
HRESULT WINAPI TestCreateBrush(void*,void** result){return TestCreationResult(result);}
struct HookTable {
 std::vector<std::pair<void**,void*>> entries;
 void Add(void* object,size_t slot,void* replacement){auto table=*reinterpret_cast<void***>(object);auto at=&table[slot];DWORD old=0;if(!VirtualProtect(at,sizeof(void*),PAGE_READWRITE,&old))throw winrt::hresult_error(HRESULT_FROM_WIN32(GetLastError()));entries.emplace_back(at,*at);*at=replacement;DWORD ignored;VirtualProtect(at,sizeof(void*),old,&ignored);}
 ~HookTable(){for(auto it=entries.rbegin();it!=entries.rend();++it){DWORD old=0;VirtualProtect(it->first,sizeof(void*),PAGE_READWRITE,&old);*it->first=it->second;DWORD ignored;VirtualProtect(it->first,sizeof(void*),old,&ignored);}}
};
struct TestMaterialBrush : wuxm::XamlCompositionBrushBaseT<TestMaterialBrush> {
 void Attach(searchcomp::CompositionBrush const& value){CompositionBrush(value);}
 wf::IInspectable Inner()const{return this->m_inner;}
};
struct TestWarmMaterialBrush : wuxm::XamlCompositionBrushBaseT<TestWarmMaterialBrush> {
 using Base=wuxm::XamlCompositionBrushBaseT<TestWarmMaterialBrush>;
 searchcomp::Compositor compositor;
 float radius=43.f;int connects=0,disconnects=0;bool hideProtected=true;
 int32_t __stdcall QueryInterface(winrt::guid const& id,void** out)noexcept{
  if(hideProtected&&id==winrt::guid_of<wuxm::IXamlCompositionBrushBaseProtected>()){*out=nullptr;return E_NOINTERFACE;}
  return Base::QueryInterface(id,out);
 }
 wf::IInspectable Inner()const{return this->m_inner;}
 searchcomp::CompositionBrush Current()const{return CompositionBrush();}
 void OnConnected(){
  ++connects;
  if(CompositionBrush())return;
  auto effect=winrt::make_self<SearchEffect>(CLSID_D2D1GaussianBlur);
  effect->properties={wf::PropertyValue::CreateSingle(radius),wf::PropertyValue::CreateUInt32(1),wf::PropertyValue::CreateUInt32(1)};
  effect->sources={searchcomp::CompositionEffectSourceParameter(L"WarmBackdrop")};
  auto factory=compositor.CreateEffectFactory(*effect);auto brush=factory.CreateBrush();
  brush.SetSourceParameter(L"WarmBackdrop",compositor.CreateBackdropBrush());CompositionBrush(brush);
 }
 void OnDisconnected(){++disconnects;if(auto brush=CompositionBrush())brush.Close();CompositionBrush(nullptr);}
};
void Check(bool value,const char* name){++checks;if(!value)++failures;std::printf("%s %s\n",value?"PASS":"FAIL",name);}
}
int Run()try{
 std::setvbuf(stdout,nullptr,_IONBF,0);

 for (int pass=0;pass<8;++pass) {
  InitSearchMaterialCapture();
  Check(g_searchMaterialCapture,"cold initialization registers all material hooks");
  Check(std::all_of(g_searchMaterialModules.begin(),g_searchMaterialModules.end(),[](auto module){return module!=nullptr;}),"all hook-target libraries are held after discovery thread exits");
  for(auto target:{reinterpret_cast<void*>(g_searchCreateFactory),reinterpret_cast<void*>(g_searchCreateFactoryWithProperties),reinterpret_cast<void*>(g_searchCreateEffectBrush),reinterpret_cast<void*>(g_searchCreateXamlBrush)}){
   MEMORY_BASIC_INFORMATION memory{};Check(VirtualQuery(target,&memory,sizeof(memory))&&memory.State==MEM_COMMIT&&(memory.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)),"pending hook points to loaded executable code before hook application");
  }
  StopSearchMaterialCapture();
  Check(std::all_of(g_searchMaterialModules.begin(),g_searchMaterialModules.end(),[](auto module){return module==nullptr;}),"shutdown releases target-library references after hook removal");
 }
 InitSearchMaterialCapture();
 Check(g_searchMaterialCapture,"public factory entry points initialize without a UI or private offsets");
 searchcomp::Compositor compositor;
 HookTable testHooks;
 testHooks.Add(winrt::get_abi(compositor),11,reinterpret_cast<void*>(SearchCreateFactoryHook));
 testHooks.Add(winrt::get_abi(compositor),12,reinterpret_cast<void*>(SearchCreateFactoryWithPropertiesHook));
 auto xamlFactory=winrt::get_activation_factory<wuxm::XamlCompositionBrushBase,wuxm::IXamlCompositionBrushBaseFactory>();
 testHooks.Add(winrt::get_abi(xamlFactory),6,reinterpret_cast<void*>(SearchCreateXamlBrushHook));
 auto blur=winrt::make_self<SearchEffect>(CLSID_D2D1GaussianBlur);
 blur->properties={wf::PropertyValue::CreateSingle(37.f),wf::PropertyValue::CreateUInt32(1),wf::PropertyValue::CreateUInt32(1)};
 blur->sources={searchcomp::CompositionEffectSourceParameter(L"arbitraryBackdropName")};
 auto matrix=winrt::make_self<SearchEffect>(CLSID_D2D1ColorMatrix);
 std::array<float,20> values{.6f,.1f,.2f,0,.3f,.7f,.2f,0,.1f,.2f,.8f,0,0,0,0,1,-.08f,.04f,.12f,0};
 matrix->properties={wf::PropertyValue::CreateSingleArray(values),wf::PropertyValue::CreateUInt32(1),wf::PropertyValue::CreateBoolean(false)};
 matrix->sources={*blur};
 auto tint=winrt::make_self<SearchEffect>(CLSID_D2D1Flood);
 tint->properties={wf::PropertyValue::CreateSingleArray({0.04f,.09f,.15f,.63f})};
 auto composite=winrt::make_self<SearchEffect>(CLSID_D2D1Composite);
 composite->properties={wf::PropertyValue::CreateUInt32(0)};
 composite->sources={*matrix,*tint};
 std::puts("STEP create test effect factory");
 auto factory=compositor.CreateEffectFactory(*composite);
 std::puts("STEP capture test effect factory");
 testHooks.Add(winrt::get_abi(factory),6,reinterpret_cast<void*>(SearchCreateEffectBrushHook));
 Check(g_searchMaterialFactories.size()==1,"actual public effect description is cached without modifying its factory");

 {
  auto descriptor=winrt::make_self<SearchEffect>(CLSID_D2D1Flood);
  descriptor->properties={wf::PropertyValue::CreateSingleArray({.1f,.2f,.3f,.5f})};
  auto weak=winrt::make_weak(static_cast<searcheffect::IGraphicsEffect>(*descriptor));
  auto retainedFactory=compositor.CreateEffectFactory(*descriptor);
  descriptor=nullptr;
  Check(!weak.get(),"native factory cache does not retain the caller's custom effect implementation");
 }

 searchcomp::CompositionEffectBrush brush{nullptr};
 brush=factory.CreateBrush();
 brush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());
 SearchBrushData captured;
 Check(TryCaptureSearchCompositionBrush(brush,captured),"custom composition brush resolves without a theme name or preset");
 Check(captured.kind==5&&captured.graph.count==5,"blur/matrix/flood/composite graph is preserved");
 Check(captured.graph.nodes[1].properties[0].values[0]==37.f,"actual blur radius replaces guessed blur");
 Check(captured.graph.nodes[3].properties[0].values[3]==.63f,"actual fractional tint alpha replaces guessed black level");
 Check(std::equal(values.begin(),values.end(),captured.graph.nodes[2].properties[0].values),"saturation and luminosity matrix values survive exactly");
 auto recreated=RecreateSearchEffectGraph(captured.graph);
 auto copiedFactory=compositor.CreateEffectFactory(recreated);
 auto copiedBrush=copiedFactory.CreateBrush();
 copiedBrush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());
 SearchEffectGraph roundtrip;CaptureSearchEffectNode(recreated.as<searcheffect::IGraphicsEffectSource>(),roundtrip);
 Check(roundtrip==captured.graph,"reconstructed graph matches captured properties, order and connections");
 {
  auto identity=winrt::make_self<TestCachedIdentity>();
  auto object=identity.as<wf::IInspectable>();auto weak=winrt::make_weak(object);
  CacheSearchMaterial(g_searchMaterialFactories,object,captured.graph);
  identity=nullptr;object=nullptr;
  Check(!weak.get(),"factory definition cache does not keep its native object alive");
  identity=winrt::make_self<TestCachedIdentity>();object=identity.as<wf::IInspectable>();weak=winrt::make_weak(object);
  CacheSearchMaterial(g_searchMaterialBrushes,object,captured.graph);
  identity=nullptr;object=nullptr;
  Check(!weak.get(),"brush definition cache does not keep its native object alive");
 }
 {
  const auto create=g_searchCreateFactory;
  const auto createBrush=g_searchCreateEffectBrush;
  const auto createProperties=g_searchCreateFactoryWithProperties;
  g_searchCreateFactory=TestCreateFactory;
  g_searchCreateFactoryWithProperties=TestCreateFactoryWithProperties;
  g_searchCreateEffectBrush=TestCreateBrush;
  g_testCreationResult=factory.as<wf::IUnknown>();
  auto identity=factory.as<IUnknown>();
  SearchEffectGraph found;
  auto stale=[&]{CacheSearchMaterial(g_searchMaterialFactories,factory,captured.graph);};
  stale();
  {searchcomp::CompositionEffectFactory output{nullptr};SearchCreateFactoryHook(nullptr,nullptr,winrt::put_abi(output));}
  Check(!FindSearchMaterial(g_searchMaterialFactories,factory,found),"reused factory identity is forgotten when there is no capturable effect");
  stale();
  auto unsupported=wf::PropertyValue::CreateString(L"not an effect graph");
  {searchcomp::CompositionEffectFactory output{nullptr};SearchCreateFactoryHook(nullptr,winrt::get_abi(unsupported),winrt::put_abi(output));}
  Check(!FindSearchMaterial(g_searchMaterialFactories,factory,found),"throwing graph capture cannot leave a reused factory definition");
  stale();
  auto animated=winrt::single_threaded_vector<winrt::hstring>({L"Tint.Color"});
  {searchcomp::CompositionEffectFactory output{nullptr};SearchCreateFactoryWithPropertiesHook(nullptr,winrt::get_abi(static_cast<searcheffect::IGraphicsEffect>(*composite)),winrt::get_abi(animated.as<wf::Collections::IIterable<winrt::hstring>>()),winrt::put_abi(output));}
  Check(!FindSearchMaterial(g_searchMaterialFactories,factory,found),"animated creation invalidates a prior static graph at the same identity");
  stale();
  auto throwing=winrt::make_self<ThrowingPropertyNames>();
  {searchcomp::CompositionEffectFactory output{nullptr};SearchCreateFactoryWithPropertiesHook(nullptr,nullptr,winrt::get_abi(throwing.as<wf::Collections::IIterable<winrt::hstring>>()),winrt::put_abi(output));}
  Check(!FindSearchMaterial(g_searchMaterialFactories,factory,found),"animation enumeration failure still invalidates the reused factory identity");
  g_testCreationResult=brush.as<wf::IUnknown>();
  CacheSearchMaterial(g_searchMaterialBrushes,brush,captured.graph);
  {searchcomp::CompositionEffectBrush output{nullptr};SearchCreateEffectBrushHook(winrt::get_abi(factory),winrt::put_abi(output));}
  Check(!FindSearchMaterial(g_searchMaterialBrushes,brush,found),"brush creation clears a reused identity even if its factory was not captured");
  g_searchCreateFactory=create;g_searchCreateFactoryWithProperties=createProperties;g_searchCreateEffectBrush=createBrush;
  g_testCreationResult=nullptr;
  CacheSearchMaterial(g_searchMaterialFactories,factory,captured.graph);
  CacheSearchMaterial(g_searchMaterialBrushes,brush,captured.graph);
 }
 std::printf("GPU factory source=%d copy=%d error=%08X\n",int(factory.LoadStatus()),int(copiedFactory.LoadStatus()),unsigned(copiedFactory.ExtendedError())); Check(SUCCEEDED(copiedFactory.ExtendedError()),"Windows creates the reconstructed material without a GPU compilation error");
 for(int alpha=0;alpha<=255;++alpha){
  auto flood=winrt::make_self<SearchEffect>(CLSID_D2D1Flood);
  flood->properties={wf::PropertyValue::CreateSingleArray({.2f,.4f,.7f,alpha/255.f})};
  SearchEffectGraph graph;CaptureSearchEffectNode(static_cast<searcheffect::IGraphicsEffectSource>(*flood),graph);
  SearchEffectGraph again;CaptureSearchEffectNode(RecreateSearchEffectGraph(graph).as<searcheffect::IGraphicsEffectSource>(),again);
  Check(graph==again,"all tint alpha levels including zero round-trip without correction");
 }
 LARGE_INTEGER frequency{},began{},ended{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&began);
 bool cacheOk=true;for(int i=0;i<5000;++i){SearchBrushData sample;if(!TryCaptureSearchCompositionBrush(brush,sample)||sample.graph!=captured.graph)cacheOk=false;}
 QueryPerformanceCounter(&ended);Check(cacheOk,"5000 unchanged captures reuse the same definition");std::printf("CACHE average_us=%.2f entries=%zu\\n",(ended.QuadPart-began.QuadPart)*1000000.0/frequency.QuadPart/5000,g_searchMaterialFactories.size());
 auto emptyFactory=compositor.CreateEffectFactory(*composite,std::vector<winrt::hstring>{});auto emptyBrush=emptyFactory.CreateBrush();emptyBrush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());SearchBrushData emptyCapture;Check(TryCaptureSearchCompositionBrush(emptyBrush,emptyCapture),"explicit empty animation lists still capture static materials");
 auto color=compositor.CreateColorBrush(winrt::Windows::UI::Color{23,17,29,41});SearchBrushData solid;
 Check(TryCaptureSearchCompositionBrush(color,solid)&&solid.kind==1&&solid.color==0x17111D29,"actual fallback color preserves alpha and channels");
 brush.SetSourceParameter(L"arbitraryBackdropName",color);SearchBrushData colored;
 Check(TryCaptureSearchCompositionBrush(brush,colored)&&colored.graph.nodes[0].kind==3&&colored.graph.nodes[0].color==0x17111D29,"named color inputs are copied from the live brush");
 brush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateSurfaceBrush());SearchBrushData unsupported;
 Check(!TryCaptureSearchCompositionBrush(brush,unsupported),"non-transferable surfaces fail safely instead of being misread as backdrops");
 auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();
 brush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());
 auto custom=winrt::make_self<TestMaterialBrush>();custom->Attach(brush);
 auto xaml=custom.as<wuxm::XamlCompositionBrushBase>();
 Check(!g_searchMaterialXaml.empty(),"real composable factory call records the wrapper during its constructor");
 xaml.FallbackColor(winrt::Windows::UI::Color{200,1,2,3});xaml.Opacity(.35);
 auto full=CaptureSearchBrush(xaml);
 std::printf("XAML kind=%u cached=%zu fallback=%08X opacity=%.2f\n",full.kind,g_searchMaterialXaml.size(),full.fallback,full.opacity);
 Check(full.kind==5&&std::abs(full.opacity-.35)<1e-6&&full.fallback==0xC8010203,"actual XAML brush exposes its composition graph, opacity and fallback");
 custom->Attach(nullptr);xaml.Opacity(.72);xaml.FallbackColor(winrt::Windows::UI::Color{123,7,8,9});
 auto disconnected=CaptureSearchBrush(xaml);
 Check(disconnected.kind==5&&disconnected.graph==full.graph&&std::abs(disconnected.opacity-.72)<1e-6&&disconnected.fallback==0x7B070809,"hidden Start preserves the cached graph while current opacity/fallback still update");
 wuxm::AcrylicBrush acrylic;acrylic.TintColor(winrt::Windows::UI::Color{0,35,67,99});acrylic.TintOpacity(.23);acrylic.TintLuminosityOpacity(.67);acrylic.AlwaysUseFallback(true);
 auto native=CaptureSearchBrush(acrylic);
 Check(native.kind==2&&native.color==0x00234363&&native.tint==.23&&native.luminosity==.67&&native.options==1,"native acrylic preserves transparent tint, luminosity and explicit fallback mode");

 custom->Attach(brush);
 auto observed=CaptureSearchBrush(xaml);
 Check(factory.Properties().Comment().empty()&&brush.Properties().Comment().empty(),"factory and brush debug comments are untouched");
 auto properties=brush.Properties().try_as<winrt::Windows::Foundation::Collections::IPropertySet>();
 Check(!properties,"no custom property container is installed on the host brush");
 auto clearLiveCaches=[] {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();};
 clearLiveCaches();
 Check(CaptureSearchBrush(xaml).kind==5,"a previously observed live brush recovers from its plain continuity snapshot");
 custom->Attach(nullptr);
 Check(CaptureSearchBrush(xaml).kind==5,"a previously observed hidden brush recovers without reconnecting its resources");
 for(int pass=0;pass<20;++pass){
  clearLiveCaches();
  custom=winrt::make_self<TestMaterialBrush>();xaml=custom.as<wuxm::XamlCompositionBrushBase>();
  factory=compositor.CreateEffectFactory(*composite);brush=factory.CreateBrush();
  brush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());custom->Attach(brush);
  auto fresh=CaptureSearchBrush(xaml);
  Check(fresh.kind==5&&fresh.graph==observed.graph,"newly created theme material is captured after each cache reset");
  custom->Attach(nullptr);
  auto hidden=CaptureSearchBrush(xaml);
  Check(hidden.kind==5&&hidden.graph==fresh.graph,"the same live brush keeps its current-load definition while hidden");
  custom->Attach(brush);
 }
 {
  g_testCreationResult=brush.as<wf::IUnknown>();
  const auto createBrush=g_searchCreateEffectBrush;g_searchCreateEffectBrush=TestCreateBrush;
  ForgetSearchMaterial(g_searchMaterialFactories,winrt::get_abi(factory));
  {searchcomp::CompositionEffectBrush output{nullptr};SearchCreateEffectBrushHook(winrt::get_abi(factory),winrt::put_abi(output));}
  Check(CaptureSearchBrush(xaml).kind==4,"a reused composition identity cannot inherit another material from the live-owner cache");
  custom->Attach(nullptr);Check(CaptureSearchBrush(xaml).kind==4,"failed live capture does not expose an older hidden material");
  g_searchCreateEffectBrush=createBrush;g_testCreationResult=nullptr;
  CacheSearchMaterial(g_searchMaterialFactories,factory,captured.graph);CacheSearchMaterial(g_searchMaterialBrushes,brush,captured.graph);custom->Attach(brush);CaptureSearchBrush(xaml);
 }
 searchcomp::CompositionEffectFactory unseenFactory{nullptr};
 winrt::check_hresult(g_searchCreateFactory(winrt::get_abi(compositor),winrt::get_abi(static_cast<searcheffect::IGraphicsEffect>(*composite)),winrt::put_abi(unseenFactory)));
 searchcomp::CompositionEffectBrush unseenBrush{nullptr};
 winrt::check_hresult(g_searchCreateEffectBrush(winrt::get_abi(unseenFactory),winrt::put_abi(unseenBrush)));
 unseenBrush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());custom->Attach(unseenBrush);
 Check(CaptureSearchBrush(xaml).kind==4,"unobserved replacement material keeps the safe brush fallback");
 custom->Attach(nullptr);Check(CaptureSearchBrush(xaml).kind==4,"disconnecting an unsupported replacement cannot restore the previous theme");
 custom->Attach(brush);Check(CaptureSearchBrush(xaml).kind==5,"observed live material remains available without reload storage");
 std::vector<searchcomp::CompositionEffectFactory> manyFactories;
 std::vector<searchcomp::CompositionEffectBrush> manyBrushes;
 for(int i=0;i<350;++i){auto next=compositor.CreateEffectFactory(*composite);auto value=next.CreateBrush();value.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());manyFactories.push_back(next);manyBrushes.push_back(value);}
 Check(g_searchMaterialFactories.size()==kSearchMaterialCacheLimit&&g_searchMaterialBrushes.size()==kSearchMaterialCacheLimit,"plain-data caches stay bounded when hundreds of native objects remain alive");
 Check(CaptureSearchBrush(xaml).kind==5,"a current live XAML material survives general-cache eviction without persistent storage");
 brush.SetSourceParameter(L"arbitraryBackdropName",color);auto liveColor=CaptureSearchBrush(xaml);
 Check(liveColor.kind==5&&liveColor.graph.nodes[0].color==0x17111D29,"cached live material still resolves current source colors");
 brush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());CaptureSearchBrush(xaml);
 Check(factory.Properties().Comment().empty()&&brush.Properties().Comment().empty(),"cache reset and eviction leave host metadata untouched");

 custom=nullptr;xaml=nullptr;
 for(int i=0;i<100;++i){auto temporary=winrt::make_self<TestMaterialBrush>();temporary->Attach(brush);auto snapshot=CaptureSearchBrush(temporary.as<wuxm::XamlCompositionBrushBase>());Check(snapshot.kind==5,"recreated custom brushes capture after prior owners are destroyed");}
 auto wrong=captured.graph;wrong.count=13;Check(!ValidSearchEffectGraph(wrong),"oversized graphs are rejected");
 wrong=captured.graph;wrong.nodes[1].sources[0]=1;Check(!ValidSearchEffectGraph(wrong),"cyclic and forward references are rejected");
 wrong=captured.graph;wrong.nodes[0].parameter[31]='x';Check(!ValidSearchEffectGraph(wrong),"unterminated parameter strings are rejected");
 wrong=captured.graph;wrong.nodes[2].properties[0].values[0]=NAN;Check(!ValidSearchEffectGraph(wrong),"nonfinite GPU properties are rejected");
 wrong=captured.graph;wrong.nodes[2].properties[0].count=21;Check(!ValidSearchEffectGraph(wrong),"oversized matrix payloads are rejected");
 wrong=captured.graph;wrong.nodes[1].id=GUID_NULL;Check(!ValidSearchEffectGraph(wrong),"unknown GPU effects are rejected");
 auto recursive=winrt::make_self<SearchEffect>(CLSID_D2D1Composite);recursive->sources={*recursive};
 bool rejected=false;try{SearchEffectGraph cycle;CaptureSearchEffectNode(static_cast<searcheffect::IGraphicsEffectSource>(*recursive),cycle);}catch(...){rejected=true;}
 recursive->sources.clear();Check(rejected,"cyclic effect descriptions hit bounded recursion");
 // Materials that predate this load are read passively. Capturing must never
 // call lifecycle overrides on another owner's brush, visible or disconnected.
 {
  auto reset=[] {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();};
  g_searchMaterialCapture=false;reset();
  auto warm=winrt::make_self<TestWarmMaterialBrush>();warm->OnConnected();auto warmXaml=warm.as<wuxm::XamlCompositionBrushBase>();
  Check(!warmXaml.try_as<wuxm::IXamlCompositionBrushBaseProtected>(),"pre-existing opaque brush keeps its protected interface private");
  for(int pass=0;pass<20;++pass){
   g_searchMaterialCapture=false;reset();warm->OnDisconnected();warm->radius=43.f+pass;warm->OnConnected();
   warm->hideProtected=pass%4<2;
   if(pass%2)warm->OnDisconnected();
   const int connects=warm->connects,disconnects=warm->disconnects;
   auto before=warm->Current();g_searchMaterialCapture=true;
   auto material=CaptureSearchBrush(warmXaml);
   Check(material.kind==4,"uncaptured pre-existing effects use the documented approximation");
   for(int read=0;read<100;++read)CaptureSearchBrush(warmXaml);
   Check(warm->connects==connects&&warm->disconnects==disconnects,"appearance polling never invokes a foreign brush lifecycle");
   Check(warm->Current()==before,"capture preserves the current composition assignment, including null");
  }
  warm->OnDisconnected();warm=nullptr;warmXaml=nullptr;
 }

 // Reload continuity uses only this mod's serialized data and current identities.
 {
  auto clearLive=[] {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();};
  auto resetRuntime=[&] {clearLive();g_searchSavedMaterials={};g_searchMaterialStorage[0]=0;g_searchSavedMaterialsDirty=false;};
  testStorage.clear();resetRuntime();LoadSearchMaterialStorage();g_searchMaterialCapture=true;
  auto warm=winrt::make_self<TestWarmMaterialBrush>();warm->hideProtected=false;warm->OnConnected();
  auto warmXaml=warm.as<wuxm::XamlCompositionBrushBase>();auto original=CaptureSearchBrush(warmXaml);
  Check(original.kind==5&&g_searchSavedMaterials.count==1,"an observed custom material seeds the plain-data continuity cache");
  SearchSavedMaterial keyA,keyB;SearchMaterialOwnerKey(warmXaml,keyA);SearchMaterialOwnerKey(warmXaml,keyB);
  Check(keyA.owner==keyB.owner&&keyA.weak==keyB.weak&&keyA.weak,"weak identity stays stable without retaining another reference");
  for(int pass=0;pass<20;++pass){
   if(pass%2)warm->OnDisconnected();
   SaveSearchMaterialStorage();g_searchMaterialCapture=false;resetRuntime();LoadSearchMaterialStorage();g_searchMaterialCapture=true;
   warm->hideProtected=pass%4<2;
   const int connects=warm->connects,disconnects=warm->disconnects;auto before=warm->Current();
   auto recovered=CaptureSearchBrush(warmXaml);
   Check(recovered.kind==5&&recovered.graph==original.graph,"reload recovers the exact graph for the same visible or hidden brush");
   Check(warm->connects==connects&&warm->disconnects==disconnects&&warm->Current()==before,"reload never reconnects or replaces the host's resources");
   const int reads=storageReads,writes=storageWrites;
   for(int read=0;read<20;++read)CaptureSearchBrush(warmXaml);
   Check(storageReads==reads&&storageWrites==writes,"appearance polling performs no recovery storage I/O");
   if(!before){warm->OnConnected();warm->hideProtected=false;original=CaptureSearchBrush(warmXaml);}
  }
  warm->hideProtected=true;clearLive();SaveSearchMaterialStorage();
  auto valid=std::make_unique<SearchSavedMaterials>(g_searchSavedMaterials);
  auto blob=[&](SearchSavedMaterials const& input){auto saved=std::make_unique<SearchSavedMaterials>(input);saved->checksum=SearchMaterialChecksum(*saved);size_t size=offsetof(SearchSavedMaterials,entries)+saved->count*sizeof(SearchSavedMaterial);auto bytes=reinterpret_cast<unsigned char const*>(saved.get());testStorage[g_searchMaterialStorage]=std::vector<unsigned char>(bytes,bytes+size);};
  auto expectRejected=[&](SearchSavedMaterials const& saved,const char* label){blob(saved);resetRuntime();LoadSearchMaterialStorage();Check(g_searchSavedMaterials.count==0,label);};
  auto invalid=std::make_unique<SearchSavedMaterials>(*valid);invalid->process^=1;expectRejected(*invalid,"a different process ID cannot reuse material continuity data");
  *invalid=*valid;invalid->created^=1;expectRejected(*invalid,"a restarted Start with a reused PID cannot reuse old materials");
  *invalid=*valid;invalid->version++;expectRejected(*invalid,"unknown recovery formats fail closed");
  *invalid=*valid;invalid->graphSize++;expectRejected(*invalid,"incompatible graph layouts are rejected");
  *invalid=*valid;invalid->entries[0].graph.nodes[0].kind=99;expectRejected(*invalid,"invalid effects are rejected even with a correct checksum");
  blob(*valid);auto& bytes=testStorage[g_searchMaterialStorage];bytes.back()^=1;resetRuntime();LoadSearchMaterialStorage();Check(g_searchSavedMaterials.count==0,"corrupted cache bytes are rejected");
  blob(*valid);testStorage[g_searchMaterialStorage].pop_back();resetRuntime();LoadSearchMaterialStorage();Check(g_searchSavedMaterials.count==0,"truncated cache records are rejected");
  blob(*valid);resetRuntime();LoadSearchMaterialStorage();clearLive();
  g_searchSavedMaterials.entries[0].weak^=2;
  Check(CaptureSearchBrush(warmXaml).kind==4,"a different weak generation cannot reuse an owner's address");
  g_searchSavedMaterials=*valid;warm->FallbackColor(winrt::Windows::UI::Color{255,17,23,41});
  Check(CaptureSearchBrush(warmXaml).kind==4,"a changed public fallback color invalidates saved appearance");
  warm->FallbackColor(winrt::Windows::UI::Color{});g_searchSavedMaterials=*valid;warm->hideProtected=false;
  auto previousNative=warm->Current();g_searchMaterialCapture=false;warm->OnDisconnected();warm->OnConnected();g_searchMaterialCapture=true;clearLive();previousNative=nullptr;
  Check(CaptureSearchBrush(warmXaml).kind==4,"a changed readable composition identity cannot reuse an older graph");
  g_searchSavedMaterials=*valid;g_searchSavedMaterialsDirty=true;storageWritable=false;SaveSearchMaterialStorage();
  Check(g_searchSavedMaterialsDirty,"failed storage writes leave recovery retryable");storageWritable=true;SaveSearchMaterialStorage();
  Check(!g_searchSavedMaterialsDirty,"successful storage writes clear the dirty flag");
  std::vector<winrt::com_ptr<TestMaterialBrush>> owners;g_searchSavedMaterials.count=0;
  for(int i=0;i<40;++i){auto owner=winrt::make_self<TestMaterialBrush>();owner->Attach(warm->Current());RememberSearchMaterial(owner.as<wuxm::XamlCompositionBrushBase>(),warm->Current(),original.graph);owners.push_back(owner);}
  Check(g_searchSavedMaterials.count==32,"continuity storage has a strict record bound");
  const auto count=g_searchSavedMaterials.count;ForgetSavedSearchMaterial(owners.back().as<wf::IUnknown>(),true);
  Check(g_searchSavedMaterials.count==count-1,"observed XAML creation invalidates an old identity record");
  ForgetSavedSearchMaterial(warm->Current(),false);
  Check(g_searchSavedMaterials.count==0,"observed native creation invalidates matching saved compositions");
  warm->OnDisconnected();warm=nullptr;warmXaml=nullptr;owners.clear();testStorage.clear();resetRuntime();
 }
 brush=nullptr;factory=nullptr;copiedBrush=nullptr;copiedFactory=nullptr;
 std::thread stopOnEngineThread([]{winrt::init_apartment(winrt::apartment_type::multi_threaded);StopSearchMaterialCapture();winrt::uninit_apartment();});stopOnEngineThread.join();Check(g_searchMaterialFactories.empty()&&g_searchMaterialBrushes.empty()&&g_searchMaterialXaml.empty(),"engine-thread shutdown clears definitions without owning factory or brush references");
 std::printf("RESULT %d checks, %d failures; appearance packet %zu bytes\n",checks,failures,sizeof(SearchBrushData));
 return failures?1:0;
}catch(winrt::hresult_error const& e){std::printf("ERROR %08X %ls\n",unsigned(e.code()),e.message().c_str());return 2;}catch(...){std::puts("ERROR unknown");return 3;}

LONG WINAPI Fault(EXCEPTION_POINTERS* e){std::printf("FAULT %08X at %p\n",unsigned(e->ExceptionRecord->ExceptionCode),e->ExceptionRecord->ExceptionAddress);void* frames[30];auto n=CaptureStackBackTrace(0,30,frames,nullptr);for(USHORT i=0;i<n;++i){HMODULE m=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<PCWSTR>(frames[i]),&m);wchar_t path[512]{};GetModuleFileNameW(m,path,512);std::printf("FRAME %p +%llx %ls\n",frames[i],(unsigned long long)((char*)frames[i]-(char*)m),path);}return EXCEPTION_EXECUTE_HANDLER;}
void ExitWithLiveMaterialCaches()try{
 std::setvbuf(stdout,nullptr,_IONBF,0);
 InitSearchMaterialCapture();
 searchcomp::Compositor compositor;
 auto effect=winrt::make_self<SearchEffect>(CLSID_D2D1Flood);
 effect->properties={wf::PropertyValue::CreateSingleArray({.1f,.2f,.3f,.5f})};
 auto factory=compositor.CreateEffectFactory(*effect);
 NoteSearchEffectFactory(winrt::get_abi(static_cast<searcheffect::IGraphicsEffect>(*effect)),winrt::get_abi(factory));
 searchcomp::CompositionEffectBrush brush{nullptr};
 winrt::check_hresult(SearchCreateEffectBrushHook(winrt::get_abi(factory),winrt::put_abi(brush)));
 Check(g_searchMaterialFactories.size()==1&&g_searchMaterialBrushes.size()==1,"process exit starts with populated material caches and live composition objects");
 std::printf("RESULT process-exit checks=%d failures=%d; no unload callback\n",checks,failures);
 // Deliberately skip StopSearchMaterialCapture and terminate the main thread
 // before global destructors run, exactly like a normal host-process exit.
 ExitProcess(failures?1:0);
}catch(...){std::puts("ERROR process-exit setup failed");ExitProcess(2);}
int main(int argc,char** argv){SetUnhandledExceptionFilter(Fault);AddVectoredExceptionHandler(1,[](EXCEPTION_POINTERS* e)->LONG{if(e->ExceptionRecord->ExceptionCode==0xc0000005)Fault(e);return EXCEPTION_CONTINUE_SEARCH;});winrt::init_apartment(winrt::apartment_type::multi_threaded);auto c=winrt::Windows::System::DispatcherQueueController::CreateOnDedicatedThread();std::promise<int> done;auto f=done.get_future();c.DispatcherQueue().TryEnqueue([&]{if(argc>1&&std::strcmp(argv[1],"--exit-live-cache")==0)ExitWithLiveMaterialCaches();done.set_value(Run());});int result=f.get();c.ShutdownQueueAsync().get();return result;}
