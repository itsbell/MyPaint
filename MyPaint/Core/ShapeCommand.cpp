/**
 * @file ShapeCommand.cpp
 * @brief ShapeCommand와 도형 그리기 구현
 */
#include "pch.h"
#include "Core/ShapeCommand.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "Core/ByteStream.h"
#include "Core/Surface.h"

namespace
{
constexpr int kMaxWidth = 1000;
constexpr double kPi = 3.14159265358979323846;

/** @brief 단위 좌표 (0~1) */
struct UnitPoint
{
    double u;
    double v;
};

/** @brief 점 목록을 자기 경계 상자에 꽉 차도록 0~1로 정규화합니다. */
std::vector<UnitPoint> Normalize(std::vector<UnitPoint> points)
{
    double minU = points.front().u, maxU = minU, minV = points.front().v, maxV = minV;
    for (const UnitPoint& p : points)
    {
        minU = (std::min)(minU, p.u);
        maxU = (std::max)(maxU, p.u);
        minV = (std::min)(minV, p.v);
        maxV = (std::max)(maxV, p.v);
    }
    for (UnitPoint& p : points)
    {
        p.u = (p.u - minU) / (maxU - minU);
        p.v = (p.v - minV) / (maxV - minV);
    }
    return points;
}

/** @brief 원 위에 꼭짓점을 고르게 놓은 다각형/별 (inner가 1이면 정다각형) */
std::vector<UnitPoint> RadialPolygon(int corners, double inner, double startDegrees)
{
    const int count = inner < 1.0 ? corners * 2 : corners;
    std::vector<UnitPoint> points;
    for (int i = 0; i < count; ++i)
    {
        const double radius = (inner < 1.0 && i % 2 == 1) ? inner : 1.0;
        const double angle = (startDegrees + 360.0 * i / count) * kPi / 180.0;
        points.push_back({radius * std::cos(angle), radius * std::sin(angle)});
    }
    return Normalize(points);
}

std::vector<UnitPoint> HeartPolygon()
{
    std::vector<UnitPoint> points;
    constexpr int kSamples = 48;
    for (int i = 0; i < kSamples; ++i)
    {
        const double t = 2.0 * kPi * i / kSamples;
        const double s = std::sin(t);
        points.push_back({16.0 * s * s * s,
                          -(13.0 * std::cos(t) - 5.0 * std::cos(2 * t) - 2.0 * std::cos(3 * t) - std::cos(4 * t))});
    }
    return Normalize(points);
}

/** @brief 다각형 도형의 단위 꼭짓점. 다각형이 아니면 빈 목록 */
std::vector<UnitPoint> UnitPolygon(ShapeKind kind)
{
    // 오른쪽 화살표를 기준으로 나머지 방향을 만듭니다.
    static const std::vector<UnitPoint> kArrowRight = {{0, 0.25}, {0.5, 0.25}, {0.5, 0},  {1, 0.5},
                                                       {0.5, 1},  {0.5, 0.75}, {0, 0.75}};
    auto mapArrow = [](auto fn) {
        std::vector<UnitPoint> points;
        for (const UnitPoint& p : kArrowRight)
        {
            points.push_back(fn(p));
        }
        return points;
    };

    switch (kind)
    {
    case ShapeKind::Triangle:
        return {{0.5, 0}, {1, 1}, {0, 1}};
    case ShapeKind::RightTriangle:
        return {{0, 0}, {1, 1}, {0, 1}};
    case ShapeKind::Diamond:
        return {{0.5, 0}, {1, 0.5}, {0.5, 1}, {0, 0.5}};
    case ShapeKind::Pentagon:
        return RadialPolygon(5, 1.0, -90.0);
    case ShapeKind::Hexagon:
        return RadialPolygon(6, 1.0, 0.0);
    case ShapeKind::ArrowRight:
        return kArrowRight;
    case ShapeKind::ArrowLeft:
        return mapArrow([](UnitPoint p) { return UnitPoint{1 - p.u, p.v}; });
    case ShapeKind::ArrowUp:
        return mapArrow([](UnitPoint p) { return UnitPoint{p.v, 1 - p.u}; });
    case ShapeKind::ArrowDown:
        return mapArrow([](UnitPoint p) { return UnitPoint{p.v, p.u}; });
    case ShapeKind::Star4:
        return RadialPolygon(4, 0.4, -90.0);
    case ShapeKind::Star5:
        return RadialPolygon(5, 0.382, -90.0);
    case ShapeKind::Star6:
        return RadialPolygon(6, 0.577, -90.0);
    case ShapeKind::Heart:
        return HeartPolygon();
    default:
        return {};
    }
}

/** @brief 시작점과 끝점을 포함하는 상자 (오른쪽/아래도 포함하는 좌표) */
CRect InclusiveBox(CPoint a, CPoint b)
{
    return CRect((std::min)(a.x, b.x), (std::min)(a.y, b.y), (std::max)(a.x, b.x), (std::max)(a.y, b.y));
}

int Sign(int value)
{
    return value < 0 ? -1 : 1;
}

/** @brief 선 그리기용 펜. 두께 1은 단순 펜, 그보다 굵으면 끝이 둥근 펜 */
HPEN CreateLinePen(COLORREF color, int width)
{
    if (width <= 1)
    {
        return ::CreatePen(PS_SOLID, 1, color);
    }
    LOGBRUSH lb{BS_SOLID, color, 0};
    return ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, static_cast<DWORD>(width), &lb,
                          0, nullptr);
}
} // namespace

const wchar_t* ShapeKindName(ShapeKind kind)
{
    switch (kind)
    {
    case ShapeKind::Line: return L"선";
    case ShapeKind::Ellipse: return L"타원";
    case ShapeKind::Rectangle: return L"직사각형";
    case ShapeKind::RoundRect: return L"둥근 직사각형";
    case ShapeKind::Triangle: return L"삼각형";
    case ShapeKind::RightTriangle: return L"직각 삼각형";
    case ShapeKind::Diamond: return L"마름모";
    case ShapeKind::Pentagon: return L"오각형";
    case ShapeKind::Hexagon: return L"육각형";
    case ShapeKind::ArrowRight: return L"오른쪽 화살표";
    case ShapeKind::ArrowLeft: return L"왼쪽 화살표";
    case ShapeKind::ArrowUp: return L"위쪽 화살표";
    case ShapeKind::ArrowDown: return L"아래쪽 화살표";
    case ShapeKind::Star4: return L"4각 별";
    case ShapeKind::Star5: return L"5각 별";
    case ShapeKind::Star6: return L"6각 별";
    case ShapeKind::Heart: return L"하트";
    default: return L"";
    }
}

void DrawShape(HDC dc, ShapeKind kind, CPoint start, CPoint end, const ShapeStyle& style)
{
    const int width = std::clamp(style.width, 1, kMaxWidth);

    if (kind == ShapeKind::Line)
    {
        HPEN pen = CreateLinePen(style.outlineColor, width);
        HGDIOBJ oldPen = ::SelectObject(dc, pen);
        ::MoveToEx(dc, start.x, start.y, nullptr);
        ::LineTo(dc, end.x, end.y);
        if (width <= 1)
        {
            ::SetPixelV(dc, end.x, end.y, style.outlineColor); // LineTo는 끝점을 그리지 않습니다.
        }
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
        return;
    }

    if (!style.outline && !style.fill)
    {
        return;
    }

    HPEN pen = nullptr;
    if (style.outline)
    {
        if (width <= 1)
        {
            pen = ::CreatePen(PS_SOLID, 1, style.outlineColor);
        }
        else
        {
            // PS_INSIDEFRAME: 사각형/타원 계열은 윤곽선이 드래그한 상자 밖으로 나가지 않습니다.
            LOGBRUSH lb{BS_SOLID, style.outlineColor, 0};
            pen = ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_INSIDEFRAME | PS_ENDCAP_FLAT | PS_JOIN_MITER,
                                 static_cast<DWORD>(width), &lb, 0, nullptr);
        }
    }
    HBRUSH brush = style.fill ? ::CreateSolidBrush(style.fillColor) : nullptr;
    HGDIOBJ oldPen = ::SelectObject(dc, pen != nullptr ? pen : ::GetStockObject(NULL_PEN));
    HGDIOBJ oldBrush = ::SelectObject(dc, brush != nullptr ? brush : ::GetStockObject(NULL_BRUSH));

    const CRect box = InclusiveBox(start, end);
    // GDI 사각형 함수는 오른쪽/아래를 제외하고, 펜이 없으면 1픽셀 더 작게 칠합니다.
    const int extra = style.outline ? 1 : 2;
    const int right = box.right + extra;
    const int bottom = box.bottom + extra;

    switch (kind)
    {
    case ShapeKind::Ellipse:
        ::Ellipse(dc, box.left, box.top, right, bottom);
        break;
    case ShapeKind::Rectangle:
        ::Rectangle(dc, box.left, box.top, right, bottom);
        break;
    case ShapeKind::RoundRect:
    {
        const int corner = (std::min)({static_cast<int>(box.Width()), static_cast<int>(box.Height()), 48}) / 2 + 2;
        ::RoundRect(dc, box.left, box.top, right, bottom, corner, corner);
        break;
    }
    default:
    {
        const std::vector<UnitPoint> unit = UnitPolygon(kind);
        if (unit.empty())
        {
            break;
        }
        // 다각형에는 PS_INSIDEFRAME이 적용되지 않으므로 두께의 절반만큼 안쪽으로 줄여서 그립니다.
        const double inset = (style.outline && width > 1) ? width / 2.0 : 0.0;
        const double l = box.left + inset;
        const double t = box.top + inset;
        const double w = (std::max)(0.0, box.Width() - 2 * inset);
        const double h = (std::max)(0.0, box.Height() - 2 * inset);
        std::vector<POINT> points;
        points.reserve(unit.size());
        for (const UnitPoint& p : unit)
        {
            points.push_back({static_cast<LONG>(std::lround(l + p.u * w)), static_cast<LONG>(std::lround(t + p.v * h))});
        }
        ::Polygon(dc, points.data(), static_cast<int>(points.size()));
        break;
    }
    }

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

CPoint ConstrainShapeEnd(ShapeKind kind, CPoint start, CPoint end)
{
    const int dx = end.x - start.x;
    const int dy = end.y - start.y;
    const int adx = std::abs(dx);
    const int ady = std::abs(dy);

    if (kind == ShapeKind::Line)
    {
        constexpr double kTan22_5 = 0.41421356;
        if (ady < adx * kTan22_5)
        {
            return CPoint(end.x, start.y); // 수평
        }
        if (adx < ady * kTan22_5)
        {
            return CPoint(start.x, end.y); // 수직
        }
    }
    // 대각선 또는 정사각형 상자
    const int m = (std::max)(adx, ady);
    return CPoint(start.x + Sign(dx) * m, start.y + Sign(dy) * m);
}

CRect ShapeCommand::Bounds() const
{
    CRect r = InclusiveBox(m_start, m_end);
    r.right += 1;
    r.bottom += 1;
    const int margin = std::clamp(m_style.width, 1, kMaxWidth) + 2;
    r.InflateRect(margin, margin);
    return r;
}

void ShapeCommand::Execute(Surface& surface) const
{
    if (surface.IsValid())
    {
        DrawShape(surface.DC(), m_kind, m_start, m_end, m_style);
    }
}

void ShapeCommand::Write(ByteWriter& writer) const
{
    writer.Write(static_cast<uint8_t>(m_kind));
    writer.Write(static_cast<int32_t>(m_start.x));
    writer.Write(static_cast<int32_t>(m_start.y));
    writer.Write(static_cast<int32_t>(m_end.x));
    writer.Write(static_cast<int32_t>(m_end.y));
    writer.Write(static_cast<uint8_t>((m_style.outline ? 1 : 0) | (m_style.fill ? 2 : 0)));
    writer.Write(static_cast<uint32_t>(m_style.outlineColor));
    writer.Write(static_cast<uint32_t>(m_style.fillColor));
    writer.Write(static_cast<int32_t>(m_style.width));
}

std::unique_ptr<ShapeCommand> ShapeCommand::Read(ByteReader& reader)
{
    const auto kind = reader.Read<uint8_t>();
    const auto sx = reader.Read<int32_t>();
    const auto sy = reader.Read<int32_t>();
    const auto ex = reader.Read<int32_t>();
    const auto ey = reader.Read<int32_t>();
    const auto flags = reader.Read<uint8_t>();
    ShapeStyle style;
    style.outlineColor = reader.Read<uint32_t>() & 0x00FFFFFF;
    style.fillColor = reader.Read<uint32_t>() & 0x00FFFFFF;
    style.width = reader.Read<int32_t>();
    if (kind >= static_cast<uint8_t>(ShapeKind::Count) || style.width < 1 || style.width > kMaxWidth)
    {
        throw std::runtime_error("ShapeCommand: 잘못된 데이터입니다.");
    }
    style.outline = (flags & 1) != 0;
    style.fill = (flags & 2) != 0;
    return std::make_unique<ShapeCommand>(static_cast<ShapeKind>(kind), CPoint(sx, sy), CPoint(ex, ey), style);
}
