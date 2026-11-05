// The production serializer against real private UTF-16 Windows INI storage.
// No Windhawk data or installed-app preferences are read or changed.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
static std::wstring file, legacy;
static bool writable = true;
static unsigned writes = 0, checks = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { printf("FAIL: %s\n", message); ExitProcess(1); }
}
size_t Wh_GetStringValue(PCWSTR key, wchar_t* buffer, size_t capacity) {
    if (!wcscmp(key, L"appLaunchCounts") && !legacy.empty()) {
        if (legacy.size() >= capacity) return 0;
        wcscpy(buffer, legacy.c_str());
        return legacy.size();
    }
    return GetPrivateProfileStringW(L"Storage", key, L"", buffer, DWORD(capacity), file.c_str());
}
BOOL Wh_SetStringValue(PCWSTR key, PCWSTR value) {
    ++writes;
    if (!writable || wcslen(value) > 32760 || wcspbrk(value, L"\r\n")) return FALSE;
    return WritePrivateProfileStringW(L"Storage", key, value, file.c_str());
}
BOOL Wh_DeleteValue(PCWSTR key) {
    if (!wcscmp(key, L"appLaunchCounts")) legacy.clear();
    return WritePrivateProfileStringW(L"Storage", key, nullptr, file.c_str());
}
// PRODUCTION_TEXT
namespace apps {
// PRODUCTION_USAGE
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    file = argv[1];
    HANDLE stream = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    Check(stream != INVALID_HANDLE_VALUE, "create a private INI fixture");
    const WORD bom = 0xfeff;
    DWORD written = 0;
    Check(WriteFile(stream, &bom, sizeof(bom), &written, nullptr) && written == sizeof(bom), "use UTF-16 INI storage");
    CloseHandle(stream);
    apps::UsageCounts counts{{L"word", 3}, {L"excel", 7}, {L"Программа", 12}, {L"Éditeur", 4},
                             {L"\x1f" L"command:日本語", 17}, {L"name\\with\nline\rbreak\x1e", 2},
                             {L" trailing space ", 9}};
    apps::SaveUsage(counts);
    auto restored = apps::LoadUsage();
    Check(writes == 1, "one bounded storage write per launch");
    for (auto const& [name, count] : counts)
        Check(restored[apps::ToLower(name)] == count, "names, Unicode, escapes and command namespaces survive the real INI round trip");
    Check(restored.size() == counts.size(), "record separators do not create phantom favorites");
    apps::ClearUsage();
    legacy = L"4\tПРОГРАММА\n7\tпрограмма\n2\tWord\n";
    restored = apps::LoadUsage();
    Check(restored[L"программа"] == 11 && restored[L"word"] == 2, "legacy Unicode keys merge without losing learned counts");
    writable = false;
    apps::SaveUsage(restored);
    Check(!legacy.empty() && apps::LoadUsage() == restored, "a failed migration keeps the original counts");
    writable = true;
    apps::SaveUsage(restored);
    Check(legacy.empty() && apps::LoadUsage() == restored, "successful migration retires the old newline value");
    counts.clear();
    for (int i = 0; i < 350; ++i) counts[L"program " + std::to_wstring(i)] = i + 1;
    apps::SaveUsage(counts);
    restored = apps::LoadUsage();
    Check(restored.size() == apps::kUsageKept && !restored.contains(L"program 0") && restored.contains(L"program 349"),
          "bounded storage keeps the most-used three hundred entries");
    counts.clear();
    for (int i = 0; i < 300; ++i) counts[std::wstring(1000, wchar_t(L'A' + i % 20)) + std::to_wstring(i)] = i + 1;
    apps::SaveUsage(counts);
    std::vector<wchar_t> buffer(65536);
    const auto length = Wh_GetStringValue(apps::kUsageSingleLineName, buffer.data(), buffer.size());
    Check(length <= 32760 && apps::LoadUsage().size() > 0, "oversized histories remain portable and parseable");
    Check(!wcspbrk(buffer.data(), L"\r\n"), "no portable value contains a line break");
    apps::SaveUsage({});
    Check(apps::LoadUsage().empty() && legacy.empty(), "turning learning off clears both current and legacy history");
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, file.c_str());
    Check(DeleteFileW(file.c_str()), "remove only the private INI fixture");
    printf("PASS: %u portable usage-storage checks\n", checks);
}
