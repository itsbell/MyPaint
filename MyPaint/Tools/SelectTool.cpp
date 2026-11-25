/**
 * @file SelectTool.cpp
 * @brief SelectTool 구현
 */
#include "pch.h"
#include "Tools/SelectTool.h"

#include "Core/Document.h"

void SelectTool::OnDown(ToolContext& context, CPoint point, MouseButton /*button*/)
{
    if (m_hasSelection && m_selection.PtInRect(point))
    {
        if (!m_floating)
        {
            Lift(context);
        }
        m_drag = Drag::Moving;
        m_grab = point - m_selection.TopLeft();
        m_dragStartSelection = m_selection;
        return;
    }

    // 선택 영역 밖을 누르면 지금 선택을 확정하고 새로 선택을 시작합니다.
    Commit(context);
    m_drag = Drag::Selecting;
    m_anchor = point;
}

void SelectTool::OnMove(ToolContext& context, CPoint point)
{
    switch (m_drag)
    {
    case Drag::Selecting:
    {
        CRect r((std::min)(m_anchor.x, point.x), (std::min)(m_anchor.y, point.y),
                (std::max)(m_anchor.x, point.x) + 1, (std::max)(m_anchor.y, point.y) + 1);
        r &= context.document.GetSurface().Bounds();
        MarkDirty();
        m_selection = r;
        m_hasSelection = !r.IsRectEmpty();
        MarkDirty();
        break;
    }
    case Drag::Moving:
        MarkDirty();
        m_selection = CRect(CPoint(point.x - m_grab.x, point.y - m_grab.y), m_selection.Size());
        RebuildPreview();
        MarkDirty();
        break;
    default:
        break;
    }
}

void SelectTool::OnUp(ToolContext& context, CPoint point)
{
    OnMove(context, point);
    // 한 픽셀만 클릭한 것은 선택으로 치지 않습니다.
    if (m_drag == Drag::Selecting && m_selection.Width() <= 1 && m_selection.Height() <= 1)
    {
        Discard();
    }
    m_drag = Drag::None;
}

void SelectTool::Cancel()
{
    if (m_drag == Drag::Moving)
    {
        MarkDirty();
        m_selection = m_dragStartSelection;
        RebuildPreview();
        MarkDirty();
    }
    else if (m_drag == Drag::Selecting)
    {
        Discard();
    }
    m_drag = Drag::None;
}

void SelectTool::Commit(ToolContext& context)
{
    m_drag = Drag::None;
    if (m_floating && m_pixels)
    {
        const bool unchanged = m_clearOrigin && m_selection == m_origin;
        if (!unchanged)
        {
            context.document.Apply(std::make_unique<PatchCommand>(m_clearOrigin ? m_origin : CRect(), m_background,
                                                                  m_selection.TopLeft(), m_selection.Size(), m_pixels));
        }
    }
    Discard();
}

void SelectTool::Discard()
{
    MarkDirty();
    m_drag = Drag::None;
    m_hasSelection = false;
    m_selection.SetRectEmpty();
    m_floating = false;
    m_clearOrigin = false;
    m_origin.SetRectEmpty();
    m_pixels.reset();
    m_preview.reset();
}

void SelectTool::Lift(ToolContext& context)
{
    PixelBlock block = context.document.GetSurface().ReadBlock(m_selection);
    if (block.IsEmpty())
    {
        return;
    }
    m_selection = block.rect;
    m_origin = block.rect;
    m_pixels = std::make_shared<const std::vector<Pixel>>(std::move(block.pixels));
    m_background = context.settings.color2;
    m_clearOrigin = true;
    m_floating = true;
    RebuildPreview();
    MarkDirty();
}

void SelectTool::RebuildPreview()
{
    if (m_floating && m_pixels)
    {
        m_preview = std::make_unique<PatchCommand>(m_clearOrigin ? m_origin : CRect(), m_background,
                                                   m_selection.TopLeft(), m_selection.Size(), m_pixels);
    }
    else
    {
        m_preview.reset();
    }
}

void SelectTool::MarkDirty()
{
    if (m_hasSelection)
    {
        CRect r = m_selection;
        r.InflateRect(1, 1); // 테두리는 선택 영역 바로 바깥에 그립니다.
        m_dirty |= r;
    }
    if (m_floating)
    {
        m_dirty |= m_origin;
    }
}

CRect SelectTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}

HCURSOR SelectTool::CursorAt(CPoint point) const
{
    if (m_hasSelection && m_selection.PtInRect(point))
    {
        return ::LoadCursor(nullptr, IDC_SIZEALL);
    }
    return ::LoadCursor(nullptr, IDC_CROSS);
}

void SelectTool::SelectAll(ToolContext& context)
{
    Commit(context);
    m_selection = context.document.GetSurface().Bounds();
    m_hasSelection = !m_selection.IsRectEmpty();
    MarkDirty();
}

bool SelectTool::GetSelectionPixels(const Document& document, CSize& size, std::vector<Pixel>& pixels) const
{
    if (!m_hasSelection)
    {
        return false;
    }
    if (m_floating && m_pixels)
    {
        size = m_selection.Size();
        pixels = *m_pixels;
        return true;
    }
    PixelBlock block = document.GetSurface().ReadBlock(m_selection);
    if (block.IsEmpty())
    {
        return false;
    }
    size = block.rect.Size();
    pixels = std::move(block.pixels);
    return true;
}

void SelectTool::DeleteSelection(ToolContext& context)
{
    if (!m_hasSelection)
    {
        return;
    }
    if (!m_floating)
    {
        context.document.Apply(PatchCommand::MakeClear(m_selection, context.settings.color2));
    }
    else if (m_clearOrigin)
    {
        // 들어서 옮기던 내용은 버리고, 원래 자리만 지웁니다.
        context.document.Apply(PatchCommand::MakeClear(m_origin, m_background));
    }
    // 붙여넣은 채 떠 있던 내용은 문서에 들어간 적이 없으므로 버리기만 하면 됩니다.
    Discard();
}

void SelectTool::PasteFloating(ToolContext& context, CSize size, std::vector<Pixel>&& pixels, CPoint at)
{
    Commit(context);
    m_pixels = std::make_shared<const std::vector<Pixel>>(std::move(pixels));
    m_selection = CRect(at, size);
    m_hasSelection = true;
    m_floating = true;
    m_clearOrigin = false;
    m_origin.SetRectEmpty();
    RebuildPreview();
    MarkDirty();
}
