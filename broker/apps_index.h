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

#include <algorithm>
#include <memory>
#include <mutex>
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
    std::wstring nameLower;  // precomputed, so filtering never allocates
    // An absolute PIDL rather than a parsing string. The parsing names come
    // in several shapes -- plain names, full paths, UWP AUMIDs,
    // known-folder GUIDs, even URLs -- and rebuilding a path by prefixing
    // "shell:AppsFolder\" fails outright for the GUID-relative ones. Going
    // through the item's own identity works for all of them: it produced
    // icons for 187 of 187 apps where string re-parsing did not.
    UniquePidl pidl;
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

class Index {
   public:
    Index() = default;
    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;

    // Enumerates names and identities only. Deliberately does not touch
    // icons: those cost ~9.7 ms each and would turn a 200 ms startup into
    // nearly two seconds.
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
            ITEMIDLIST* pidl = nullptr;
            if (SUCCEEDED(SHGetIDListFromObject(item, &pidl)) && pidl) {
                a.pidl.reset(pidl);
            }
            if (!a.name.empty() && a.pidl) {
                a.nameLower = ToLower(a.name);
                fresh.push_back(std::move(a));
            }
            item->Release();
            item = nullptr;
        }
        e->Release();

        std::lock_guard<std::mutex> lock(mutex_);
        apps_ = std::move(fresh);
        return true;
    }

    size_t Count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return apps_.size();
    }

    // In-memory filter. Runs per keystroke, so it must stay cheap: a prefix
    // match sorts above a word-start match, which sorts above a substring.
    std::vector<Match> Search(const std::wstring& query, size_t limit) const {
        std::vector<Match> hits;
        if (query.empty()) {
            return hits;
        }
        const std::wstring q = ToLower(query);

        std::lock_guard<std::mutex> lock(mutex_);
        for (const App& a : apps_) {
            size_t pos = a.nameLower.find(q);
            if (pos == std::wstring::npos) {
                continue;
            }
            int score;
            if (pos == 0) {
                score = 0;  // starts with the query
            } else if (a.nameLower[pos - 1] == L' ') {
                score = 1;  // starts a word
            } else {
                score = 2;  // somewhere inside
            }
            hits.push_back({&a, score});
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

    // Icons are fetched on demand, for visible rows only. Caller owns the
    // bitmap.
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

    // Launching goes through the item's identity too, for the same reason
    // icons do.
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
