#pragma once

/**
 * @file SelectTool.h
 * @brief 사각형 선택 도구 (선택, 이동, 붙여넣기한 그림 배치)
 */

#include <memory>
#include <vector>

#include "Core/ImageCommands.h"
#include "Tools/Tool.h"

/**
 * @brief 사각형 선택 도구
 *
 * - 빈 곳을 끌면 선택 영역을 만듭니다.
 * - 선택 영역 안을 끌면 내용이 들려서(원래 자리는 색 2로 지워짐) 따라 움직입니다.
 * - 들린 내용은 다른 곳을 클릭하거나 도구를 바꿀 때 PatchCommand 하나로 확정됩니다.
 *   그래서 이동 전체가 실행 취소 한 번으로 되돌아갑니다.
 */
class SelectTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override;
    void Commit(ToolContext& context) override;
    bool IsActive() const override { return m_drag != Drag::None; }
    const Command* PreviewCommand() const override { return m_preview.get(); }
    CRect TakePreviewDirty() override;
    CRect OverlayRect() const override { return m_hasSelection ? m_selection : CRect(); }
    HCURSOR CursorAt(CPoint point) const override;

    bool HasSelection() const { return m_hasSelection; }
    CRect SelectionRect() const { return m_hasSelection ? m_selection : CRect(); }

    /** @brief 캔버스 전체를 선택합니다. (지금 선택은 먼저 확정) */
    void SelectAll(ToolContext& context);

    /** @brief 선택 영역의 현재 내용 (들려 있으면 들린 픽셀). 선택이 없으면 false */
    bool GetSelectionPixels(const Document& document, CSize& size, std::vector<Pixel>& pixels) const;

    /** @brief 선택 영역을 지웁니다. 캔버스에 있던 자리는 색 2로 채웁니다. */
    void DeleteSelection(ToolContext& context);

    /** @brief 붙여넣은 그림을 at 위치에 떠 있는 선택 영역으로 둡니다. (지금 선택은 먼저 확정) */
    void PasteFloating(ToolContext& context, CSize size, std::vector<Pixel>&& pixels, CPoint at);

    /** @brief 확정하지 않고 선택을 버립니다. (자르기처럼 캔버스가 통째로 바뀐 뒤) */
    void Discard();

private:
    enum class Drag
    {
        None,
        Selecting, ///< 새 선택 영역을 그리는 중
        Moving,    ///< 선택 내용을 옮기는 중
    };

    /** @brief 선택 영역의 픽셀을 들어 올립니다. 원래 자리는 확정할 때 bg로 지워집니다. */
    void Lift(ToolContext& context);

    /** @brief 들린 내용을 현재 위치에 그리는 미리보기 명령을 다시 만듭니다. */
    void RebuildPreview();

    /** @brief 테두리와 미리보기가 차지하는 영역을 다시 그리도록 표시합니다. */
    void MarkDirty();

    Drag m_drag = Drag::None;
    bool m_hasSelection = false;
    CRect m_selection; ///< 현재 위치 (문서 좌표)
    CPoint m_anchor;   ///< 선택을 시작한 점
    CPoint m_grab;     ///< 이동할 때 선택 영역 왼쪽 위와 마우스의 차이
    CRect m_dragStartSelection;

    bool m_floating = false; ///< 내용이 들려 있는지
    bool m_clearOrigin = false;
    CRect m_origin;    ///< 들어 올린 원래 자리
    COLORREF m_background = RGB(255, 255, 255);
    PatchCommand::PixelData m_pixels;

    std::unique_ptr<PatchCommand> m_preview;
    CRect m_dirty;
};
