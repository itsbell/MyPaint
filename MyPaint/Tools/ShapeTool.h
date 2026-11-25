#pragma once

/**
 * @file ShapeTool.h
 * @brief 도형 도구
 */

#include <memory>
#include <vector>

#include "Core/ShapeCommand.h"
#include "Tools/Tool.h"

/**
 * @brief 드래그한 상자에 현재 선택된 도형을 그립니다.
 *
 * 왼쪽 버튼은 윤곽선 = 색 1, 채우기 = 색 2이고 오른쪽 버튼은 반대입니다.
 * Shift를 누르면 선은 45° 단위, 다른 도형은 정사각형 비율로 고정됩니다.
 *
 * 곡선과 다각형은 그림판처럼 여러 번에 나눠 그립니다.
 * - 곡선: 끌어서 직선을 그린 뒤, 두 번 더 클릭(또는 끌기)해서 구부리면 확정됩니다.
 * - 다각형: 끌어서 첫 변을 그린 뒤 클릭할 때마다 꼭짓점이 늘어나고,
 *   시작점 근처를 클릭하거나 같은 자리를 두 번 클릭하면 닫혀서 확정됩니다.
 * 중간에 도구를 바꾸거나 Esc를 누르면 그때까지 그린 모양으로 확정됩니다.
 */
class ShapeTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override;
    void Commit(ToolContext& context) override;
    bool IsActive() const override { return m_mouseDown; }
    const Command* PreviewCommand() const override { return m_preview.get(); }
    CRect TakePreviewDirty() override;
    void OnSettingsChanged(ToolContext& context) override;

private:
    /** @brief 곡선/다각형 진행 단계 */
    enum class PathPhase
    {
        None,   ///< 그리는 중이 아님
        Line,   ///< 첫 선(변)을 끄는 중
        Bend1,  ///< 곡선: 첫 번째 구부리기
        Bend2,  ///< 곡선: 두 번째 구부리기
        Vertex, ///< 다각형: 꼭짓점 추가
    };

    void LoadStyle(const ToolSettings& settings);
    void UpdateShapePreview(ToolContext& context, CPoint point);

    void PathDown(ToolContext& context, CPoint point, MouseButton button);
    void PathMove(ToolContext& context, CPoint point);
    void PathUp(ToolContext& context, CPoint point);
    void UpdatePathPreview();
    void FinishPath(ToolContext& context);
    void ResetPath();

    /** @brief 미리보기를 바꾸고 이전/새 영역을 다시 그릴 영역에 더합니다. */
    void SetPreview(std::unique_ptr<Command> preview);

    CPoint m_start;
    MouseButton m_button = MouseButton::Left;
    ShapeKind m_kind = ShapeKind::Rectangle;
    ShapeStyle m_style;
    bool m_mouseDown = false;
    std::unique_ptr<Command> m_preview;
    CRect m_dirty;

    PathPhase m_phase = PathPhase::None;
    std::vector<CPoint> m_points; ///< 곡선: 시작, 조절점 1, 조절점 2, 끝 / 다각형: 꼭짓점
};
