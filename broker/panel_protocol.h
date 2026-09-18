// Wire protocol between the broker and the in-process panel.
//
// This is the canonical copy. everything-search.wh.cpp carries an identical
// one inline, because a Windhawk mod is a single translation unit and cannot
// include a project header. Both sides check kVersion on every push, so if
// the two copies drift the first message fails loudly instead of being read
// as garbage.
//
// Direction matters and is not symmetric. SearchHost is a low-integrity
// AppContainer: messages from normal integrity down to it are delivered, and
// messages from it back up are dropped by UIPI without an error on either
// side. So every message here travels one way, and the panel answers by
// returning a value from its window procedure -- a return value is the result
// of the broker's own call rather than a message of its own, so it crosses
// back where a message would not.

#pragma once

#include <windows.h>

namespace protocol {

inline constexpr DWORD kMagic = 0x564E5045;  // EPNV
inline constexpr DWORD kVersion = 1;

// The panel's listening window. A real top-level window, not HWND_MESSAGE:
// FindWindow cannot see message-only windows from another process.
inline constexpr wchar_t kWindowClass[] = L"WindhawkEverythingSearchPanel";

inline constexpr DWORD kMsgResults = 0x45565031;  // payload: Header + rows
inline constexpr DWORD kMsgPoll = 0x45565032;     // no payload
inline constexpr DWORD kMsgRelease = 0x45565033;  // no payload: hand back

enum RowKind : DWORD { kRowApp = 0, kRowFile = 1 };
enum RowFlags : DWORD { kRowFolder = 1 };

#pragma pack(push, 1)
struct Header {
    DWORD magic;
    DWORD version;
    DWORD seq;  // bumped per push, so a click can be matched to what it hit
    DWORD appCount;
    DWORD fileCount;
    DWORD fileTotal;  // total Everything matches, not just those sent
    DWORD statusLen;  // characters, excluding the terminator
    // followed by the status string, then appCount + fileCount rows
};

struct RowHeader {
    DWORD kind;
    DWORD flags;
    DWORD titleLen;     // characters, excluding the terminator
    DWORD subtitleLen;  // characters, excluding the terminator
    DWORD iconSize;     // pixels per side; 0 when no icon was sent
    DWORD iconBytes;    // iconSize * iconSize * 4, BGRA, premultiplied
    // followed by title, subtitle, then the icon bytes
};
#pragma pack(pop)

// The panel refuses more than this per column, so there is no point sending
// more.
inline constexpr DWORD kMaxRowsPerColumn = 64;

// The panel answers by returning a value from its window procedure, because
// it cannot send a message upward. That value is truncated to 32 bits on the
// way back across the process boundary -- measured, not assumed: the panel
// returned 0x4000000100010002 and the broker received 0x00010002, exactly the
// low half. So the whole action has to fit in 32 bits.
inline constexpr DWORD kActionValid = 0x80000000u;

inline LRESULT EncodeAction(DWORD seq, DWORD kind, DWORD index) {
    DWORD packed = kActionValid | ((seq & 0x7FFF) << 16) |
                   ((kind & 0xF) << 12) | (index & 0xFFF);
    return static_cast<LRESULT>(packed);
}

struct Action {
    bool valid = false;
    DWORD seq = 0;
    DWORD kind = 0;
    DWORD index = 0;
};

inline Action DecodeAction(LRESULT value) {
    Action a;
    // Mask before testing the flag: whether a truncated return arrives
    // sign-extended is not worth depending on.
    DWORD packed =
        static_cast<DWORD>(static_cast<ULONG_PTR>(value) & 0xFFFFFFFFu);
    if (!(packed & kActionValid)) {
        return a;
    }
    a.valid = true;
    a.seq = (packed >> 16) & 0x7FFF;   // 15 bits: compare with seq & 0x7FFF
    a.kind = (packed >> 12) & 0xF;
    a.index = packed & 0xFFF;
    return a;
}

// Finds the panel. Absent means the mod is not loaded, or SearchHost has not
// opened its search page yet.
inline HWND FindPanel() {
    return FindWindowW(kWindowClass, nullptr);
}

}  // namespace protocol
