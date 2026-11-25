/**
 * @file Clipboard.cpp
 * @brief 클립보드 도우미 구현
 */
#include "pch.h"
#include "UI/Clipboard.h"

#include <cstring>

namespace
{
/** @brief 범위를 벗어나면 자동으로 클립보드를 닫습니다. */
class ClipboardLock
{
public:
    explicit ClipboardLock(HWND owner) : m_open(::OpenClipboard(owner) != FALSE) {}
    ~ClipboardLock()
    {
        if (m_open)
        {
            ::CloseClipboard();
        }
    }
    ClipboardLock(const ClipboardLock&) = delete;
    ClipboardLock& operator=(const ClipboardLock&) = delete;

    bool IsOpen() const { return m_open; }

private:
    bool m_open;
};
} // namespace

namespace Clipboard
{
bool PutImage(HWND owner, CSize size, const std::vector<Pixel>& pixels)
{
    if (size.cx <= 0 || size.cy <= 0 || pixels.size() != static_cast<size_t>(size.cx) * size.cy)
    {
        return false;
    }

    const size_t stride = (static_cast<size_t>(size.cx) * 3 + 3) & ~static_cast<size_t>(3);
    const size_t imageBytes = stride * size.cy;
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + imageBytes);
    if (memory == nullptr)
    {
        return false;
    }

    auto* header = static_cast<BITMAPINFOHEADER*>(::GlobalLock(memory));
    std::memset(header, 0, sizeof(BITMAPINFOHEADER) + imageBytes);
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = size.cx;
    header->biHeight = size.cy; // 양수 = 아래에서 위로
    header->biPlanes = 1;
    header->biBitCount = 24;
    header->biCompression = BI_RGB;
    header->biSizeImage = static_cast<DWORD>(imageBytes);

    auto* bits = reinterpret_cast<uint8_t*>(header + 1);
    for (int y = 0; y < size.cy; ++y)
    {
        const Pixel* src = pixels.data() + static_cast<size_t>(y) * size.cx;
        uint8_t* dst = bits + stride * (size.cy - 1 - y);
        for (int x = 0; x < size.cx; ++x)
        {
            dst[x * 3 + 0] = static_cast<uint8_t>(src[x] & 0xFF);         // B
            dst[x * 3 + 1] = static_cast<uint8_t>((src[x] >> 8) & 0xFF);  // G
            dst[x * 3 + 2] = static_cast<uint8_t>((src[x] >> 16) & 0xFF); // R
        }
    }
    ::GlobalUnlock(memory);

    ClipboardLock lock(owner);
    if (!lock.IsOpen() || !::EmptyClipboard() || ::SetClipboardData(CF_DIB, memory) == nullptr)
    {
        ::GlobalFree(memory);
        return false;
    }
    return true; // 성공하면 메모리는 시스템이 가집니다.
}

bool HasImage()
{
    return ::IsClipboardFormatAvailable(CF_BITMAP) || ::IsClipboardFormatAvailable(CF_DIB) ||
           ::IsClipboardFormatAvailable(CF_DIBV5);
}

bool GetImage(HWND owner, CSize& size, std::vector<Pixel>& pixels)
{
    ClipboardLock lock(owner);
    if (!lock.IsOpen())
    {
        return false;
    }
    auto bitmap = static_cast<HBITMAP>(::GetClipboardData(CF_BITMAP));
    BITMAP info{};
    if (bitmap == nullptr || ::GetObject(bitmap, sizeof(info), &info) == 0 || info.bmWidth <= 0 || info.bmHeight <= 0)
    {
        return false;
    }

    Surface surface;
    if (!surface.Create(info.bmWidth, info.bmHeight, RGB(255, 255, 255)))
    {
        return false;
    }
    HDC source = ::CreateCompatibleDC(nullptr);
    HGDIOBJ old = ::SelectObject(source, bitmap);
    ::BitBlt(surface.DC(), 0, 0, info.bmWidth, info.bmHeight, source, 0, 0, SRCCOPY);
    ::SelectObject(source, old);
    ::DeleteDC(source);

    PixelBlock block = surface.ReadBlock(surface.Bounds());
    for (Pixel& p : block.pixels)
    {
        p &= 0x00FFFFFF; // 알파 바이트는 쓰지 않습니다.
    }
    size = block.rect.Size();
    pixels = std::move(block.pixels);
    return true;
}

bool GetText(HWND owner, std::wstring& text)
{
    if (!::IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        return false;
    }
    ClipboardLock lock(owner);
    if (!lock.IsOpen())
    {
        return false;
    }
    HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
    if (data == nullptr)
    {
        return false;
    }
    const auto* chars = static_cast<const wchar_t*>(::GlobalLock(data));
    if (chars == nullptr)
    {
        return false;
    }
    text = chars;
    ::GlobalUnlock(data);
    return true;
}
} // namespace Clipboard
