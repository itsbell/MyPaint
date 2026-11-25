#pragma once

/**
 * @file ShapeTool.h
 * @brief 도형 도구
 */

#include <memory>

#include "Core/ShapeCommand.h"
#include "Tools/Tool.h"

/**
 * @brief 드래그한 상자에 현재 선택된 도형을 그립니다.
 *
 * 왼쪽 버튼은 윤곽선 = 색 1, 채우기 = 색 2이고 오른쪽 버튼은 반대입니다.
 * Shift를 누르면 선은 45° 단위, 다른 도형은 정사각형 비율로 고정됩니다.
 */
class ShapeTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override;
    bool IsActive() const override { return m_preview != nullptr; }
    const Command* PreviewCommand() const override { return m_preview.get(); }
    CRect TakePreviewDirty() override;

private:
    void UpdatePreview(ToolContext& context, CPoint point);

    CPoint m_start;
    MouseButton m_button = MouseButton::Left;
    ShapeKind m_kind = ShapeKind::Rectangle;
    ShapeStyle m_style;
    std::unique_ptr<ShapeCommand> m_preview;
    CRect m_dirty;
};
