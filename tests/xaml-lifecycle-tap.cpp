#define NOMINMAX
#include <windows.h>
#include <xamlom.h>
#include <ocidl.h>
#include <winrt/base.h>
#include <atomic>
#include <cstdio>
struct Watcher : winrt::implements<Watcher,IVisualTreeServiceCallback2,winrt::non_agile>{
 static std::atomic<DWORD> taskbarThread; static std::atomic<unsigned> taskbarAdds,otherAdds;static std::atomic<HRESULT> lastHr;
 winrt::com_ptr<IVisualTreeService3> service;
 HRESULT STDMETHODCALLTYPE OnVisualTreeChange(ParentChildRelation,VisualElement,VisualMutationType type)override{if(type==Add){if(GetCurrentThreadId()==taskbarThread)++taskbarAdds;else ++otherAdds;}return S_OK;}
 HRESULT STDMETHODCALLTYPE OnElementStateChanged(InstanceHandle,VisualElementState,LPCWSTR)noexcept override{return S_OK;}
};
std::atomic<DWORD> Watcher::taskbarThread{};std::atomic<unsigned> Watcher::taskbarAdds{},Watcher::otherAdds{};std::atomic<HRESULT> Watcher::lastHr{E_PENDING};
winrt::com_ptr<Watcher> watcher;
struct Tap:winrt::implements<Tap,IObjectWithSite,winrt::non_agile>{winrt::com_ptr<IUnknown> site;
 HRESULT STDMETHODCALLTYPE SetSite(IUnknown* value)noexcept override{try{if(watcher){watcher->service->UnadviseVisualTreeChange(watcher.get());watcher=nullptr;}site.copy_from(value);if(value){watcher=winrt::make_self<Watcher>();watcher->service=site.as<IVisualTreeService3>();Watcher::lastHr=E_PENDING;auto* ptr=watcher.get();ptr->AddRef();HANDLE thread=CreateThread(nullptr,0,[](void* p)->DWORD{CoInitializeEx(nullptr,COINIT_MULTITHREADED);auto* w=static_cast<Watcher*>(p);Watcher::lastHr=w->service->AdviseVisualTreeChange(w);w->Release();CoUninitialize();return 0;},ptr,0,nullptr);if(thread)CloseHandle(thread);else{ptr->Release();return HRESULT_FROM_WIN32(GetLastError());}}return S_OK;}catch(...){return winrt::to_hresult();}}
 HRESULT STDMETHODCALLTYPE GetSite(REFIID id,void** result)noexcept override{return site.as(id,result);}
};
struct Factory:winrt::implements<Factory,IClassFactory,winrt::non_agile>{HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID id,void** out)noexcept override{if(outer)return CLASS_E_NOAGGREGATION;try{return winrt::make<Tap>().as(id,out);}catch(...){return winrt::to_hresult();}}HRESULT STDMETHODCALLTYPE LockServer(BOOL)noexcept override{return S_OK;}};
extern "C" __declspec(dllexport) HRESULT __stdcall DllGetClassObject(REFCLSID,REFIID id,void** out){try{return winrt::make<Factory>().as(id,out);}catch(...){return winrt::to_hresult();}}
extern "C" __declspec(dllexport) HRESULT __stdcall DllCanUnloadNow(){return S_FALSE;}
extern "C" __declspec(dllexport) void SetThread(DWORD tid){Watcher::taskbarThread=tid;}
extern "C" __declspec(dllexport) void Reset(){if(watcher){watcher->service->UnadviseVisualTreeChange(watcher.get());watcher=nullptr;}Watcher::taskbarAdds=0;Watcher::otherAdds=0;Watcher::lastHr=E_PENDING;}
extern "C" __declspec(dllexport) void Read(unsigned* own,unsigned* other,HRESULT* hr){*own=Watcher::taskbarAdds;*other=Watcher::otherAdds;*hr=Watcher::lastHr;}
