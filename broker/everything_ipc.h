// Everything IPC client.
//
// Everything exposes a hidden window that answers WM_COPYDATA queries. This
// is the only transport worth shipping: the HTTP server is off by default and
// the SDK DLL would have to be redistributed, whereas the IPC window is there
// whenever Everything is running.
//
// Two query messages exist. QUERY2 is preferred: it carries a sort order and
// returns size, dates, attributes and run count, which the ranker needs.
// QUERYW is the original and is kept as a fallback for older builds -- it
// returns names and paths only, in whatever order the Everything window
// happens to be sorted by.
//
// The struct layouts were taken from the official everything_ipc.h shipped
// with the "es" client, then confirmed against a live reply, because that
// header documents the QUERY2 field order in a sequence that does not match
// the flag bit order. They are ABI: a wrong offset reads arbitrary bytes out
// of a buffer another process filled in, so every read here is bounds-checked
// rather than trusted.

#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace everything {

// Window class of Everything's IPC listener.
inline constexpr wchar_t kIpcWindowClass[] = L"EVERYTHING_TASKBAR_NOTIFICATION";

// WM_COPYDATA dwData values for the two query messages.
inline constexpr DWORD kCopyDataQueryW = 2;
inline constexpr DWORD kCopyDataQuery2W = 18;

// Search flags. Worth spelling out rather than trusting memory: match-path is
// 0x4, while 0x2 is match-whole-word. Confusing the two does not fail, it
// silently changes what every query means.
inline constexpr DWORD kMatchCase = 0x00000001;
inline constexpr DWORD kMatchWholeWord = 0x00000002;
inline constexpr DWORD kMatchPath = 0x00000004;
inline constexpr DWORD kRegex = 0x00000008;

// max_results sentinel.
inline constexpr DWORD kAllResults = 0xFFFFFFFF;

// Item flags in a reply.
inline constexpr DWORD kItemFolder = 0x00000001;
inline constexpr DWORD kItemDrive = 0x00000002;

// QUERY2 request flags. Only the subset below is supported, and that is
// deliberate: the official header lists the fields inside the data blob in an
// order that disagrees with their bit order for EXTENSION, TYPE_NAME and the
// highlighted variants. For every flag named here the two orders agree, so
// the blob can be walked in ascending bit order without guessing. Adding one
// of the others means re-establishing the order against a live reply first.
inline constexpr DWORD kReqName = 0x00000001;
inline constexpr DWORD kReqPath = 0x00000002;
inline constexpr DWORD kReqFullPath = 0x00000004;
inline constexpr DWORD kReqSize = 0x00000010;
inline constexpr DWORD kReqDateCreated = 0x00000020;
inline constexpr DWORD kReqDateModified = 0x00000040;
inline constexpr DWORD kReqDateAccessed = 0x00000080;
inline constexpr DWORD kReqAttributes = 0x00000100;
inline constexpr DWORD kReqRunCount = 0x00000400;
inline constexpr DWORD kReqDateRun = 0x00000800;

inline constexpr DWORD kReqSupported =
    kReqName | kReqPath | kReqFullPath | kReqSize | kReqDateCreated |
    kReqDateModified | kReqDateAccessed | kReqAttributes | kReqRunCount |
    kReqDateRun;

// What the panel shows, plus what the ranker sorts on.
inline constexpr DWORD kDefaultRequestFlags = kReqName | kReqPath | kReqSize |
                                              kReqDateModified |
                                              kReqAttributes | kReqRunCount;

// Sort orders. Name-ascending is the only one guaranteed instant; the others
// are fast only when the matching fast-sort is enabled in Everything's
// options, which cannot be assumed on a user machine. So the client asks for
// name-ascending and the ranking happens here.
inline constexpr DWORD kSortNameAscending = 1;
inline constexpr DWORD kSortDateModifiedDescending = 14;
inline constexpr DWORD kSortRunCountDescending = 20;

#pragma pack(push, 1)
struct QueryHeaderW {
    DWORD reply_hwnd;              // truncated HWND; 32 bits suffice on x64
    DWORD reply_copydata_message;  // dwData Everything uses on the reply
    DWORD search_flags;
    DWORD offset;
    DWORD max_results;
    // followed by the null-terminated search string
};

struct ListHeaderW {
    DWORD totfolders;
    DWORD totfiles;
    DWORD totitems;
    DWORD numfolders;
    DWORD numfiles;
    DWORD numitems;
    DWORD offset;
    // followed by numitems ItemW, then the string pool
};

struct ItemW {
    DWORD flags;
    DWORD filename_offset;  // byte offset from the start of ListHeaderW
    DWORD path_offset;
};

struct Query2HeaderW {
    DWORD reply_hwnd;
    DWORD reply_copydata_message;
    DWORD search_flags;
    DWORD offset;
    DWORD max_results;
    DWORD request_flags;
    DWORD sort_type;
    // followed by the null-terminated search string
};

struct List2HeaderW {
    DWORD totitems;
    DWORD numitems;
    DWORD offset;
    DWORD request_flags;  // what Everything actually honoured
    DWORD sort_type;      // may differ from what was asked for
    // followed by numitems Item2, then the per-item data blobs
};

struct Item2 {
    DWORD flags;
    DWORD data_offset;  // byte offset from the start of List2HeaderW
};
#pragma pack(pop)

struct Result {
    std::wstring name;
    std::wstring path;
    bool isFolder = false;
    ULONGLONG size = 0;
    FILETIME dateModified{};
    DWORD attributes = 0;
    DWORD runCount = 0;
};

// Is Everything running and listening?
inline HWND FindIpcWindow() {
    return FindWindowW(kIpcWindowClass, nullptr);
}

namespace detail {

// A bounds-checked walk over a reply buffer. Once a read runs past the end
// the cursor latches failed, so a truncated or mismatched reply yields no
// results instead of reading whatever happens to follow it in memory.
class Cursor {
   public:
    Cursor(const BYTE* base, size_t size, size_t pos)
        : base_(base), size_(size), pos_(pos), ok_(pos <= size) {}

    bool ok() const { return ok_; }

    template <typename T>
    bool Read(T* out) {
        if (!ok_ || pos_ + sizeof(T) > size_) {
            ok_ = false;
            return false;
        }
        memcpy(out, base_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }

    // A length-prefixed, null-terminated wide string: a DWORD character count
    // excluding the terminator, then count+1 characters.
    bool ReadString(std::wstring* out) {
        DWORD chars = 0;
        if (!Read(&chars)) {
            return false;
        }
        // Cap the count before it is multiplied: a corrupt length could
        // otherwise wrap the size computation and slip past the bounds check.
        if (chars > (1u << 20)) {
            ok_ = false;
            return false;
        }
        size_t bytes = (static_cast<size_t>(chars) + 1) * sizeof(wchar_t);
        if (pos_ + bytes > size_) {
            ok_ = false;
            return false;
        }
        out->assign(reinterpret_cast<const wchar_t*>(base_ + pos_), chars);
        pos_ += bytes;
        return true;
    }

    bool Skip(size_t bytes) {
        if (!ok_ || pos_ + bytes > size_) {
            ok_ = false;
            return false;
        }
        pos_ += bytes;
        return true;
    }

   private:
    const BYTE* base_;
    size_t size_;
    size_t pos_;
    bool ok_;
};

}  // namespace detail

// Parses a QUERY2 reply.
inline bool ParseReply2(const void* data, DWORD size, std::vector<Result>* out,
                        DWORD* totalMatches) {
    if (!data || size < sizeof(List2HeaderW)) {
        return false;
    }
    const auto* base = static_cast<const BYTE*>(data);
    List2HeaderW list{};
    memcpy(&list, base, sizeof(list));

    if (totalMatches) {
        *totalMatches = list.totitems;
    }
    // Anything Everything honoured that this parser does not know the
    // position of means the blob cannot be walked safely.
    if (list.request_flags & ~kReqSupported) {
        return false;
    }
    size_t itemsEnd = sizeof(List2HeaderW) +
                      static_cast<size_t>(list.numitems) * sizeof(Item2);
    if (list.numitems > 100000 || itemsEnd > size) {
        return false;
    }

    for (DWORD i = 0; i < list.numitems; i++) {
        Item2 item{};
        memcpy(&item, base + sizeof(List2HeaderW) + i * sizeof(Item2),
               sizeof(item));

        detail::Cursor c(base, size, item.data_offset);
        Result r;
        r.isFolder = (item.flags & kItemFolder) != 0;

        // Ascending bit order, which for this flag subset is also the order
        // the fields appear in.
        std::wstring scratch;
        if ((list.request_flags & kReqName) && !c.ReadString(&r.name)) break;
        if ((list.request_flags & kReqPath) && !c.ReadString(&r.path)) break;
        if ((list.request_flags & kReqFullPath) && !c.ReadString(&scratch)) break;
        if ((list.request_flags & kReqSize) && !c.Read(&r.size)) break;
        if ((list.request_flags & kReqDateCreated) && !c.Skip(sizeof(FILETIME))) break;
        if ((list.request_flags & kReqDateModified) && !c.Read(&r.dateModified)) break;
        if ((list.request_flags & kReqDateAccessed) && !c.Skip(sizeof(FILETIME))) break;
        if ((list.request_flags & kReqAttributes) && !c.Read(&r.attributes)) break;
        if ((list.request_flags & kReqRunCount) && !c.Read(&r.runCount)) break;
        if ((list.request_flags & kReqDateRun) && !c.Skip(sizeof(FILETIME))) break;

        if (!c.ok()) {
            break;
        }
        out->push_back(std::move(r));
    }
    return true;
}

// Parses a legacy QUERYW reply.
inline bool ParseReply(const void* data, DWORD size, std::vector<Result>* out,
                       DWORD* totalMatches) {
    if (!data || size < sizeof(ListHeaderW)) {
        return false;
    }
    const auto* base = static_cast<const BYTE*>(data);
    ListHeaderW list{};
    memcpy(&list, base, sizeof(list));

    size_t itemsEnd = sizeof(ListHeaderW) +
                      static_cast<size_t>(list.numitems) * sizeof(ItemW);
    if (list.numitems > 100000 || itemsEnd > size) {
        return false;
    }
    if (totalMatches) {
        *totalMatches = list.totitems;
    }

    for (DWORD i = 0; i < list.numitems; i++) {
        ItemW it{};
        memcpy(&it, base + sizeof(ListHeaderW) + i * sizeof(ItemW), sizeof(it));
        if (it.filename_offset >= size || it.path_offset >= size) {
            return false;  // layout mismatch; refuse the whole reply
        }
        auto readString = [&](DWORD offset) -> std::wstring {
            const auto* p = reinterpret_cast<const wchar_t*>(base + offset);
            DWORD maxChars = (size - offset) / sizeof(wchar_t);
            DWORD n = 0;
            while (n < maxChars && p[n]) {
                n++;
            }
            return std::wstring(p, n);
        };
        Result r;
        r.name = readString(it.filename_offset);
        r.path = readString(it.path_offset);
        r.isFolder = (it.flags & kItemFolder) != 0;
        out->push_back(std::move(r));
    }
    return true;
}

// Synchronous query. Creates a hidden window, sends the request, and pumps
// until the reply arrives or the timeout expires.
class Client {
   public:
    Client() = default;
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    ~Client() {
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
        if (atom_) {
            UnregisterClassW(kReplyClass, GetModuleHandleW(nullptr));
        }
    }

    bool Init() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Client::WndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = kReplyClass;
        atom_ = RegisterClassExW(&wc);
        // A pre-existing class is fine; only a genuine failure is not.
        if (!atom_ && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
        hwnd_ = CreateWindowExW(0, kReplyClass, L"", WS_POPUP, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, wc.hInstance, this);
        if (!hwnd_) {
            return false;
        }

        // Everything answers by sending WM_COPYDATA back to this window, and
        // UIPI silently drops messages sent from a lower integrity level to a
        // higher one. Everything runs at medium, so this only bites when the
        // broker is elevated -- but when it does, the failure is invisible:
        // the query is accepted, Everything runs the search, and the reply is
        // discarded, which is indistinguishable from a timeout. Measured on
        // this machine: elevated without this call, zero replies in 2 s; with
        // it, the same query answers in about 30 ms.
        ChangeWindowMessageFilterEx(hwnd_, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
        return true;
    }

    // Did the reply come back through QUERY2? Only meaningful after a
    // successful Query; false means the extra fields are not populated.
    bool usedQuery2() const { return usedQuery2_; }

    bool Query(const std::wstring& text, DWORD maxResults,
               std::vector<Result>* out, DWORD* totalMatches,
               DWORD timeoutMs = 2000,
               DWORD requestFlags = kDefaultRequestFlags,
               DWORD sortType = kSortNameAscending) {
        HWND everything = FindIpcWindow();
        if (!everything || !hwnd_) {
            return false;
        }

        if (SendQuery2(everything, text, maxResults, requestFlags, sortType) &&
            Await(timeoutMs)) {
            usedQuery2_ = true;
            *out = std::move(results_);
            if (totalMatches) {
                *totalMatches = total_;
            }
            return true;
        }

        // Older Everything builds answer FALSE to QUERY2. Names and paths
        // still come back through the original message; the ranker then has
        // only the name to score on.
        if (!SendQueryLegacy(everything, text, maxResults) ||
            !Await(timeoutMs)) {
            return false;
        }
        usedQuery2_ = false;
        *out = std::move(results_);
        if (totalMatches) {
            *totalMatches = total_;
        }
        return true;
    }

   private:
    static constexpr wchar_t kReplyClass[] = L"WindhawkEverythingBrokerReply";
    static constexpr DWORD kReplyId = 0x45565251;   // EVRQ
    static constexpr DWORD kReplyId2 = 0x45565232;  // EVR2

    void Reset() {
        results_.clear();
        total_ = 0;
        replied_ = false;
    }

    bool SendQuery2(HWND everything, const std::wstring& text,
                    DWORD maxResults, DWORD requestFlags, DWORD sortType) {
        Reset();
        expecting_ = kReplyId2;
        std::vector<BYTE> buffer(sizeof(Query2HeaderW) +
                                 (text.size() + 1) * sizeof(wchar_t));
        auto* q = reinterpret_cast<Query2HeaderW*>(buffer.data());
        q->reply_hwnd = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(hwnd_));
        q->reply_copydata_message = kReplyId2;
        q->search_flags = 0;
        q->offset = 0;
        q->max_results = maxResults;
        q->request_flags = requestFlags & kReqSupported;
        q->sort_type = sortType;
        memcpy(buffer.data() + sizeof(Query2HeaderW), text.c_str(),
               (text.size() + 1) * sizeof(wchar_t));
        return Send(everything, kCopyDataQuery2W, buffer);
    }

    bool SendQueryLegacy(HWND everything, const std::wstring& text,
                         DWORD maxResults) {
        Reset();
        expecting_ = kReplyId;
        std::vector<BYTE> buffer(sizeof(QueryHeaderW) +
                                 (text.size() + 1) * sizeof(wchar_t));
        auto* q = reinterpret_cast<QueryHeaderW*>(buffer.data());
        q->reply_hwnd = static_cast<DWORD>(reinterpret_cast<ULONG_PTR>(hwnd_));
        q->reply_copydata_message = kReplyId;
        q->search_flags = 0;
        q->offset = 0;
        q->max_results = maxResults;
        memcpy(buffer.data() + sizeof(QueryHeaderW), text.c_str(),
               (text.size() + 1) * sizeof(wchar_t));
        return Send(everything, kCopyDataQueryW, buffer);
    }

    bool Send(HWND everything, DWORD message, std::vector<BYTE>& buffer) {
        COPYDATASTRUCT cds{};
        cds.dwData = message;
        cds.cbData = static_cast<DWORD>(buffer.size());
        cds.lpData = buffer.data();
        return SendMessageW(everything, WM_COPYDATA,
                            reinterpret_cast<WPARAM>(hwnd_),
                            reinterpret_cast<LPARAM>(&cds)) != 0;
    }

    // Everything answers with a sent (not posted) message, so this thread has
    // to be inside a message-retrieval call for the reply to be delivered.
    //
    // This used to spin on Sleep(1). That looked harmless and was not: the
    // default system timer tick is about 15.6 ms, so Sleep(1) sleeps for a
    // tick, and every query appeared to cost 15, 30 or 45 ms. A search with
    // zero matches measured 30 ms -- all of it this loop. Blocking on the
    // message queue instead removes that floor entirely.
    bool Await(DWORD timeoutMs) {
        const DWORD start = GetTickCount();
        MSG msg;
        for (;;) {
            // Drain first. The reply may already be waiting, and blocking
            // before draining would wait for the message after it.
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (replied_) {
                return true;
            }
            const DWORD elapsed = GetTickCount() - start;
            if (elapsed >= timeoutMs) {
                return false;
            }
            MsgWaitForMultipleObjectsEx(0, nullptr, timeoutMs - elapsed,
                                        QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        }
    }

    static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        if (m == WM_CREATE) {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h, GWLP_USERDATA,
                              reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return 0;
        }
        if (m == WM_COPYDATA) {
            auto* self =
                reinterpret_cast<Client*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            auto* cds = reinterpret_cast<COPYDATASTRUCT*>(l);
            if (self && cds && cds->dwData == self->expecting_) {
                if (cds->dwData == kReplyId2) {
                    ParseReply2(cds->lpData, cds->cbData, &self->results_,
                                &self->total_);
                } else {
                    ParseReply(cds->lpData, cds->cbData, &self->results_,
                               &self->total_);
                }
                self->replied_ = true;
                return TRUE;
            }
        }
        return DefWindowProcW(h, m, w, l);
    }

    ATOM atom_ = 0;
    HWND hwnd_ = nullptr;
    std::vector<Result> results_;
    DWORD total_ = 0;
    DWORD expecting_ = 0;
    bool replied_ = false;
    bool usedQuery2_ = false;
};

}  // namespace everything
