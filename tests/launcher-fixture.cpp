#include <windows.h>
#include <vector>

#ifdef LAUNCHER_RESOURCE_WRITER
int wmain(int argc,wchar_t** argv) {
    if(argc!=3)return 2;
    const auto file=CreateFileW(argv[2],GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return 3;
    const auto length=GetFileSize(file,nullptr);std::vector<BYTE> bytes(length<=65536?length:0);DWORD got=0;
    const bool read=!bytes.empty() && ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&got,nullptr) && got==bytes.size();
    CloseHandle(file);if(!read)return 4;
    const auto update=BeginUpdateResourceW(argv[1],FALSE);if(!update)return 5;
    if(!UpdateResourceW(update,MAKEINTRESOURCEW(16),MAKEINTRESOURCEW(1),MAKELANGID(LANG_NEUTRAL,SUBLANG_NEUTRAL),bytes.data(),static_cast<DWORD>(bytes.size()))) {
        EndUpdateResourceW(update,TRUE);return 6;
    }
    return EndUpdateResourceW(update,FALSE)?0:7;
}
#elif defined(LAUNCHER_CONSOLE_FIXTURE)
int main(){return 0;}
#else
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){return 0;}
#endif
