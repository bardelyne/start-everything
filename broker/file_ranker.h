// Relevance ordering for Everything results.
//
// Everything is asked for results sorted by name ascending, because that is
// the only sort it guarantees is instant -- every other sort is fast only if
// the user happens to have the matching fast-sort enabled in Tools ->
// Options -> Indexes, which a mod cannot assume. Name-ascending is useless as
// a presentation order, though: a search for "code" leads with
// "-abstract-code-quality-task" out of a Gradle docs folder.
//
// So the client over-fetches a pool and this reorders it.
//
// The ordering is a lexicographic key rather than a sum of weights. A sum
// reads naturally but behaves badly here: the first attempt added small
// bonuses for recency and run count on top of a match score, and because
// thousands of results tie on the match score and most have a run count of
// zero, the top eight for "code" came back as eight different folders all
// literally named "Code", ordered by nothing meaningful. A key makes the
// tie-breaks explicit and total: how well the name matched, then how often
// the user has opened it, then how recently it changed.
//
// Known limitation: the pool is the alphabetically first N matches, so
// ranking can only reorder what that window happened to catch. A very
// recently edited file whose name sorts late loses to worse matches that sort
// early, and for a query with thousands of hits the window is a small
// fraction of them. Fixing it properly needs a sort Everything can do
// server-side, which means date-modified-descending -- fast only when the
// user has that fast-sort enabled, and silently slow when they have not. The
// honest options are to keep this, or to probe the cost of the date sort once
// at startup and use it when it turns out to be cheap. Not decided yet.

#pragma once

#include <windows.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

#include "everything_ipc.h"

namespace ranker {

// How many results to ask Everything for before ranking. The pool has to be
// much larger than what is displayed or ranking has nothing to work with: at
// a pool of 10, a name-ascending query for "code" only ever sees names
// starting with punctuation.
//
// Measured on this machine (best of 3, ms, blocking wait):
//
//   query          total     8    50   200   300   500  1000
//   c             589945  11.3  11.7  16.6  48.6  65.7 145.6
//   code            8365  30.1  28.4  32.8  39.3  44.3  51.4
//   readme          2434  29.6  28.0  32.8  37.6  39.2  50.3
//   (no matches)       0  22.6  23.0  23.5  27.1  24.6  25.2
//
// So about 23 ms is a fixed cost inside Everything that no pool size avoids,
// and 200 is the largest pool that stays close to it for every query
// including a single letter. 300 is already 48 ms on "c" and 1000 is far too
// slow to run per keystroke.
inline constexpr DWORD kDefaultPool = 200;

inline std::wstring ToLower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return s;
}

namespace detail {

// Match quality, coarse buckets, lower is better. This is the primary key and
// the only signal available when Everything is too old for QUERY2.
enum MatchClass : int {
    kExactName = 0,
    kNamePrefix = 1,
    kNameWordStart = 2,
    kNameSubstring = 3,
    kPathOnly = 4,
};

inline int Classify(const std::wstring& nameLower, const std::wstring& q) {
    size_t pos = nameLower.find(q);
    if (pos == std::wstring::npos) {
        return kPathOnly;
    }
    if (pos == 0) {
        return nameLower.size() == q.size() ? kExactName : kNamePrefix;
    }
    wchar_t prev = nameLower[pos - 1];
    bool wordStart = prev == L' ' || prev == L'-' || prev == L'_' ||
                     prev == L'.' || prev == L'(' || prev == L'[';
    return wordStart ? kNameWordStart : kNameSubstring;
}

// Results out of package caches, build outputs and version-control internals
// swamp everything else on a developer machine: the first unranked page for
// "code" was eight Gradle doc stubs. Demoted a whole match class rather than
// hidden, so they still show up once the better matches run out.
inline bool IsNoise(const std::wstring& pathLower) {
    static const wchar_t* kNoisy[] = {
        L"\\node_modules\\",
        L"\\.git\\",
        L"\\.gradle\\",
        L"\\appdata\\local\\temp\\",
        L"\\appdata\\local\\packages\\",
        L"\\__pycache__\\",
        L"\\.venv\\",
        L"\\site-packages\\",
        L"\\.cache\\",
        L"\\build\\intermediates\\",
        L"\\obj\\debug\\",
        L"\\obj\\release\\",
        L"\\windows\\winsxs\\",
        L"\\windows\\servicing\\",
    };
    for (const wchar_t* n : kNoisy) {
        if (pathLower.find(n) != std::wstring::npos) {
            return true;
        }
    }
    return false;
}

inline ULONGLONG AsTicks(const FILETIME& ft) {
    ULARGE_INTEGER v{};
    v.LowPart = ft.dwLowDateTime;
    v.HighPart = ft.dwHighDateTime;
    return v.QuadPart;
}

// Sort key. Descending fields are stored pre-negated so the comparison is a
// plain ascending tuple compare -- writing the descending fields by swapping
// this and other inside std::tie does work, but it is the kind of expression
// nobody can check at a glance.
struct Key {
    int matchClass;         // ascending: better match first
    ULONGLONG runsDesc;     // negated run count: more opens first
    ULONGLONG modifiedDesc; // negated timestamp: more recent first
    size_t index;           // keeps the order total and reproducible

    bool operator<(const Key& o) const {
        return std::tie(matchClass, runsDesc, modifiedDesc, index) <
               std::tie(o.matchClass, o.runsDesc, o.modifiedDesc, o.index);
    }
};

// Turns a "higher is better" value into a sort key.
inline ULONGLONG Descending(ULONGLONG v) {
    return ~0ull - v;
}

}  // namespace detail

// Reorders a pool in place and truncates it to limit.
inline void Rank(std::vector<everything::Result>* pool,
                 const std::wstring& query, size_t limit) {
    if (!pool || pool->empty()) {
        return;
    }
    const std::wstring q = ToLower(query);

    std::vector<detail::Key> keys;
    keys.reserve(pool->size());
    for (size_t i = 0; i < pool->size(); i++) {
        const everything::Result& r = (*pool)[i];
        int cls = detail::Classify(ToLower(r.name), q);
        // A noisy location costs a whole class, so an exact name match buried
        // in node_modules still loses to a plain prefix match somewhere real.
        if (detail::IsNoise(ToLower(r.path))) {
            cls += detail::kPathOnly + 1;
        }
        // Everything only counts opens that went through Everything itself,
        // so this is sparse -- but where it is set it is the strongest
        // evidence available that the user wants this particular file. Capped
        // so one heavily used file cannot dominate a whole class.
        ULONGLONG runs = r.runCount > 50 ? 50 : r.runCount;
        keys.push_back({cls, detail::Descending(runs),
                        detail::Descending(detail::AsTicks(r.dateModified)), i});
    }

    std::sort(keys.begin(), keys.end());

    std::vector<everything::Result> ranked;
    ranked.reserve(keys.size() < limit ? keys.size() : limit);
    for (size_t i = 0; i < keys.size() && ranked.size() < limit; i++) {
        ranked.push_back(std::move((*pool)[keys[i].index]));
    }
    *pool = std::move(ranked);
}

}  // namespace ranker
