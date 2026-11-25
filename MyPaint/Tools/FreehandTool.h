#pragma once

/**
 * @file FreehandTool.h
 * @brief 연필, 브러시, 지우개 도구
 */

#include <memory>

#include "Core/StrokeCommand.h"
#include "Tools/Tool.h"

/**
 * @brief 마우스 궤적을 StrokeCommand로 기록하는 도구
 *
 * 연필과 브러시는 누른 버튼의 색으로, 지우개는 항상 색 2로 그립니다.
 */
class FreehandTool : public Tool
{
public:
    explicit FreehandTool(StrokeStyle style) : m_style(style) {}

    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override;
    bool IsActive() const override { return m_stroke != nullptr; }
    const Command* PreviewCommand() const override { return m_stroke.get(); }
    CRect TakePreviewDirty() override;

private:
    StrokeStyle m_style;
    std::unique_ptr<StrokeCommand> m_stroke;
    CRect m_dirty;
};
