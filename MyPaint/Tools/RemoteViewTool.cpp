/**
 * @file RemoteViewTool.cpp
 * @brief RemoteViewTool 구현
 */
#include "pch.h"
#include "Tools/RemoteViewTool.h"

#include "Core/Command.h"

void RemoteViewTool::MarkDirty()
{
    if (m_preview)
    {
        m_dirty |= m_preview->Bounds();
    }
    if (!m_overlay.IsRectEmpty())
    {
        CRect frame = m_overlay;
        frame.InflateRect(2, 2); // 테두리는 영역 바로 바깥에 그려집니다.
        m_dirty |= frame;
    }
}

void RemoteViewTool::SetPreview(std::unique_ptr<Command> preview, const CRect& overlay)
{
    MarkDirty(); // 이전 모양을 지우기 위해
    m_preview = std::move(preview);
    m_overlay = overlay;
    MarkDirty();
}

CRect RemoteViewTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}
