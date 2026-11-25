#pragma once

/**
 * @file RemoteViewTool.h
 * @brief 공유 화면을 보는 동안 캔버스에 끼우는 도구 (입력은 무시하고 방장의 미리보기만 보여 줌)
 */

#include <memory>

#include "Tools/Tool.h"

/**
 * @brief 방장의 미리보기를 보여 주는 보기 전용 도구
 */
class RemoteViewTool : public Tool
{
public:
    void OnDown(ToolContext& /*context*/, CPoint /*point*/, MouseButton /*button*/) override {}
    void OnMove(ToolContext& /*context*/, CPoint /*point*/) override {}
    void OnUp(ToolContext& /*context*/, CPoint /*point*/) override {}
    const Command* PreviewCommand() const override { return m_preview.get(); }
    CRect OverlayRect() const override { return m_overlay; }
    HCURSOR CursorAt(CPoint /*point*/) const override { return ::LoadCursor(nullptr, IDC_ARROW); }
    CRect TakePreviewDirty() override;

    /** @brief 방장이 보낸 미리보기와 선택 영역 테두리로 바꿉니다. */
    void SetPreview(std::unique_ptr<Command> preview, const CRect& overlay);

    /** @brief 미리보기를 지웁니다. */
    void Clear() { SetPreview(nullptr, CRect()); }

private:
    void MarkDirty();

    std::unique_ptr<Command> m_preview;
    CRect m_overlay;
    CRect m_dirty;
};
