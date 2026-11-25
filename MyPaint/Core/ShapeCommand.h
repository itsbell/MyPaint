#pragma once

/**
 * @file ShapeCommand.h
 * @brief 선, 사각형, 타원, 다각형 같은 도형을 그리는 명령
 */

#include "Core/Command.h"

/** @brief 도형 종류. 직렬화에 쓰이므로 순서를 바꾸지 말고 끝에만 추가하세요. */
enum class ShapeKind : uint8_t
{
    Line,
    Ellipse,
    Rectangle,
    RoundRect,
    Triangle,
    RightTriangle,
    Diamond,
    Pentagon,
    Hexagon,
    ArrowRight,
    ArrowLeft,
    ArrowUp,
    ArrowDown,
    Star4,
    Star5,
    Star6,
    Heart,
    Curve,   ///< 곡선: 여러 단계로 그리며 PathCommand가 됩니다.
    Polygon, ///< 다각형: 여러 단계로 그리며 PathCommand가 됩니다.
    Count,
};

/** @brief 도구 패널에 보여 줄 도형 순서 (그림판과 비슷하게 선, 곡선, 타원 …) */
constexpr ShapeKind kShapeDisplayOrder[] = {
    ShapeKind::Line,         ShapeKind::Curve,      ShapeKind::Ellipse,   ShapeKind::Rectangle,
    ShapeKind::RoundRect,    ShapeKind::Polygon,    ShapeKind::Triangle,  ShapeKind::RightTriangle,
    ShapeKind::Diamond,      ShapeKind::Pentagon,   ShapeKind::Hexagon,   ShapeKind::ArrowRight,
    ShapeKind::ArrowLeft,    ShapeKind::ArrowUp,    ShapeKind::ArrowDown, ShapeKind::Star4,
    ShapeKind::Star5,        ShapeKind::Star6,      ShapeKind::Heart,
};

/** @brief 끌기 한 번이 아니라 여러 번 클릭해서 그리는 도형인지 */
inline bool IsPathShape(ShapeKind kind)
{
    return kind == ShapeKind::Curve || kind == ShapeKind::Polygon;
}

/** @brief 도구 패널 툴팁 등에 쓰는 이름 */
const wchar_t* ShapeKindName(ShapeKind kind);

/** @brief 선 그리기용 펜. 두께 1은 단순 펜, 그보다 굵으면 끝과 꺾인 곳이 둥근 펜 */
HPEN CreateLinePen(COLORREF color, int width);

/** @brief 도형의 윤곽선과 채우기 */
struct ShapeStyle
{
    bool outline = true;                     ///< 윤곽선 그리기
    bool fill = false;                       ///< 안쪽 채우기
    COLORREF outlineColor = RGB(0, 0, 0);    ///< 윤곽선 색 (선 도형은 항상 이 색)
    COLORREF fillColor = RGB(255, 255, 255); ///< 채우기 색
    int width = 1;                           ///< 윤곽선 두께
};

/**
 * @brief 시작점과 끝점으로 정해지는 상자 안에 도형을 그립니다.
 *
 * 명령 실행과 도구 패널 아이콘이 같은 함수를 써서 모양이 일치합니다.
 */
void DrawShape(HDC dc, ShapeKind kind, CPoint start, CPoint end, const ShapeStyle& style);

/**
 * @brief Shift를 누르고 그릴 때의 끝점 보정
 *
 * 선은 45° 단위로, 나머지 도형은 정사각형 상자로 맞춥니다.
 */
CPoint ConstrainShapeEnd(ShapeKind kind, CPoint start, CPoint end);

/**
 * @brief 도형 명령
 */
class ShapeCommand : public Command
{
public:
    ShapeCommand(ShapeKind kind, CPoint start, CPoint end, const ShapeStyle& style)
        : m_kind(kind), m_start(start), m_end(end), m_style(style)
    {
    }

    CommandType Type() const override { return CommandType::Shape; }
    CRect Bounds() const override;
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<ShapeCommand> Read(ByteReader& reader);

private:
    ShapeKind m_kind;
    CPoint m_start;
    CPoint m_end;
    ShapeStyle m_style;
};
