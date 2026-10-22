#define NOMINMAX
#include <windows.h>
#include <xamlom.h>
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#undef GetCurrentTime
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <atomic>
#include <thread>
#include <string>
#include <cstdio>
namespace wux=winrt::Windows::UI::Xaml;
// PRODUCTION_SHUTDOWN
struct Host {HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);DWORD tid=0;HWND hwnd=nullptr;std::thread thread;bool drain=false;
 void Start(bool doDrain){drain=doDrain;thread=std::thread([this]{try{winrt::init_apartment(winrt::apartment_type::single_threaded);tid=GetCurrentThreadId();hwnd=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"Isolated XAML lifecycle test",WS_POPUP,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);auto manager=wux::Hosting::WindowsXamlManager::InitializeForCurrentThread();auto source=wux::Hosting::DesktopWindowXamlSource();source.as<IDesktopWindowXamlSourceNative>()->AttachToWindow(hwnd);{auto grid=wux::Controls::Grid();grid.Name(L"TaskbarTestRoot");grid.Children().Append(wux::Controls::TextBlock());source.Content(grid);}SetEvent(ready);MSG m;while(GetMessageW(&m,nullptr,0,0)>0){TranslateMessage(&m);DispatchMessageW(&m);}source.Content(nullptr);source.Close();source=nullptr;manager.Close();manager=nullptr;DestroyWindow(hwnd);if(drain)DrainSearchXamlThreadMessages();winrt::uninit_apartment();}catch(winrt::hresult_error const& e){printf("HOST ERROR %08X\n",unsigned(e.code()));SetEvent(ready);}});if(WaitForSingleObject(ready,10000)!=0)throw std::runtime_error("host startup timeout");}
 void Stop(){PostThreadMessageW(tid,WM_QUIT,0,0);thread.join();CloseHandle(ready);}
};
int wmain(int argc,wchar_t** argv){setvbuf(stdout,nullptr,_IONBF,0);bool drain=argc>1&&std::wstring(argv[1])==L"--drain";try{winrt::init_apartment(winrt::apartment_type::multi_threaded);wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);auto path=std::wstring(exe);path=path.substr(0,path.find_last_of(L"\\/")+1)+L"lifecycle-tap.dll";auto lib=LoadLibraryW(path.c_str());if(!lib)throw winrt::hresult_error(HRESULT_FROM_WIN32(GetLastError()));auto setThread=reinterpret_cast<void(*)(DWORD)>(GetProcAddress(lib,"SetThread"));auto reset=reinterpret_cast<void(*)()>(GetProcAddress(lib,"Reset"));auto read=reinterpret_cast<void(*)(unsigned*,unsigned*,HRESULT*)>(GetProcAddress(lib,"Read"));Host taskbar;taskbar.Start(true);setThread(taskbar.tid);auto wuxlib=LoadLibraryW(L"Windows.UI.Xaml.dll");auto attach=reinterpret_cast<decltype(&InitializeXamlDiagnosticsEx)>(GetProcAddress(wuxlib,"InitializeXamlDiagnosticsEx"));CLSID id{0x68bd73e4,0x75d2,0x4bad,{0xb3,0xb2,0x9b,0x8f,0xa2,0x34,0x13,0x44}};
 int failures=0,checks=0;
 for(int round=0;round<(drain?13:4);++round){if(round){Host extra;extra.Start(drain);Sleep(100);extra.Stop();}reset();HRESULT hr=HRESULT_FROM_WIN32(ERROR_NOT_FOUND);int found=0;for(int i=1;i<=10000;++i){wchar_t name[80];swprintf(name,80,L"VisualDiagConnection%d",i);hr=attach(name,GetCurrentProcessId(),L"",path.c_str(),id,nullptr);if(hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND)){found=i;break;}}unsigned own=0,other=0;HRESULT advice=E_PENDING;for(int i=0;i<100;++i){read(&own,&other,&advice);if(advice!=E_PENDING)break;Sleep(20);}printf("ROUND %d drain=%d connection=%d attach=%08X advice=%08X taskbarAdds=%u otherAdds=%u\n",round,drain,found,unsigned(hr),unsigned(advice),own,other);++checks;if(FAILED(hr)||((drain||round==0)&&own<7)||(drain?advice!=S_OK:(round>0?advice!=E_PENDING:advice!=S_OK)))++failures;}
 reset();taskbar.Stop();winrt::uninit_apartment();printf("RESULT %d checks, %d failures\n",checks,failures);return failures?1:0;}catch(winrt::hresult_error const& e){printf("ERROR %08X\n",unsigned(e.code()));return 1;}catch(std::exception const& e){printf("ERROR %s\n",e.what());return 1;}}
