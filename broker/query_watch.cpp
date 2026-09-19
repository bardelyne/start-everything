// Prints the query the panel publishes, as it changes.
//
// Read-only on purpose. The broker launches whatever is clicked, so it must
// never run elevated; this watcher opens nothing and can be run from any
// shell to check the query path on its own.
//
// Two things are being checked at once. The box on screen shows whatever the
// host has completed the text to; this prints what the panel decided the user
// actually typed. When someone types "n" and the box reads "nVIDIA App",
// this should still print "n".
#include <windows.h>

#include <cstdio>
#include <string>

#include "panel_protocol.h"

int wmain() {
    wprintf(L"watching for the panel...\n");
    fflush(stdout);

    HWND panel = nullptr;
    std::wstring last = L"\x01";  // not any real title
    bool announced = false;

    for (;;) {
        Sleep(40);

        if (!panel || !IsWindow(panel)) {
            panel = protocol::FindPanel();
            if (!panel) {
                if (announced) {
                    wprintf(L"panel gone; waiting for it to come back\n");
                    fflush(stdout);
                    announced = false;
                }
                continue;
            }
            DWORD pid = 0;
            GetWindowThreadProcessId(panel, &pid);
            wprintf(L"panel found: hwnd=%p pid=%lu\n", panel, pid);
            fflush(stdout);
            announced = true;
            last = L"\x01";
        }

        wchar_t buffer[512] = {};
        int n = GetWindowTextW(panel, buffer, ARRAYSIZE(buffer));
        std::wstring title(buffer, n > 0 ? static_cast<size_t>(n) : 0);
        if (title == last) {
            continue;
        }
        last = title;

        SYSTEMTIME st;
        GetLocalTime(&st);
        if (title == protocol::kWindowClass) {
            wprintf(L"%02d:%02d:%02d.%03d  (nothing published yet)\n", st.wHour,
                    st.wMinute, st.wSecond, st.wMilliseconds);
        } else if (title.empty()) {
            wprintf(L"%02d:%02d:%02d.%03d  query cleared\n", st.wHour,
                    st.wMinute, st.wSecond, st.wMilliseconds);
        } else {
            wprintf(L"%02d:%02d:%02d.%03d  typed: '%ls'\n", st.wHour,
                    st.wMinute, st.wSecond, st.wMilliseconds, title.c_str());
        }
        fflush(stdout);
    }
    return 0;
}
