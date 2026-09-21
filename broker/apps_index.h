// An index of installed applications, from the shell's AppsFolder.
//
// AppsFolder is the virtual folder that unions Win32 shortcuts and Store
// apps, so one enumeration covers both. Measured on a real machine: 187 apps,
// ~200 ms to enumerate names but ~9.7 ms per icon -- which is why names are
// enumerated once up front and icons are fetched lazily, for visible rows
// only.
//
// This must run at normal integrity: SearchHost is a Low-IL AppContainer with
// no shell namespace access, so the index lives in the broker and results are
// pushed to the panel.

#pragma once

#include <windows.h>

#include <shlobj.h>
#include <shobjidl.h>

#include "settings_data.h"

#include <algorithm>
#include <cwctype>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace apps {

namespace detail {

struct PidlDeleter {
    void operator()(ITEMIDLIST* p) const noexcept {
        if (p) {
            CoTaskMemFree(p);
        }
    }
};

}  // namespace detail

using UniquePidl = std::unique_ptr<ITEMIDLIST, detail::PidlDeleter>;

struct App {
    std::wstring name;
    std::wstring nameLower;        // precomputed, so filtering never allocates
    std::wstring targetPath;       // file path, AUMID, or ms-settings: URI
    std::wstring targetPathLower;  // precomputed lowercase target path
    std::wstring exeNameLower;     // executable / command name (e.g. "cmd", "wt", "calc")
    std::wstring acronym;          // acronym from name words (e.g. "cp" for Command Prompt)
    std::vector<std::wstring> words;   // individual words in name
    std::vector<std::wstring> aliases; // smart aliases / keywords
    UniquePidl pidl;
    bool isSetting = false;        // true if Windows Setting or Control Panel page
    std::wstring area;             // Category/Area (e.g. "System", "Devices", "Personalization")
};

struct Match {
    const App* app;
    int score;  // lower is better
};

inline std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

inline std::wstring ResolveAppParsingPath(const std::wstring& parse) {
    if (parse.empty()) return L"";
    if (parse.front() == L'{') {
        size_t closing = parse.find(L'}');
        if (closing != std::wstring::npos) {
            std::wstring guidStr = parse.substr(0, closing + 1);
            GUID guid{};
            if (SUCCEEDED(IIDFromString(guidStr.c_str(), &guid))) {
                PWSTR kfPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(guid, 0, NULL, &kfPath)) && kfPath) {
                    std::wstring resolved = kfPath;
                    CoTaskMemFree(kfPath);
                    if (closing + 1 < parse.size()) {
                        resolved += parse.substr(closing + 1);
                    }
                    return resolved;
                }
            }
        }
    }
    return parse;
}

inline std::wstring ResolveLnkTarget(const std::wstring& lnkPath) {
    if (lnkPath.size() < 4) return L"";
    std::wstring ext = lnkPath.substr(lnkPath.size() - 4);
    for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));
    if (ext != L".lnk") return L"";

    IShellLinkW* psl = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&psl)))) {
        return L"";
    }
    IPersistFile* ppf = nullptr;
    std::wstring result;
    if (SUCCEEDED(psl->QueryInterface(IID_PPV_ARGS(&ppf)))) {
        if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
            wchar_t szTarget[MAX_PATH] = {};
            if (SUCCEEDED(psl->GetPath(szTarget, MAX_PATH, nullptr, 0)) && szTarget[0]) {
                result = szTarget;
            }
        }
        ppf->Release();
    }
    psl->Release();
    return result;
}

inline std::wstring ExtractExeName(const std::wstring& path) {
    if (path.empty()) return L"";
    if (path.starts_with(L"ms-settings:")) {
        std::wstring sub = path.substr(12);
        size_t q = sub.find_first_of(L"?#");
        if (q != std::wstring::npos) sub = sub.substr(0, q);
        return sub;
    }
    if (path.starts_with(L"control ")) {
        return L"control";
    }
    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring filename = (lastSlash != std::wstring::npos) ? path.substr(lastSlash + 1) : path;
    size_t dot = filename.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        return filename.substr(0, dot);
    }
    size_t bang = filename.find_last_of(L'!');
    if (bang != std::wstring::npos) {
        size_t underscore = filename.find_first_of(L'_');
        if (underscore != std::wstring::npos) {
            std::wstring pkg = filename.substr(0, underscore);
            size_t dotInPkg = pkg.find_last_of(L'.');
            if (dotInPkg != std::wstring::npos) {
                return pkg.substr(dotInPkg + 1);
            }
            return pkg;
        }
    }
    return filename;
}

inline void PopulateAppAliases(App& a) {
    if (a.exeNameLower.empty()) {
        if (a.nameLower == L"command prompt") a.exeNameLower = L"cmd";
        else if (a.nameLower.find(L"powershell") != std::wstring::npos) a.exeNameLower = L"powershell";
        else if (a.nameLower == L"terminal" || a.nameLower == L"windows terminal") a.exeNameLower = L"wt";
        else if (a.nameLower == L"task manager") a.exeNameLower = L"taskmgr";
        else if (a.nameLower == L"registry editor") a.exeNameLower = L"regedit";
        else if (a.nameLower == L"calculator") a.exeNameLower = L"calc";
        else if (a.nameLower == L"control panel") a.exeNameLower = L"control";
        else if (a.nameLower == L"file explorer") a.exeNameLower = L"explorer";
        else if (a.nameLower == L"notepad") a.exeNameLower = L"notepad";
        else if (a.nameLower == L"paint") a.exeNameLower = L"mspaint";
        else if (a.nameLower.find(L"snipping") != std::wstring::npos) a.exeNameLower = L"snippingtool";
        else if (a.nameLower.find(L"remote desktop") != std::wstring::npos) a.exeNameLower = L"mstsc";
        else if (a.nameLower.find(L"visual studio code") != std::wstring::npos) a.exeNameLower = L"code";
    }

    if (a.targetPath.empty()) {
        if (a.exeNameLower == L"cmd") a.targetPath = L"C:\\Windows\\System32\\cmd.exe";
        else if (a.exeNameLower == L"powershell") a.targetPath = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe";
        else if (a.exeNameLower == L"taskmgr") a.targetPath = L"C:\\Windows\\System32\\Taskmgr.exe";
        else if (a.exeNameLower == L"regedit") a.targetPath = L"C:\\Windows\\regedit.exe";
        else if (a.exeNameLower == L"control") a.targetPath = L"C:\\Windows\\System32\\control.exe";
        else if (a.exeNameLower == L"explorer") a.targetPath = L"C:\\Windows\\explorer.exe";
        else if (a.exeNameLower == L"notepad") a.targetPath = L"C:\\Windows\\System32\\notepad.exe";
        else if (a.exeNameLower == L"mspaint") a.targetPath = L"C:\\Windows\\System32\\mspaint.exe";
    }

    if (!a.exeNameLower.empty() && a.exeNameLower != a.nameLower) {
        a.aliases.push_back(a.exeNameLower);
    }

    if (a.exeNameLower == L"cmd" || a.nameLower == L"command prompt") {
        a.aliases.insert(a.aliases.end(), {L"cmd", L"cmd.exe", L"command", L"prompt", L"terminal", L"console", L"cli", L"shell", L"dos"});
    } else if (a.exeNameLower == L"powershell" || a.nameLower.find(L"powershell") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"powershell", L"pwsh", L"ps", L"posh", L"shell", L"terminal", L"console"});
    } else if (a.nameLower == L"terminal" || a.targetPathLower.find(L"windowsterminal") != std::wstring::npos || a.exeNameLower == L"wt") {
        a.aliases.insert(a.aliases.end(), {L"wt", L"wt.exe", L"terminal", L"windows terminal", L"bash", L"cmd", L"powershell", L"console"});
    } else if (a.exeNameLower == L"taskmgr" || a.nameLower == L"task manager") {
        a.aliases.insert(a.aliases.end(), {L"taskmgr", L"taskmgr.exe", L"task", L"tasks", L"process", L"processes", L"kill", L"performance"});
    } else if (a.exeNameLower == L"regedit" || a.nameLower == L"registry editor") {
        a.aliases.insert(a.aliases.end(), {L"regedit", L"regedit.exe", L"reg", L"registry"});
    } else if (a.exeNameLower == L"calc" || a.nameLower == L"calculator") {
        a.aliases.insert(a.aliases.end(), {L"calc", L"calc.exe", L"calculator", L"math"});
    } else if (a.exeNameLower == L"control" || a.nameLower == L"control panel") {
        a.aliases.insert(a.aliases.end(), {L"control", L"control.exe", L"control panel", L"cpl", L"settings"});
    } else if (a.exeNameLower == L"explorer" || a.nameLower == L"file explorer") {
        a.aliases.insert(a.aliases.end(), {L"explorer", L"explorer.exe", L"files", L"my pc", L"this pc"});
    } else if (a.exeNameLower == L"notepad" || a.nameLower == L"notepad") {
        a.aliases.insert(a.aliases.end(), {L"notepad", L"notepad.exe", L"text", L"editor", L"notes", L"txt"});
    } else if (a.exeNameLower == L"mspaint" || a.nameLower == L"paint") {
        a.aliases.insert(a.aliases.end(), {L"mspaint", L"paint", L"pbrush", L"draw", L"sketch"});
    } else if (a.nameLower.find(L"snipping") != std::wstring::npos || a.targetPathLower.find(L"snippingtool") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"snippingtool", L"snip", L"screenshot", L"capture", L"screen"});
    } else if (a.exeNameLower == L"mstsc" || a.nameLower.find(L"remote desktop") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"mstsc", L"mstsc.exe", L"rdp", L"remote"});
    } else if (a.exeNameLower == L"code" || a.nameLower.find(L"visual studio code") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"code", L"code.exe", L"vscode", L"vs code", L"ide", L"editor"});
    } else if (a.nameLower.find(L"device manager") != std::wstring::npos || a.targetPathLower.find(L"devmgmt") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"devmgmt", L"devmgmt.msc", L"device manager", L"devices"});
    } else if (a.nameLower.find(L"disk management") != std::wstring::npos || a.targetPathLower.find(L"diskmgmt") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"diskmgmt", L"diskmgmt.msc", L"partition", L"disk"});
    } else if (a.nameLower == L"services") {
        a.aliases.insert(a.aliases.end(), {L"services", L"services.msc"});
    } else if (a.nameLower.find(L"event viewer") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"eventvwr", L"eventvwr.msc", L"event viewer", L"logs"});
    } else if (a.nameLower.find(L"system configuration") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"msconfig", L"msconfig.exe"});
    } else if (a.nameLower.find(L"system information") != std::wstring::npos) {
        a.aliases.insert(a.aliases.end(), {L"msinfo32", L"msinfo"});
    } else if (a.nameLower == L"settings") {
        a.aliases.insert(a.aliases.end(), {L"settings", L"control", L"preferences", L"config"});
    }
}

inline int DamerauLevenshteinDistance(const std::wstring& s1, const std::wstring& s2, int maxDist = 2) {
    int len1 = static_cast<int>(s1.size());
    int len2 = static_cast<int>(s2.size());
    if (std::abs(len1 - len2) > maxDist) return maxDist + 1;
    if (len1 == 0) return len2;
    if (len2 == 0) return len1;

    constexpr int kMaxDim = 40;
    if (len1 >= kMaxDim || len2 >= kMaxDim) return maxDist + 1;

    int d[kMaxDim + 1][kMaxDim + 1];
    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;

    for (int i = 1; i <= len1; ++i) {
        int rowMin = 999;
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({
                d[i - 1][j] + 1,       // deletion
                d[i][j - 1] + 1,       // insertion
                d[i - 1][j - 1] + cost // substitution
            });
            // Transposition
            if (i > 1 && j > 1 && s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1]) {
                d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
            }
            if (d[i][j] < rowMin) rowMin = d[i][j];
        }
        if (rowMin > maxDist) return maxDist + 1;
    }
    return d[len1][len2];
}

inline int ScoreApp(const App& app, const std::wstring& q) {
    if (q.empty()) return -1;
    int s = -1;

    // 1. Exact Name match
    if (app.nameLower == q) {
        s = 0;
    }
    // 2. Exact Exe Name match (e.g. q == "cmd")
    else if (!app.exeNameLower.empty() && app.exeNameLower == q) {
        s = 2;
    }
    // 3. Exact Alias match (e.g. q == "cmd", "wifi", "calc")
    else {
        for (const auto& al : app.aliases) {
            if (al == q) {
                s = 4;
                break;
            }
        }
    }

    // 4. Name starts with query
    if (s < 0) {
        if (app.nameLower.starts_with(q)) {
            s = 10;
        }
        // 5. Exe Name starts with query
        else if (!app.exeNameLower.empty() && app.exeNameLower.starts_with(q)) {
            s = 15;
        }
        // 6. Word in Name starts with query
        else {
            for (const auto& w : app.words) {
                if (w.starts_with(q)) {
                    s = 20;
                    break;
                }
            }
            // 7. Alias starts with query
            if (s < 0) {
                for (const auto& al : app.aliases) {
                    if (al.starts_with(q)) {
                        s = 25;
                        break;
                    }
                }
            }
        }
    }

    // 8. Exact Acronym match (e.g. "cp" -> "Command Prompt")
    if (s < 0) {
        if (!app.acronym.empty() && app.acronym == q) {
            s = 30;
        }
        // 9. Substring in Name
        else {
            size_t namePos = app.nameLower.find(q);
            if (namePos != std::wstring::npos) {
                s = 40 + static_cast<int>(std::min<size_t>(namePos, 20));
            }
            // 10. Substring in Exe / Target Path
            else if (!app.targetPathLower.empty() && app.targetPathLower.find(q) != std::wstring::npos) {
                s = 60;
            }
            // 11. Substring in any alias
            else {
                for (const auto& al : app.aliases) {
                    if (al.find(q) != std::wstring::npos) {
                        s = 70;
                        break;
                    }
                }
            }
        }
    }

    // 12. Fuzzy subsequence match on Name or Exe Name
    if (s < 0) {
        auto isSubsequence = [](const std::wstring& pattern, const std::wstring& text, int& gaps) -> bool {
            if (pattern.size() > text.size()) return false;
            size_t p = 0;
            size_t lastMatch = 0;
            gaps = 0;
            for (size_t t = 0; t < text.size() && p < pattern.size(); ++t) {
                if (pattern[p] == text[t]) {
                    if (p > 0) gaps += static_cast<int>(t - lastMatch - 1);
                    lastMatch = t;
                    ++p;
                }
            }
            return p == pattern.size();
        };

        int gaps = 0;
        if (q.size() >= 2 && isSubsequence(q, app.nameLower, gaps)) {
            s = 80 + std::min(gaps, 30);
        } else if (q.size() >= 2 && !app.exeNameLower.empty() && isSubsequence(q, app.exeNameLower, gaps)) {
            s = 85 + std::min(gaps, 30);
        }
    }

    // 13. Typo-tolerant edit distance matching
    if (s < 0 && q.size() >= 3) {
        int maxDist = (q.size() <= 4) ? 1 : 2;
        int bestDist = 999;

        auto checkCandidate = [&](const std::wstring& cand) {
            if (cand.empty()) return;
            // Whole string distance
            int d = DamerauLevenshteinDistance(q, cand, maxDist);
            if (d <= maxDist && d < bestDist) {
                bestDist = d;
            }
            // Prefix distance if candidate is longer
            if (cand.size() > q.size()) {
                std::wstring candPfx = cand.substr(0, q.size());
                int pfxD = DamerauLevenshteinDistance(q, candPfx, maxDist);
                if (pfxD <= maxDist && pfxD < bestDist) {
                    bestDist = pfxD;
                }
                if (cand.size() > q.size() + 1) {
                    std::wstring candPfx1 = cand.substr(0, q.size() + 1);
                    int pfxD1 = DamerauLevenshteinDistance(q, candPfx1, maxDist);
                    if (pfxD1 <= maxDist && pfxD1 < bestDist) {
                        bestDist = pfxD1;
                    }
                }
            }
        };

        checkCandidate(app.nameLower);
        if (!app.exeNameLower.empty()) {
            checkCandidate(app.exeNameLower);
        }
        for (const auto& w : app.words) {
            checkCandidate(w);
        }
        for (const auto& al : app.aliases) {
            checkCandidate(al);
        }

        if (bestDist <= maxDist) {
            s = 120 + bestDist * 15;
        }
    }

    if (s >= 0 && app.isSetting) {
        s += 1; // tie-breaker: slightly prefer real apps on exact ties
    }
    return s;
}

class Index {
   public:
    Index() = default;
    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;

    bool Rebuild() {
        std::vector<App> fresh;

        IShellItem* folder = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(L"shell:AppsFolder", nullptr,
                                                 IID_PPV_ARGS(&folder));
        if (FAILED(hr) || !folder) {
            return false;
        }

        IEnumShellItems* e = nullptr;
        hr = folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(&e));
        folder->Release();
        if (FAILED(hr) || !e) {
            return false;
        }

        IShellItem* item = nullptr;
        while (e->Next(1, &item, nullptr) == S_OK && item) {
            App a;
            LPWSTR display = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_NORMALDISPLAY, &display)) &&
                display) {
                a.name = display;
                CoTaskMemFree(display);
            }
            LPWSTR fsPath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &fsPath)) && fsPath) {
                std::wstring resolved = ResolveLnkTarget(fsPath);
                if (!resolved.empty()) {
                    a.targetPath = resolved;
                } else {
                    a.targetPath = fsPath;
                }
                CoTaskMemFree(fsPath);
            }
            
            LPWSTR parse = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &parse)) && parse) {
                std::wstring resolvedParse = ResolveAppParsingPath(parse);
                if (a.targetPath.empty()) {
                    a.targetPath = resolvedParse;
                } else if (a.targetPath.size() >= 4 && 
                           ToLower(a.targetPath.substr(a.targetPath.size() - 4)) == L".lnk" &&
                           !resolvedParse.empty() && resolvedParse.front() != L'{') {
                    a.targetPath = resolvedParse;
                }
                CoTaskMemFree(parse);
            }

            ITEMIDLIST* pidl = nullptr;
            if (SUCCEEDED(SHGetIDListFromObject(item, &pidl)) && pidl) {
                a.pidl.reset(pidl);
            }
            if (!a.name.empty() && a.pidl) {
                a.nameLower = ToLower(a.name);
                a.targetPathLower = ToLower(a.targetPath);
                a.exeNameLower = ToLower(ExtractExeName(a.targetPath));

                std::wistringstream ss(a.nameLower);
                std::wstring word;
                while (ss >> word) {
                    while (!word.empty() && iswpunct(word.front())) word.erase(word.begin());
                    while (!word.empty() && iswpunct(word.back())) word.pop_back();
                    if (!word.empty()) {
                        a.words.push_back(word);
                        a.acronym.push_back(word[0]);
                    }
                }

                PopulateAppAliases(a);
                fresh.push_back(std::move(a));
            }
            item->Release();
            item = nullptr;
        }
        e->Release();

        // Append Windows Settings
        for (const auto& s : settings::GetSettingsList()) {
            App a;
            a.name = s.name;
            a.targetPath = s.command;
            a.area = s.area;
            a.isSetting = true;
            a.nameLower = ToLower(a.name);
            a.targetPathLower = ToLower(a.targetPath);
            a.exeNameLower = ToLower(ExtractExeName(a.targetPath));

            std::wistringstream ss(a.nameLower);
            std::wstring word;
            while (ss >> word) {
                while (!word.empty() && iswpunct(word.front())) word.erase(word.begin());
                while (!word.empty() && iswpunct(word.back())) word.pop_back();
                if (!word.empty()) {
                    a.words.push_back(word);
                    a.acronym.push_back(word[0]);
                }
            }

            if (s.altNames && s.altNames[0]) {
                std::wistringstream alts(s.altNames);
                std::wstring alt;
                while (std::getline(alts, alt, L';')) {
                    if (!alt.empty()) {
                        a.aliases.push_back(ToLower(alt));
                    }
                }
            }
            if (!a.area.empty()) {
                a.aliases.push_back(ToLower(a.area));
            }

            fresh.push_back(std::move(a));
        }

        std::lock_guard<std::mutex> lock(mutex_);
        apps_ = std::move(fresh);
        return true;
    }

    size_t Count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return apps_.size();
    }

    std::vector<Match> Search(const std::wstring& query, size_t limit) const {
        std::vector<Match> hits;
        if (query.empty()) {
            return hits;
        }
        const std::wstring q = ToLower(query);

        std::lock_guard<std::mutex> lock(mutex_);
        for (const App& a : apps_) {
            int score = ScoreApp(a, q);
            if (score >= 0) {
                hits.push_back({&a, score});
            }
        }

        std::stable_sort(hits.begin(), hits.end(),
                         [](const Match& x, const Match& y) {
                             if (x.score != y.score) {
                                 return x.score < y.score;
                             }
                             return x.app->name.size() < y.app->name.size();
                         });
        if (hits.size() > limit) {
            hits.resize(limit);
        }
        return hits;
    }

    static HBITMAP LoadIcon(const App& app, int size) {
        if (!app.pidl) {
            return nullptr;
        }
        IShellItem* item = nullptr;
        if (FAILED(SHCreateItemFromIDList(app.pidl.get(), IID_PPV_ARGS(&item))) ||
            !item) {
            return nullptr;
        }
        IShellItemImageFactory* factory = nullptr;
        HBITMAP bitmap = nullptr;
        if (SUCCEEDED(item->QueryInterface(IID_PPV_ARGS(&factory))) && factory) {
            SIZE s{size, size};
            if (FAILED(factory->GetImage(s, SIIGBF_ICONONLY, &bitmap))) {
                bitmap = nullptr;
            }
            factory->Release();
        }
        item->Release();
        return bitmap;
    }

    static bool Launch(const App& app) {
        if (!app.pidl) {
            return false;
        }
        SHELLEXECUTEINFOW ei{};
        ei.cbSize = sizeof(ei);
        ei.fMask = SEE_MASK_IDLIST | SEE_MASK_FLAG_NO_UI;
        ei.lpIDList = app.pidl.get();
        ei.lpVerb = L"open";
        ei.nShow = SW_SHOWNORMAL;
        return ShellExecuteExW(&ei) != FALSE;
    }

   private:
    mutable std::mutex mutex_;
    std::vector<App> apps_;
};

}  // namespace apps
