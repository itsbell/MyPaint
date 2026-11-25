/**
 * @file StrokeCommand.cpp
 * @brief StrokeCommand 구현
 */
#include "pch.h"
#include "Core/StrokeCommand.h"

#include <algorithm>
#include <cstdlib>

#include "Core/ByteStream.h"
#include "Core/Surface.h"

namespace
{
constexpr int kMaxWidth = 1000;
constexpr uint32_t kMaxPoints = 1000000;

/** @brief 브레즌햄 직선 알고리즘으로 a부터 b까지(양 끝 포함) 각 픽셀에 fn을 호출합니다. */
template <typename Fn>
void ForEachLinePixel(CPoint a, CPoint b, Fn&& fn)
{
    const int dx = std::abs(b.x - a.x);
    const int dy = -std::abs(b.y - a.y);
    const int sx = a.x < b.x ? 1 : -1;
    const int sy = a.y < b.y ? 1 : -1;
    int err = dx + dy;
    int x = a.x;
    int y = a.y;
    for (;;)
    {
        fn(x, y);
        if (x == b.x && y == b.y)
        {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y += sy;
        }
    }
}

/** @brief 점 하나에 찍히는 width × width 도장 영역 */
CRect StampRect(int x, int y, int width)
{
    const int half = width / 2;
    return CRect(x - half, y - half, x - half + width, y - half + width);
}
} // namespace

StrokeCommand::StrokeCommand(StrokeStyle style, COLORREF color, int width)
    : m_style(style), m_color(color), m_width(std::clamp(width, 1, kMaxWidth))
{
}

bool StrokeCommand::AddPoint(CPoint point)
{
    if (!m_points.empty() && m_points.back() == point)
    {
        return false;
    }
    m_points.push_back(point);
    return true;
}

CRect StrokeCommand::SegmentBounds(CPoint a, CPoint b, int width)
{
    CRect r((std::min)(a.x, b.x), (std::min)(a.y, b.y), (std::max)(a.x, b.x) + 1, (std::max)(a.y, b.y) + 1);
    // 도장과 둥근 펜 모두 중심에서 width/2 + 1 안에 들어갑니다. 여유로 1을 더 둡니다.
    r.InflateRect(width / 2 + 2, width / 2 + 2);
    return r;
}

CRect StrokeCommand::Bounds() const
{
    if (m_points.empty())
    {
        return CRect();
    }
    CRect r(m_points.front(), m_points.front());
    for (const CPoint& p : m_points)
    {
        r.left = (std::min)(r.left, p.x);
        r.top = (std::min)(r.top, p.y);
        r.right = (std::max)(r.right, p.x);
        r.bottom = (std::max)(r.bottom, p.y);
    }
    return SegmentBounds(r.TopLeft(), r.BottomRight(), m_width);
}

void StrokeCommand::Execute(Surface& surface) const
{
    if (m_points.empty() || !surface.IsValid())
    {
        return;
    }
    if (m_style == StrokeStyle::Brush && m_width > 1)
    {
        ExecuteRoundPen(surface.DC());
    }
    else
    {
        ExecuteStamps(surface.DC());
    }
}

void StrokeCommand::ExecuteStamps(HDC dc) const
{
    // 미리보기는 DC에 클리핑을 걸고 다시 그리므로, 보이지 않는 도장은 건너뜁니다.
    CRect clip;
    const bool hasClip = ::GetClipBox(dc, &clip) != NULLREGION;
    if (!hasClip)
    {
        return;
    }

    HBRUSH brush = ::CreateSolidBrush(m_color);
    auto stamp = [&](int x, int y) {
        const CRect r = StampRect(x, y, m_width);
        CRect visible;
        if (visible.IntersectRect(r, clip))
        {
            ::FillRect(dc, &r, brush);
        }
    };

    if (m_points.size() == 1)
    {
        stamp(m_points.front().x, m_points.front().y);
    }
    for (size_t i = 1; i < m_points.size(); ++i)
    {
        // 선분 전체가 클리핑 밖이면 픽셀 단위로 돌지 않습니다.
        CRect visible;
        if (visible.IntersectRect(SegmentBounds(m_points[i - 1], m_points[i], m_width), clip))
        {
            ForEachLinePixel(m_points[i - 1], m_points[i], stamp);
        }
    }
    ::DeleteObject(brush);
}

void StrokeCommand::ExecuteRoundPen(HDC dc) const
{
    if (m_points.size() == 1)
    {
        // 길이 0인 선은 GDI가 그리지 않으므로 원으로 찍습니다.
        HBRUSH brush = ::CreateSolidBrush(m_color);
        HPEN pen = ::CreatePen(PS_SOLID, 1, m_color);
        HGDIOBJ oldBrush = ::SelectObject(dc, brush);
        HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const CRect r = StampRect(m_points.front().x, m_points.front().y, m_width);
        ::Ellipse(dc, r.left, r.top, r.right, r.bottom);
        ::SelectObject(dc, oldPen);
        ::SelectObject(dc, oldBrush);
        ::DeleteObject(pen);
        ::DeleteObject(brush);
        return;
    }

    LOGBRUSH lb{BS_SOLID, m_color, 0};
    HPEN pen = ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                              static_cast<DWORD>(m_width), &lb, 0, nullptr);
    HGDIOBJ oldPen = ::SelectObject(dc, pen);
    ::Polyline(dc, m_points.data(), static_cast<int>(m_points.size()));
    ::SelectObject(dc, oldPen);
    ::DeleteObject(pen);
}

void StrokeCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<uint8_t>(m_style));
    writer.Write(static_cast<uint32_t>(m_color));
    writer.Write(static_cast<int32_t>(m_width));
    writer.Write(static_cast<uint32_t>(m_points.size()));
    for (const CPoint& p : m_points)
    {
        writer.Write(static_cast<int32_t>(p.x));
        writer.Write(static_cast<int32_t>(p.y));
    }
}

std::unique_ptr<StrokeCommand> StrokeCommand::Read(ByteReader& reader)
{
    const auto style = reader.Read<uint8_t>();
    const auto color = reader.Read<uint32_t>();
    const auto width = reader.Read<int32_t>();
    const auto count = reader.Read<uint32_t>();
    if (style > static_cast<uint8_t>(StrokeStyle::Eraser) || width < 1 || width > kMaxWidth || count > kMaxPoints)
    {
        throw std::runtime_error("StrokeCommand: 잘못된 데이터입니다.");
    }

    auto command = std::make_unique<StrokeCommand>(static_cast<StrokeStyle>(style), color & 0x00FFFFFF, width);
    command->m_points.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        const auto x = reader.Read<int32_t>();
        const auto y = reader.Read<int32_t>();
        command->m_points.emplace_back(x, y);
    }
    return command;
}
