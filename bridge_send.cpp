// Normal-integrity side of the bridge probe: finds the window the mod created
// inside the sandboxed search host and sends it a WM_COPYDATA.
#include <windows.h>
#include <cstdio>

int wmain() {
    const wchar_t* cls = L"WindhawkEverythingBridgeProbe";
    HWND target = FindWindowW(cls, nullptr);
    if (!target) {
        wprintf(L"FAIL: window '%ls' not found. Is the probe mod loaded in SearchHost?\n", cls);
        return 1;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(target, &pid);
    wprintf(L"found hwnd=%p owned by pid=%lu\n", target, pid);

    const wchar_t* payload = L"hello from a normal-integrity process";
    COPYDATASTRUCT cds{};
    cds.dwData = 0x45564254;  // 'EVBT'
    cds.cbData = static_cast<DWORD>((wcslen(payload) + 1) * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t*>(payload);

    SetLastError(0);
    LRESULT r = SendMessageW(target, WM_COPYDATA, 0,
                             reinterpret_cast<LPARAM>(&cds));
    DWORD err = GetLastError();
    wprintf(L"SendMessage(WM_COPYDATA) returned %lld, GetLastError=%lu\n",
            (long long)r, err);
    if (r) {
        wprintf(L"The receiver handled it. Check the mod's log to confirm the payload arrived.\n");
    } else {
        wprintf(L"Returned 0 -- likely blocked (UIPI/AppContainer) or not handled.\n");
    }
    return 0;
}
