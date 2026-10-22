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
#include <cwchar>
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
static std::unordered_map<std::wstring,std::vector<unsigned char>> g_storageByKey;
static auto& g_storage=g_storageByKey[L"appearanceMaterialCache.v1.session.1"];
static DWORD g_testSession=1;
static bool g_sessionQueryFails=false;
BOOL TestProcessIdToSessionId(DWORD,DWORD* session){if(g_sessionQueryFails)return FALSE;*session=g_testSession;return TRUE;}
#define ProcessIdToSessionId TestProcessIdToSessionId
static int g_storageReads=0,g_storageWrites=0,g_storageDeletes=0;
size_t Wh_GetBinaryValue(PCWSTR key,void* data,size_t size){++g_storageReads;auto& value=g_storageByKey[key];if(value.size()>size)return 0;if(!value.empty())std::memcpy(data,value.data(),value.size());return value.size();}
BOOL Wh_SetBinaryValue(PCWSTR key,void const* data,size_t size){++g_storageWrites;auto first=static_cast<unsigned char const*>(data);g_storageByKey[key].assign(first,first+size);return TRUE;}
BOOL Wh_DeleteValue(PCWSTR key){++g_storageDeletes;g_storageByKey[key].clear();return TRUE;}
namespace {
// PRODUCTION
static_assert(std::is_trivially_destructible_v<SearchCapturedMaterial>,"factory and brush entries must never own COM resources");
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
 auto savedBefore=CaptureSearchBrush(xaml);
 const auto writesBefore=g_storageWrites;const auto readsBefore=g_storageReads;
 for(int i=0;i<500;++i)CaptureSearchBrush(xaml);
 Check(g_storageWrites==writesBefore&&g_storageReads==readsBefore,"normal appearance polling performs no storage I/O");
 SaveSearchMaterialStorage();
 Check(g_storageWrites==writesBefore+1&&!g_storage.empty(),"definitions are saved through the mod storage API");
 auto goodStorage=g_storage;
 Check(std::wstring(g_searchMaterialStorage)==L"appearanceMaterialCache.v1.session.1","recovery storage key is scoped to the Windows session");
 {
  auto& other=g_storageByKey[L"appearanceMaterialCache.v1.session.2"];
  other={1,2,3,4};
  LoadSearchMaterialStorage();
  Check(other==std::vector<unsigned char>({1,2,3,4}),"loading one session does not read or delete another session's cache");
  g_testSession=2;LoadSearchMaterialStorage();
  Check(other.empty()&&g_storage==goodStorage,"discarding an invalid current-session record preserves the other session");
  g_testSession=1;LoadSearchMaterialStorage();
  auto& legacy=g_storageByKey[kSearchMaterialLegacyStorage];
  legacy=goodStorage;g_storage.clear();LoadSearchMaterialStorage();
  Check(g_searchSavedMaterials.count&&g_searchSavedMaterialsDirty,"same-process legacy recovery survives migration to a session key");
  SaveSearchMaterialStorage();
  Check(!g_storage.empty()&&legacy==goodStorage,"migration writes the session cache without deleting the shared legacy record");
  auto invalidLegacy=std::make_unique<SearchSavedMaterials>();std::memcpy(invalidLegacy.get(),goodStorage.data(),goodStorage.size());invalidLegacy->process^=1;
  std::memcpy(legacy.data(),invalidLegacy.get(),legacy.size());auto untouchedLegacy=legacy;
  g_storage.clear();LoadSearchMaterialStorage();
  Check(!g_searchSavedMaterials.count&&legacy==untouchedLegacy,"a foreign legacy record is ignored and left for its owning session");
  legacy.clear();g_storage=goodStorage;LoadSearchMaterialStorage();
  const auto reads=g_storageReads,writes=g_storageWrites,deletes=g_storageDeletes;
  g_sessionQueryFails=true;LoadSearchMaterialStorage();g_searchSavedMaterialsDirty=true;SaveSearchMaterialStorage();
  Check(g_storageReads==reads&&g_storageWrites==writes&&g_storageDeletes==deletes,"missing session identity disables storage I/O instead of using a shared key");
  g_sessionQueryFails=false;LoadSearchMaterialStorage();
 }
 Check(factory.Properties().Comment().empty()&&brush.Properties().Comment().empty(),"factory and brush debug comments are untouched");
 auto properties=brush.Properties().try_as<winrt::Windows::Foundation::Collections::IPropertySet>();
 Check(!properties,"no custom property container is installed on the host brush");
 {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();g_searchSavedMaterials={};}
 LoadSearchMaterialStorage();
 auto liveReload=CaptureSearchBrush(xaml);
 Check(liveReload.kind==5&&liveReload.graph==savedBefore.graph,"live material recovers exclusively from Windhawk storage after reload");
 custom->Attach(nullptr);
 {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();g_searchSavedMaterials={};}
 LoadSearchMaterialStorage();
 xaml.Opacity(.42);xaml.FallbackColor(winrt::Windows::UI::Color{91,4,8,12});
 auto hiddenReload=CaptureSearchBrush(xaml);
 Check(hiddenReload.kind==5&&hiddenReload.graph==savedBefore.graph&&std::abs(hiddenReload.opacity-.42)<1e-6&&hiddenReload.fallback==0x5B04080C,"hidden Start recovers while opacity and fallback remain current");
 Check(CaptureSearchBrush(xaml)==hiddenReload,"unchanged hidden captures reuse the local cache");
 auto another=winrt::make_self<TestMaterialBrush>();
 Check(CaptureSearchBrush(another.as<wuxm::XamlCompositionBrushBase>()).kind!=5,"new theme brush does not inherit another brush's saved definition");
 // Create a native effect outside the hooks, as if another theme changed it
 // while this mod was disabled. A different composition identity rejects it.
 searchcomp::CompositionEffectFactory unseenFactory{nullptr};
 winrt::check_hresult(g_searchCreateFactory(winrt::get_abi(compositor),winrt::get_abi(static_cast<searcheffect::IGraphicsEffect>(*composite)),winrt::put_abi(unseenFactory)));
 searchcomp::CompositionEffectBrush unseenBrush{nullptr};
 winrt::check_hresult(g_searchCreateEffectBrush(winrt::get_abi(unseenFactory),winrt::put_abi(unseenBrush)));
 unseenBrush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());
 custom->Attach(unseenBrush);
 Check(CaptureSearchBrush(xaml).kind==4,"changed unobserved composition does not reuse an old theme snapshot");
 custom->Attach(brush);
 auto current=CaptureSearchBrush(xaml);
 Check(current.kind==5,"same live native composition can recover again");
 {
  g_testCreationResult=brush.as<wf::IUnknown>();
  const auto createBrush=g_searchCreateEffectBrush;g_searchCreateEffectBrush=TestCreateBrush;
  ForgetSearchMaterial(g_searchMaterialFactories,winrt::get_abi(factory));
  {searchcomp::CompositionEffectBrush output{nullptr};SearchCreateEffectBrushHook(winrt::get_abi(factory),winrt::put_abi(output));}
  Check(CaptureSearchBrush(xaml).kind==4,"a recreated unsupported brush cannot inherit a persisted graph from its reused address");
  g_searchCreateEffectBrush=createBrush;g_testCreationResult=nullptr;
  CacheSearchMaterial(g_searchMaterialFactories,factory,captured.graph);CacheSearchMaterial(g_searchMaterialBrushes,brush,captured.graph);CaptureSearchBrush(xaml);
  auto inner=custom->Inner();
  NoteSearchXamlBrush(winrt::get_abi(xaml),winrt::get_abi(inner),winrt::get_abi(inner));
  SearchEffectGraph previous;
  Check(!RecallSearchMaterial(xaml,nullptr,previous),"a recreated XAML wrapper cannot inherit a disconnected snapshot from its reused address");
  CaptureSearchBrush(xaml);
 }
 for(int pass=0;pass<20;++pass){
  SaveSearchMaterialStorage();custom->Attach(nullptr);
  {std::lock_guard lock(g_searchMaterialMutex);g_searchMaterialFactories.clear();g_searchMaterialBrushes.clear();g_searchMaterialXaml.clear();g_searchSavedMaterials={};}
  LoadSearchMaterialStorage();
  auto recovered=CaptureSearchBrush(xaml);Check(recovered.kind==5&&recovered.graph==savedBefore.graph,"repeated hidden update/off-on cycles retain the exact definition");
  custom->Attach(brush);CaptureSearchBrush(xaml);
 }
 // Rebuild storage guards from a known-good record for each malformed case.
 for(int kind=0;kind<7;++kind){
  g_storage=goodStorage;
  auto bad=std::make_unique<SearchSavedMaterials>();std::memcpy(bad.get(),g_storage.data(),g_storage.size());
  if(kind==0)bad->version=99;
  if(kind==1)bad->graphSize=1;
  if(kind==2)bad->process^=1;
  if(kind==3)bad->created^=1;
  if(kind==4)bad->count=33;
  if(kind==5)bad->entries[0].graph.count=99;
  if(kind==6)bad->entries[0].graph.nodes[0].parameter[31]='x';
  std::memcpy(g_storage.data(),bad.get(),g_storage.size());
  LoadSearchMaterialStorage();Check(g_searchSavedMaterials.count==0&&g_storage.empty(),"incompatible process, schema or malformed graph is discarded");
 }
 g_storage=goodStorage;LoadSearchMaterialStorage();
 for(int i=0;i<350;++i){auto temporaryFactory=compositor.CreateEffectFactory(*composite);auto temporaryBrush=temporaryFactory.CreateBrush();temporaryBrush.SetSourceParameter(L"arbitraryBackdropName",compositor.CreateBackdropBrush());}
 Check(g_searchMaterialFactories.size()<=kSearchMaterialCacheLimit&&g_searchMaterialBrushes.size()<=kSearchMaterialCacheLimit,"plain-data caches remain bounded under factory and brush churn");
 Check(CaptureSearchBrush(xaml).kind==5,"an evicted material recovers from the mod's last-known definition");
 Check(factory.Properties().Comment().empty()&&brush.Properties().Comment().empty(),"reload, cache eviction and recovery leave host metadata untouched");

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
