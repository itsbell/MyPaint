/**
 * @file ShapeTool.cpp
 * @brief ShapeTool 구현
 */
#include "pch.h"
#include "Tools/ShapeTool.h"

#include "Core/Document.h"

void ShapeTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    // 드래그하는 동안 패널 설정이 바뀌어도 시작할 때의 모양을 유지합니다.
    const ToolSettings& s = context.settings;
    m_start = point;
    m_button = button;
    m_kind = s.shapeKind;
    m_style.outline = s.shapeOutline;
    m_style.fill = s.shapeFill;
    m_style.outlineColor = s.ColorFor(button);
    m_style.fillColor = s.OtherColor(button);
    m_style.width = s.WidthFor(WidthKind::Brush);
    UpdatePreview(context, point);
}

void ShapeTool::OnMove(ToolContext& context, CPoint point)
{
    if (m_preview)
    {
        UpdatePreview(context, point);
    }
}

void ShapeTool::OnUp(ToolContext& context, CPoint point)
{
    if (!m_preview)
    {
        return;
    }
    UpdatePreview(context, point);
    context.document.Apply(std::move(m_preview));
}

void ShapeTool::Cancel()
{
    if (m_preview)
    {
        m_dirty |= m_preview->Bounds();
        m_preview.reset();
    }
}

void ShapeTool::UpdatePreview(ToolContext& context, CPoint point)
{
    const CPoint end = context.shift ? ConstrainShapeEnd(m_kind, m_start, point) : point;
    if (m_preview)
    {
        m_dirty |= m_preview->Bounds(); // 이전 모양을 지우기 위해
    }
    m_preview = std::make_unique<ShapeCommand>(m_kind, m_start, end, m_style);
    m_dirty |= m_preview->Bounds();
}

CRect ShapeTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}
