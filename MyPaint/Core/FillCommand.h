#pragma once

/**
 * @file FillCommand.h
 * @brief 같은 색으로 이어진 영역을 채우는 명령 (페인트 통)
 */

#include <vector>

#include "Core/Command.h"

/**
 * @brief 시작점과 색이 정확히 같은 픽셀 중 상하좌우로 이어진 영역을 채웁니다.
 */
class FillCommand : public Command
{
public:
    FillCommand(CPoint seed, COLORREF color) : m_seed(seed), m_color(color) {}

    CommandType Type() const override { return CommandType::Fill; }

    /** @brief 표면 없이는 영역을 알 수 없으므로 시작점 한 픽셀만 돌려줍니다. */
    CRect Bounds() const override { return CRect(m_seed, CSize(1, 1)); }

    CRect ComputeBounds(const Surface& surface) const override;
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<FillCommand> Read(ByteReader& reader);

private:
    /** @brief 한 행에서 채울 구간 [x0, x1) */
    struct Span
    {
        int y;
        int x0;
        int x1;
    };

    /** @brief 채울 구간들을 찾습니다. 채울 것이 없으면 빈 목록 */
    std::vector<Span> FindSpans(const Surface& surface) const;

    CPoint m_seed;
    COLORREF m_color;
};
