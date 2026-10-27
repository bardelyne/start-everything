// A short-lived, network-free service used only by the explicit elevated test.
#define NOMINMAX
#include <windows.h>
#include <winsvc.h>
SERVICE_STATUS_HANDLE statusHandle=nullptr;
HANDLE stopEvent=nullptr;
void Report(DWORD state,DWORD controls){
    SERVICE_STATUS status{};status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState=state;status.dwControlsAccepted=controls;
    status.dwWaitHint=state==SERVICE_STOP_PENDING?1000:0;
    SetServiceStatus(statusHandle,&status);
}
DWORD WINAPI Control(DWORD control,DWORD,void*,void*){
    if(control==SERVICE_CONTROL_STOP){Report(SERVICE_STOP_PENDING,0);SetEvent(stopEvent);}
    return NO_ERROR;
}
void WINAPI Main(DWORD count,wchar_t** args){
    if(!count)return;
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    statusHandle=RegisterServiceCtrlHandlerExW(args[0],Control,nullptr);
    if(!statusHandle || !stopEvent)return;
    Report(SERVICE_RUNNING,SERVICE_ACCEPT_STOP);
    WaitForSingleObject(stopEvent,120000); // Backstop even if the test exits early.
    Report(SERVICE_STOPPED,0);CloseHandle(stopEvent);
}
int wmain(){SERVICE_TABLE_ENTRYW table[]={{const_cast<wchar_t*>(L""),Main},{nullptr,nullptr}};
    return StartServiceCtrlDispatcherW(table)?0:static_cast<int>(GetLastError());}
