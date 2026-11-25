/**
 * @file FreehandTool.cpp
 * @brief FreehandTool 구현
 */
#include "pch.h"
#include "Tools/FreehandTool.h"

#include "Core/Document.h"

void FreehandTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    const COLORREF color = m_style == StrokeStyle::Eraser ? context.settings.color2 : context.settings.ColorFor(button);
    const WidthKind widthKind = m_style == StrokeStyle::Pencil  ? WidthKind::Pencil
                                : m_style == StrokeStyle::Eraser ? WidthKind::Eraser
                                                                 : WidthKind::Brush;
    m_stroke = std::make_unique<StrokeCommand>(m_style, color, context.settings.WidthFor(widthKind));
    m_stroke->AddPoint(point);
    m_dirty |= m_stroke->Bounds();
}

void FreehandTool::OnMove(ToolContext& /*context*/, CPoint point)
{
    if (!m_stroke)
    {
        return;
    }
    const CPoint last = m_stroke->Points().back();
    if (m_stroke->AddPoint(point))
    {
        m_dirty |= StrokeCommand::SegmentBounds(last, point, m_stroke->Width());
    }
}

void FreehandTool::OnUp(ToolContext& context, CPoint point)
{
    if (!m_stroke)
    {
        return;
    }
    OnMove(context, point);
    // 문서가 바뀐 영역을 알려 주므로 미리보기 영역을 따로 지울 필요는 없지만, 안전하게 함께 갱신합니다.
    m_dirty |= m_stroke->Bounds();
    context.document.Apply(std::move(m_stroke));
}

void FreehandTool::Cancel()
{
    if (m_stroke)
    {
        m_dirty |= m_stroke->Bounds();
        m_stroke.reset();
    }
}

CRect FreehandTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}
