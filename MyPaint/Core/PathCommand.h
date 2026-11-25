#pragma once

/**
 * @file PathCommand.h
 * @brief 점 여러 개로 정해지는 곡선과 다각형을 그리는 명령
 */

#include <vector>

#include "Core/Command.h"
#include "Core/ShapeCommand.h"

/** @brief 경로 종류. 직렬화에 쓰이므로 값을 바꾸지 마세요. */
enum class PathKind : uint8_t
{
    Curve = 0,    ///< 3차 베지어 곡선 (점 4개: 시작, 조절점 1, 조절점 2, 끝)
    Polyline = 1, ///< 열린 꺾은선 (다각형을 그리는 도중의 미리보기)
    Polygon = 2,  ///< 닫힌 다각형 (윤곽선/채우기)
};

/**
 * @brief 곡선/다각형 명령
 *
 * 선과 꺾은선은 윤곽선 색으로만 그리고, 닫힌 다각형은 ShapeStyle의 윤곽선/채우기 설정을 따릅니다.
 */
class PathCommand : public Command
{
public:
    /** @brief 다각형 꼭짓점 최대 수 */
    static constexpr uint32_t kMaxPoints = 10000;

    PathCommand(PathKind kind, std::vector<CPoint> points, const ShapeStyle& style)
        : m_kind(kind), m_points(std::move(points)), m_style(style)
    {
    }

    CommandType Type() const override { return CommandType::Path; }
    CRect Bounds() const override;
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<PathCommand> Read(ByteReader& reader);

private:
    PathKind m_kind;
    std::vector<CPoint> m_points;
    ShapeStyle m_style;
};
