/**
 * @file Surface.cpp
 * @brief Surface 구현
 */
#include "pch.h"
#include "Core/Surface.h"

#include <algorithm>
#include <utility>

namespace
{
/** @brief 한 표면에 허용하는 최대 픽셀 수 (약 400MB) */
constexpr long long kMaxPixels = 100LL * 1000 * 1000;
} // namespace

Surface::~Surface()
{
    Destroy();
}

Surface::Surface(Surface&& other) noexcept
{
    *this = std::move(other);
}

Surface& Surface::operator=(Surface&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        std::swap(m_dc, other.m_dc);
        std::swap(m_bitmap, other.m_bitmap);
        std::swap(m_oldBitmap, other.m_oldBitmap);
        std::swap(m_bits, other.m_bits);
        std::swap(m_width, other.m_width);
        std::swap(m_height, other.m_height);
    }
    return *this;
}

bool Surface::Create(int width, int height, COLORREF fill)
{
    Destroy();
    if (width <= 0 || height <= 0 || static_cast<long long>(width) * height > kMaxPixels)
    {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // 음수 = top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bitmap == nullptr)
    {
        return false;
    }
    HDC dc = ::CreateCompatibleDC(nullptr);
    if (dc == nullptr)
    {
        ::DeleteObject(bitmap);
        return false;
    }

    m_dc = dc;
    m_bitmap = bitmap;
    m_oldBitmap = ::SelectObject(dc, bitmap);
    m_bits = static_cast<Pixel*>(bits);
    m_width = width;
    m_height = height;
    Fill(fill);
    return true;
}

void Surface::Destroy()
{
    if (m_dc != nullptr)
    {
        ::SelectObject(m_dc, m_oldBitmap);
        ::DeleteDC(m_dc);
    }
    if (m_bitmap != nullptr)
    {
        ::DeleteObject(m_bitmap);
    }
    m_dc = nullptr;
    m_bitmap = nullptr;
    m_oldBitmap = nullptr;
    m_bits = nullptr;
    m_width = 0;
    m_height = 0;
}

Pixel* Surface::Bits()
{
    ::GdiFlush();
    return m_bits;
}

const Pixel* Surface::Bits() const
{
    ::GdiFlush();
    return m_bits;
}

Pixel Surface::GetPixel(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_width || y >= m_height)
    {
        return 0;
    }
    return Bits()[static_cast<size_t>(y) * m_width + x];
}

void Surface::Fill(COLORREF color)
{
    if (!IsValid())
    {
        return;
    }
    Pixel* bits = Bits();
    std::fill(bits, bits + static_cast<size_t>(m_width) * m_height, ToPixel(color));
}

PixelBlock Surface::ReadBlock(const CRect& rect) const
{
    PixelBlock block;
    if (!block.rect.IntersectRect(rect, Bounds()))
    {
        block.rect.SetRectEmpty();
        return block;
    }

    const int w = block.rect.Width();
    const int h = block.rect.Height();
    block.pixels.resize(static_cast<size_t>(w) * h);
    const Pixel* src = Bits();
    for (int y = 0; y < h; ++y)
    {
        const Pixel* row = src + static_cast<size_t>(block.rect.top + y) * m_width + block.rect.left;
        std::copy(row, row + w, block.pixels.begin() + static_cast<ptrdiff_t>(y) * w);
    }
    return block;
}

void Surface::WriteBlock(const PixelBlock& block)
{
    if (block.IsEmpty())
    {
        return;
    }
    const CRect& r = block.rect;
    ASSERT(r.left >= 0 && r.top >= 0 && r.right <= m_width && r.bottom <= m_height);
    ASSERT(block.pixels.size() == static_cast<size_t>(r.Width()) * r.Height());

    const int w = r.Width();
    Pixel* dst = Bits();
    for (int y = 0; y < r.Height(); ++y)
    {
        const auto first = block.pixels.begin() + static_cast<ptrdiff_t>(y) * w;
        std::copy(first, first + w, dst + static_cast<size_t>(r.top + y) * m_width + r.left);
    }
}

void Surface::CopyFrom(const Surface& source, const CRect& rect)
{
    ASSERT(source.Size() == Size());
    CRect r;
    if (!r.IntersectRect(rect, Bounds()))
    {
        return;
    }

    const int w = r.Width();
    const Pixel* src = source.Bits();
    Pixel* dst = Bits();
    for (int y = r.top; y < r.bottom; ++y)
    {
        const size_t offset = static_cast<size_t>(y) * m_width + r.left;
        std::copy(src + offset, src + offset + w, dst + offset);
    }
}

void Surface::FillRect(const CRect& rect, COLORREF color)
{
    CRect r;
    if (!r.IntersectRect(rect, Bounds()))
    {
        return;
    }
    const Pixel value = ToPixel(color);
    Pixel* bits = Bits();
    for (int y = r.top; y < r.bottom; ++y)
    {
        Pixel* row = bits + static_cast<size_t>(y) * m_width;
        std::fill(row + r.left, row + r.right, value);
    }
}

void Surface::WritePixels(CPoint dest, int width, int height, const Pixel* pixels)
{
    CRect r;
    if (pixels == nullptr || !r.IntersectRect(CRect(dest, CSize(width, height)), Bounds()))
    {
        return;
    }
    Pixel* bits = Bits();
    const int count = r.Width();
    for (int y = r.top; y < r.bottom; ++y)
    {
        const Pixel* src = pixels + static_cast<size_t>(y - dest.y) * width + (r.left - dest.x);
        std::copy(src, src + count, bits + static_cast<size_t>(y) * m_width + r.left);
    }
}
