/**
 * @file ImageCommands.cpp
 * @brief 픽셀 단위 편집 명령 구현
 */
#include "pch.h"
#include "Core/ImageCommands.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Core/ByteStream.h"

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxSkewDegrees = 89;

void WriteRect(ByteWriter& writer, const CRect& r)
{
    writer.Write(static_cast<int32_t>(r.left));
    writer.Write(static_cast<int32_t>(r.top));
    writer.Write(static_cast<int32_t>(r.right));
    writer.Write(static_cast<int32_t>(r.bottom));
}

CRect ReadRect(ByteReader& reader)
{
    const auto l = reader.Read<int32_t>();
    const auto t = reader.Read<int32_t>();
    const auto r = reader.Read<int32_t>();
    const auto b = reader.Read<int32_t>();
    return CRect(l, t, r, b);
}

bool IsValidCanvasSize(CSize size)
{
    return size.cx > 0 && size.cy > 0 && size.cx <= kMaxCanvasSide && size.cy <= kMaxCanvasSide;
}

/** @brief 기울이기로 늘어나는 픽셀 수 (부호 포함) */
int SkewOffset(int length, int degrees)
{
    return static_cast<int>(std::lround(length * std::tan(degrees * kPi / 180.0)));
}
} // namespace

// ---------------------------------------------------------------------------
// PatchCommand
// ---------------------------------------------------------------------------

std::unique_ptr<PatchCommand> PatchCommand::MakeClear(const CRect& rect, COLORREF color)
{
    return std::make_unique<PatchCommand>(rect, color, CPoint(), CSize(), nullptr);
}

PatchCommand::PatchCommand(const CRect& clearRect, COLORREF clearColor, CPoint dest, CSize size, PixelData pixels)
    : m_clearRect(clearRect), m_clearColor(clearColor), m_dest(dest), m_size(size), m_pixels(std::move(pixels))
{
    ASSERT(!m_pixels || m_pixels->size() == static_cast<size_t>(m_size.cx) * m_size.cy);
}

CRect PatchCommand::Bounds() const
{
    CRect r = m_clearRect;
    if (m_pixels)
    {
        r |= CRect(m_dest, m_size);
    }
    return r;
}

void PatchCommand::Execute(Surface& surface) const
{
    if (!m_clearRect.IsRectEmpty())
    {
        surface.FillRect(m_clearRect, m_clearColor);
    }
    if (m_pixels)
    {
        surface.WritePixels(m_dest, m_size.cx, m_size.cy, m_pixels->data());
    }
}

void PatchCommand::Write(ByteWriter& writer) const
{
    WriteRect(writer, m_clearRect);
    writer.Write(static_cast<uint32_t>(m_clearColor));
    writer.Write(static_cast<int32_t>(m_dest.x));
    writer.Write(static_cast<int32_t>(m_dest.y));
    writer.Write(static_cast<int32_t>(m_size.cx));
    writer.Write(static_cast<int32_t>(m_size.cy));
    writer.WriteVector(m_pixels ? *m_pixels : std::vector<Pixel>());
}

std::unique_ptr<PatchCommand> PatchCommand::Read(ByteReader& reader)
{
    const CRect clearRect = ReadRect(reader);
    const auto clearColor = reader.Read<uint32_t>() & 0x00FFFFFF;
    const auto dx = reader.Read<int32_t>();
    const auto dy = reader.Read<int32_t>();
    const auto w = reader.Read<int32_t>();
    const auto h = reader.Read<int32_t>();
    if (w < 0 || h < 0 || w > kMaxCanvasSide || h > kMaxCanvasSide)
    {
        throw std::runtime_error("PatchCommand: 잘못된 크기입니다.");
    }
    auto pixels = reader.ReadVector<Pixel>(static_cast<uint32_t>(w) * static_cast<uint32_t>(h));
    PixelData data;
    if (!pixels.empty())
    {
        if (pixels.size() != static_cast<size_t>(w) * h)
        {
            throw std::runtime_error("PatchCommand: 픽셀 수가 맞지 않습니다.");
        }
        data = std::make_shared<const std::vector<Pixel>>(std::move(pixels));
    }
    return std::make_unique<PatchCommand>(clearRect, clearColor, CPoint(dx, dy), CSize(w, h), std::move(data));
}

// ---------------------------------------------------------------------------
// InvertCommand
// ---------------------------------------------------------------------------

void InvertCommand::Execute(Surface& surface) const
{
    CRect r;
    if (!r.IntersectRect(m_rect, surface.Bounds()))
    {
        return;
    }
    Pixel* bits = surface.Bits();
    for (int y = r.top; y < r.bottom; ++y)
    {
        Pixel* row = bits + static_cast<size_t>(y) * surface.Width();
        for (int x = r.left; x < r.right; ++x)
        {
            row[x] = (row[x] ^ 0x00FFFFFF) & 0x00FFFFFF;
        }
    }
}

void InvertCommand::Write(ByteWriter& writer) const
{
    WriteRect(writer, m_rect);
}

std::unique_ptr<InvertCommand> InvertCommand::Read(ByteReader& reader)
{
    return std::make_unique<InvertCommand>(ReadRect(reader));
}

// ---------------------------------------------------------------------------
// TransformCommand
// ---------------------------------------------------------------------------

void TransformCommand::Execute(Surface& surface) const
{
    const int w = surface.Width();
    const int h = surface.Height();
    const bool swap = m_kind == TransformKind::RotateRight || m_kind == TransformKind::RotateLeft;
    const int newW = swap ? h : w;
    const int newH = swap ? w : h;

    Surface result;
    if (!result.Create(newW, newH, RGB(255, 255, 255)))
    {
        return;
    }
    const Pixel* src = surface.Bits();
    Pixel* dst = result.Bits();
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            int nx = x;
            int ny = y;
            switch (m_kind)
            {
            case TransformKind::RotateRight: nx = h - 1 - y; ny = x; break;
            case TransformKind::RotateLeft: nx = y; ny = w - 1 - x; break;
            case TransformKind::Rotate180: nx = w - 1 - x; ny = h - 1 - y; break;
            case TransformKind::FlipHorizontal: nx = w - 1 - x; break;
            case TransformKind::FlipVertical: ny = h - 1 - y; break;
            default: break;
            }
            dst[static_cast<size_t>(ny) * newW + nx] = src[static_cast<size_t>(y) * w + x];
        }
    }
    surface = std::move(result);
}

void TransformCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<uint8_t>(m_kind));
}

std::unique_ptr<TransformCommand> TransformCommand::Read(ByteReader& reader)
{
    const auto kind = reader.Read<uint8_t>();
    if (kind >= static_cast<uint8_t>(TransformKind::Count))
    {
        throw std::runtime_error("TransformCommand: 잘못된 종류입니다.");
    }
    return std::make_unique<TransformCommand>(static_cast<TransformKind>(kind));
}

// ---------------------------------------------------------------------------
// ResizeSkewCommand
// ---------------------------------------------------------------------------

ResizeSkewCommand::ResizeSkewCommand(CSize newSize, int skewHorizontalDegrees, int skewVerticalDegrees,
                                     COLORREF background)
    : m_newSize(newSize),
      m_skewH(std::clamp(skewHorizontalDegrees, -kMaxSkewDegrees, kMaxSkewDegrees)),
      m_skewV(std::clamp(skewVerticalDegrees, -kMaxSkewDegrees, kMaxSkewDegrees)),
      m_background(background)
{
}

CSize ResizeSkewCommand::ResultSize() const
{
    CSize size = m_newSize;
    size.cx += std::abs(SkewOffset(size.cy, m_skewH));
    size.cy += std::abs(SkewOffset(size.cx, m_skewV));
    return size;
}

void ResizeSkewCommand::Execute(Surface& surface) const
{
    if (!IsValidCanvasSize(m_newSize) || !IsValidCanvasSize(ResultSize()))
    {
        return;
    }

    // 1) 크기 조정
    if (m_newSize != surface.Size())
    {
        Surface resized;
        if (!resized.Create(m_newSize.cx, m_newSize.cy, m_background))
        {
            return;
        }
        ::SetStretchBltMode(resized.DC(), HALFTONE);
        ::SetBrushOrgEx(resized.DC(), 0, 0, nullptr);
        ::StretchBlt(resized.DC(), 0, 0, m_newSize.cx, m_newSize.cy, surface.DC(), 0, 0, surface.Width(),
                     surface.Height(), SRCCOPY);
        surface = std::move(resized);
    }

    // 2) 가로 기울이기: 아래쪽이 오른쪽으로 밀리는 평행사변형
    if (m_skewH != 0)
    {
        const int w = surface.Width();
        const int h = surface.Height();
        const int dx = SkewOffset(h, m_skewH);
        Surface skewed;
        if (!skewed.Create(w + std::abs(dx), h, m_background))
        {
            return;
        }
        const int shift = dx < 0 ? -dx : 0;
        const POINT corners[3] = {{shift, 0}, {shift + w, 0}, {shift + dx, h}};
        ::PlgBlt(skewed.DC(), corners, surface.DC(), 0, 0, w, h, nullptr, 0, 0);
        surface = std::move(skewed);
    }

    // 3) 세로 기울이기: 오른쪽이 아래로 밀리는 평행사변형
    if (m_skewV != 0)
    {
        const int w = surface.Width();
        const int h = surface.Height();
        const int dy = SkewOffset(w, m_skewV);
        Surface skewed;
        if (!skewed.Create(w, h + std::abs(dy), m_background))
        {
            return;
        }
        const int shift = dy < 0 ? -dy : 0;
        const POINT corners[3] = {{0, shift}, {w, shift + dy}, {0, shift + h}};
        ::PlgBlt(skewed.DC(), corners, surface.DC(), 0, 0, w, h, nullptr, 0, 0);
        surface = std::move(skewed);
    }
}

void ResizeSkewCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<int32_t>(m_newSize.cx));
    writer.Write(static_cast<int32_t>(m_newSize.cy));
    writer.Write(static_cast<int32_t>(m_skewH));
    writer.Write(static_cast<int32_t>(m_skewV));
    writer.Write(static_cast<uint32_t>(m_background));
}

std::unique_ptr<ResizeSkewCommand> ResizeSkewCommand::Read(ByteReader& reader)
{
    const auto w = reader.Read<int32_t>();
    const auto h = reader.Read<int32_t>();
    const auto skewH = reader.Read<int32_t>();
    const auto skewV = reader.Read<int32_t>();
    const auto background = reader.Read<uint32_t>() & 0x00FFFFFF;
    if (!IsValidCanvasSize(CSize(w, h)))
    {
        throw std::runtime_error("ResizeSkewCommand: 잘못된 크기입니다.");
    }
    return std::make_unique<ResizeSkewCommand>(CSize(w, h), skewH, skewV, background);
}

// ---------------------------------------------------------------------------
// CanvasSizeCommand
// ---------------------------------------------------------------------------

void CanvasSizeCommand::Execute(Surface& surface) const
{
    Surface result;
    if (!IsValidCanvasSize(m_newSize) || !result.Create(m_newSize.cx, m_newSize.cy, m_background))
    {
        return;
    }
    result.WritePixels(m_offset, surface.Width(), surface.Height(), surface.Bits());
    surface = std::move(result);
}

void CanvasSizeCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<int32_t>(m_newSize.cx));
    writer.Write(static_cast<int32_t>(m_newSize.cy));
    writer.Write(static_cast<int32_t>(m_offset.x));
    writer.Write(static_cast<int32_t>(m_offset.y));
    writer.Write(static_cast<uint32_t>(m_background));
}

std::unique_ptr<CanvasSizeCommand> CanvasSizeCommand::Read(ByteReader& reader)
{
    const auto w = reader.Read<int32_t>();
    const auto h = reader.Read<int32_t>();
    const auto ox = reader.Read<int32_t>();
    const auto oy = reader.Read<int32_t>();
    const auto background = reader.Read<uint32_t>() & 0x00FFFFFF;
    if (!IsValidCanvasSize(CSize(w, h)))
    {
        throw std::runtime_error("CanvasSizeCommand: 잘못된 크기입니다.");
    }
    return std::make_unique<CanvasSizeCommand>(CSize(w, h), CPoint(ox, oy), background);
}
