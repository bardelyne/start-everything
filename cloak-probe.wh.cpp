// ==WindhawkMod==
// @id              cloak-probe
// @name            Cloak probe (temporary)
// @description     Finds out who cloaks the Start menu window when the search page takes over. Diagnostic only.
// @version         0.1
// @author          bardelyne
// @github          https://github.com/bardelyne
// @include         StartMenuExperienceHost.exe
// @architecture    x86-64
// @license         GPL-3.0
// @compilerOptions -ldwmapi
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Cloak probe

When the search page takes over, the Start menu's window is cloaked
(`DWMWA_CLOAKED` reports `DWM_CLOAKED_SHELL`). That flag says the shell did
it, but not *which code* called it -- and that matters: if the call comes from
inside `StartMenuExperienceHost`, a mod can hook and suppress it.

This hooks `DwmSetWindowAttribute` and records every cloak call along with the
module that made it. With `suppressCloak` on, it also refuses the call, so the
effect can be observed directly.

Output: `%TEMP%\cloak-probe.log`.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- suppressCloak: false
  $name: Refuse cloak requests
  $description: >-
    Log cloak calls and block them, instead of only logging. Turn on only to
    see what happens; it may leave the Start menu visible when it should not
    be.
*/
// ==/WindhawkModSettings==

#include <windows.h>

#include <dwmapi.h>
#include <windhawk_utils.h>

#include <atomic>

namespace {

bool g_suppress = false;

void Rec(const wchar_t* fmt, ...) {
    wchar_t body[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, ARRAYSIZE(body), _TRUNCATE, fmt, args);
    va_end(args);

    wchar_t path[MAX_PATH];
    DWORD n = GetTempPathW(MAX_PATH, path);
    if (!n || n > MAX_PATH - 32) {
        return;
    }
    wcscat_s(path, MAX_PATH, L"cloak-probe.log");

    HANDLE h = CreateFileW(path, FILE_APPEND_DATA,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    static const wchar_t kCrLf[] = {13, 10, 0};
    wchar_t line[1200];
    int len = wsprintfW(line, L"%02d:%02d:%02d.%03d  %ls%ls", st.wHour,
                        st.wMinute, st.wSecond, st.wMilliseconds, body, kCrLf);
    DWORD written = 0;
    WriteFile(h, line, static_cast<DWORD>(len * sizeof(wchar_t)), &written,
              nullptr);
    CloseHandle(h);
}

// Which module asked for this? That is the whole question -- if it is code in
// this process, a mod can intervene; if the call never appears at all, the
// cloak is being applied from outside and option C is genuinely closed.
void NameCaller(void* returnAddress, wchar_t* out, size_t chars) {
    out[0] = 0;
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(returnAddress), &mod) &&
        mod) {
        wchar_t full[MAX_PATH] = {};
        if (GetModuleFileNameW(mod, full, ARRAYSIZE(full))) {
            const wchar_t* leaf = wcsrchr(full, L'\\');
            wcscpy_s(out, chars, leaf ? leaf + 1 : full);
            return;
        }
    }
    wcscpy_s(out, chars, L"<unknown>");
}

using DwmSetWindowAttribute_t = decltype(&DwmSetWindowAttribute);
DwmSetWindowAttribute_t DwmSetWindowAttribute_Original;

HRESULT WINAPI DwmSetWindowAttribute_Hook(HWND hwnd, DWORD attribute,
                                          LPCVOID value, DWORD size) {
    if (attribute == DWMWA_CLOAK) {
        DWORD requested = (value && size >= sizeof(DWORD))
                              ? *static_cast<const DWORD*>(value)
                              : 0xFFFFFFFF;
        wchar_t caller[MAX_PATH];
        NameCaller(__builtin_return_address(0), caller, ARRAYSIZE(caller));

        wchar_t cls[128] = {};
        GetClassNameW(hwnd, cls, ARRAYSIZE(cls));

        Rec(L"DWMWA_CLOAK  hwnd=%p class='%ls'  value=%lu  caller=%ls%ls",
            hwnd, cls, requested, caller,
            g_suppress ? L"   [REFUSED]" : L"");

        if (g_suppress) {
            return S_OK;  // pretend it worked, do nothing
        }
    }
    return DwmSetWindowAttribute_Original(hwnd, attribute, value, size);
}

}  // namespace

void LoadSettings() {
    g_suppress = Wh_GetIntSetting(L"suppressCloak") != 0;
    Rec(L"settings: suppressCloak=%d", g_suppress ? 1 : 0);
}

BOOL Wh_ModInit() {
    Wh_Log(L">");
    LoadSettings();
    Rec(L"=== cloak probe attached, pid=%lu ===", GetCurrentProcessId());

    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) {
        Rec(L"FAIL: dwmapi.dll not loadable");
        return FALSE;
    }
    auto target = reinterpret_cast<DwmSetWindowAttribute_t>(
        GetProcAddress(dwm, "DwmSetWindowAttribute"));
    if (!target) {
        Rec(L"FAIL: DwmSetWindowAttribute not found");
        return FALSE;
    }
    if (!WindhawkUtils::Wh_SetFunctionHookT(target, DwmSetWindowAttribute_Hook,
                                            &DwmSetWindowAttribute_Original)) {
        Rec(L"FAIL: could not hook DwmSetWindowAttribute");
        return FALSE;
    }
    Rec(L"hooked DwmSetWindowAttribute; open Start and type to exercise it");
    return TRUE;
}

BOOL Wh_ModSettingsChanged(BOOL* bReload) {
    Wh_Log(L">");
    LoadSettings();
    *bReload = FALSE;
    return TRUE;
}

void Wh_ModUninit() {
    Wh_Log(L">");
    Rec(L"=== cloak probe detaching ===");
}
