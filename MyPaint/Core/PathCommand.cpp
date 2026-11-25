/**
 * @file PathCommand.cpp
 * @brief PathCommand 구현
 */
#include "pch.h"
#include "Core/PathCommand.h"

#include <algorithm>
#include <stdexcept>

#include "Core/ByteStream.h"
#include "Core/Surface.h"

namespace
{
constexpr int kMaxWidth = 1000;

/** @brief 종류별로 필요한 점 수가 맞는지 */
bool IsValidPointCount(PathKind kind, size_t count)
{
    switch (kind)
    {
    case PathKind::Curve: return count == 4;
    case PathKind::Polyline: return count >= 2 && count <= PathCommand::kMaxPoints;
    case PathKind::Polygon: return count >= 3 && count <= PathCommand::kMaxPoints;
    default: return false;
    }
}
} // namespace

CRect PathCommand::Bounds() const
{
    if (m_points.empty())
    {
        return CRect();
    }
    // 베지어 곡선은 네 점의 볼록 껍질 안에 있으므로 점들의 경계 상자로 충분합니다.
    CRect r(m_points.front(), CSize(0, 0));
    for (const CPoint& p : m_points)
    {
        r.left = (std::min)(r.left, p.x);
        r.top = (std::min)(r.top, p.y);
        r.right = (std::max)(r.right, p.x);
        r.bottom = (std::max)(r.bottom, p.y);
    }
    r.right += 1;
    r.bottom += 1;
    const int margin = std::clamp(m_style.width, 1, kMaxWidth) + 2;
    r.InflateRect(margin, margin);
    return r;
}

void PathCommand::Execute(Surface& surface) const
{
    if (!surface.IsValid() || !IsValidPointCount(m_kind, m_points.size()))
    {
        return;
    }
    HDC dc = surface.DC();
    const int width = std::clamp(m_style.width, 1, kMaxWidth);
    std::vector<POINT> points(m_points.begin(), m_points.end());
    const int count = static_cast<int>(points.size());

    if (m_kind == PathKind::Curve || m_kind == PathKind::Polyline)
    {
        HPEN pen = CreateLinePen(m_style.outlineColor, width);
        HGDIOBJ oldPen = ::SelectObject(dc, pen);
        if (m_kind == PathKind::Curve)
        {
            ::PolyBezier(dc, points.data(), 4);
        }
        else
        {
            ::Polyline(dc, points.data(), count);
        }
        if (width <= 1)
        {
            // 두께 1 펜은 마지막 점을 그리지 않습니다.
            ::SetPixelV(dc, points.back().x, points.back().y, m_style.outlineColor);
        }
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        return;
    }

    if (!m_style.outline && !m_style.fill)
    {
        return;
    }
    HPEN pen = m_style.outline ? CreateLinePen(m_style.outlineColor, width) : nullptr;
    HBRUSH brush = m_style.fill ? ::CreateSolidBrush(m_style.fillColor) : nullptr;
    HGDIOBJ oldPen = ::SelectObject(dc, pen != nullptr ? pen : ::GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = ::SelectObject(dc, brush != nullptr ? brush : ::GetStockObject(NULL_BRUSH));
    const int oldFillMode = ::SetPolyFillMode(dc, ALTERNATE);
    ::Polygon(dc, points.data(), count);
    ::SetPolyFillMode(dc, oldFillMode);
    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    if (brush != nullptr)
    {
        ::DeleteObject(brush);
    }
    if (pen != nullptr)
    {
        ::DeleteObject(pen);
    }
}

void PathCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<uint8_t>(m_kind));
    writer.Write(static_cast<uint8_t>((m_style.outline ? 1 : 0) | (m_style.fill ? 2 : 0)));
    writer.Write(static_cast<uint32_t>(m_style.outlineColor));
    writer.Write(static_cast<uint32_t>(m_style.fillColor));
    writer.Write(static_cast<int32_t>(m_style.width));
    writer.Write(static_cast<uint32_t>(m_points.size()));
    for (const CPoint& p : m_points)
    {
        writer.Write(static_cast<int32_t>(p.x));
        writer.Write(static_cast<int32_t>(p.y));
    }
}

std::unique_ptr<PathCommand> PathCommand::Read(ByteReader& reader)
{
    const auto kind = static_cast<PathKind>(reader.Read<uint8_t>());
    const auto flags = reader.Read<uint8_t>();
    ShapeStyle style;
    style.outline = (flags & 1) != 0;
    style.fill = (flags & 2) != 0;
    style.outlineColor = reader.Read<uint32_t>() & 0x00FFFFFF;
    style.fillColor = reader.Read<uint32_t>() & 0x00FFFFFF;
    style.width = reader.Read<int32_t>();
    const auto count = reader.Read<uint32_t>();
    if (!IsValidPointCount(kind, count) || style.width < 1 || style.width > kMaxWidth)
    {
        throw std::runtime_error("PathCommand: 잘못된 데이터입니다.");
    }
    std::vector<CPoint> points;
    points.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto x = reader.Read<int32_t>();
        const auto y = reader.Read<int32_t>();
        points.emplace_back(x, y);
    }
    return std::make_unique<PathCommand>(kind, std::move(points), style);
}
