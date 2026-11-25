#pragma once

/**
 * @file StrokeCommand.h
 * @brief 연필, 브러시, 지우개처럼 마우스 궤적을 따라 그리는 명령
 */

#include <vector>

#include "Core/Command.h"

/** @brief 궤적을 그리는 방식 */
enum class StrokeStyle : uint8_t
{
    Pencil = 0, ///< 정사각형 도장, 안티에일리어싱 없음
    Brush = 1,  ///< 둥근 펜
    Eraser = 2, ///< 정사각형 도장 (보통 배경색으로 그림)
};

/**
 * @brief 점 목록을 이어 그리는 명령
 */
class StrokeCommand : public Command
{
public:
    StrokeCommand(StrokeStyle style, COLORREF color, int width);

    /**
     * @brief 궤적에 점을 추가합니다.
     * @return 직전 점과 같아서 무시했으면 false
     */
    bool AddPoint(CPoint point);

    const std::vector<CPoint>& Points() const { return m_points; }
    int Width() const { return m_width; }

    /** @brief 두 점을 잇는 선분이 차지하는 영역 */
    static CRect SegmentBounds(CPoint a, CPoint b, int width);

    CommandType Type() const override { return CommandType::Stroke; }
    CRect Bounds() const override;
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    /** @brief Write로 쓴 본문을 읽습니다. */
    static std::unique_ptr<StrokeCommand> Read(ByteReader& reader);

private:
    void ExecuteStamps(HDC dc) const;
    void ExecuteRoundPen(HDC dc) const;

    StrokeStyle m_style;
    COLORREF m_color;
    int m_width;
    std::vector<CPoint> m_points;
};
