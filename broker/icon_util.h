// Turning shell icons into the pixel format the panel can draw.
//
// The panel lives in an AppContainer with no shell namespace and no file
// access, so it cannot fetch an icon for anything. Icons have to arrive as
// raw pixels over the same message that carries the rows: BGRA, top-down,
// premultiplied, which is what a XAML WriteableBitmap expects.

#pragma once

#include <windows.h>

#include <commoncontrols.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace icons {

// Copies a bitmap into a top-down 32bpp BGRA buffer.
inline bool BitmapToBgra(HBITMAP bitmap, int size, std::vector<BYTE>* out) {
    if (!bitmap) {
        return false;
    }
    BITMAP info{};
    if (!GetObjectW(bitmap, sizeof(info), &info)) {
        return false;
    }
    // Rescaling is not this function's job. A mismatch means the caller asked
    // the shell for one size and got another, and sending a buffer whose
    // dimensions disagree with its byte count would just be rejected at the
    // other end.
    if (info.bmWidth != size || info.bmHeight != size) {
        return false;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;  // negative: top-down, matching XAML
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    out->assign(static_cast<size_t>(size) * size * 4, 0);
    HDC screen = GetDC(nullptr);
    int scanned = GetDIBits(screen, bitmap, 0, size, out->data(), &bi,
                            DIB_RGB_COLORS);
    ReleaseDC(nullptr, screen);
    return scanned == size;
}

// Draws an icon into a 32bpp surface and copies it out. DrawIconEx is used
// rather than reading the icon's own bitmaps because it handles both modern
// 32bpp icons and the old mask-plus-colour pairs, and produces straight
// alpha either way.
inline bool IconToBgra(HICON icon, int size, std::vector<BYTE>* out) {
    if (!icon) {
        return false;
    }
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (!dc) {
        return false;
    }
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        DeleteDC(dc);
        return false;
    }
    HGDIOBJ previous = SelectObject(dc, dib);
    memset(bits, 0, static_cast<size_t>(size) * size * 4);
    BOOL drawn = DrawIconEx(dc, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);
    if (drawn) {
        out->assign(static_cast<BYTE*>(bits),
                    static_cast<BYTE*>(bits) + static_cast<size_t>(size) * size * 4);
    }
    SelectObject(dc, previous);
    DeleteObject(dib);
    DeleteDC(dc);
    return drawn != FALSE;
}

// Icons for files, keyed by extension.
//
// Fetching a real icon per result is far too slow to do per keystroke -- a
// single IShellItemImageFactory::GetImage measured 38-220 ms. Almost every
// file of the same type has the same icon, though, so the shell is asked once
// per extension using SHGFI_USEFILEATTRIBUTES, which answers from the
// registered file type without touching the disk at all.
class ExtensionCache {
   public:
    explicit ExtensionCache(int size) : size_(size) {}

    // Returns BGRA pixels, or nullptr when the shell had nothing.
    const std::vector<BYTE>* Get(const std::wstring& nameOrPath,
                                 bool isFolder) {
        std::wstring key = isFolder ? L"<dir>" : ExtensionOf(nameOrPath);
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            return it->second.empty() ? nullptr : &it->second;
        }

        std::vector<BYTE> pixels;
        // A name that does not exist is fine and is the point: with
        // SHGFI_USEFILEATTRIBUTES the shell answers from the extension alone.
        std::wstring probe = isFolder ? L"folder" : (L"file" + key);
        SHFILEINFOW info{};
        DWORD attributes =
            isFolder ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
        if (SHGetFileInfoW(probe.c_str(), attributes, &info, sizeof(info),
                           SHGFI_USEFILEATTRIBUTES | SHGFI_ICON |
                               SHGFI_LARGEICON)) {
            IconToBgra(info.hIcon, size_, &pixels);
            DestroyIcon(info.hIcon);
        }
        auto inserted = cache_.emplace(key, std::move(pixels));
        return inserted.first->second.empty() ? nullptr
                                              : &inserted.first->second;
    }

    size_t size() const { return cache_.size(); }

   private:
    static std::wstring ExtensionOf(const std::wstring& name) {
        size_t dot = name.rfind(L'.');
        if (dot == std::wstring::npos || dot + 1 >= name.size()) {
            return L"";
        }
        std::wstring ext = name.substr(dot);
        for (wchar_t& c : ext) {
            c = static_cast<wchar_t>(towlower(c));
        }
        return ext;
    }

    int size_;
    std::unordered_map<std::wstring, std::vector<BYTE>> cache_;
};

}  // namespace icons
