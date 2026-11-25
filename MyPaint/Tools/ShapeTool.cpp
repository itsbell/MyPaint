/**
 * @file ShapeTool.cpp
 * @brief ShapeTool 구현
 */
#include "pch.h"
#include "Tools/ShapeTool.h"

#include <algorithm>
#include <cstdlib>

#include "Core/Document.h"
#include "Core/PathCommand.h"

namespace
{
/** @brief 다각형을 닫는 클릭으로 볼 거리 (문서 픽셀) */
constexpr int kCloseTolerance = 4;

bool IsNear(CPoint a, CPoint b, int tolerance)
{
    return std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
}
} // namespace

void ShapeTool::LoadStyle(const ToolSettings& s)
{
    m_style.outline = s.shapeOutline;
    m_style.fill = s.shapeFill;
    m_style.outlineColor = s.ColorFor(m_button);
    m_style.fillColor = s.OtherColor(m_button);
    m_style.width = s.WidthFor(WidthKind::Brush);
}

void ShapeTool::SetPreview(std::unique_ptr<Command> preview)
{
    if (m_preview)
    {
        m_dirty |= m_preview->Bounds(); // 이전 모양을 지우기 위해
    }
    m_preview = std::move(preview);
    if (m_preview)
    {
        m_dirty |= m_preview->Bounds();
    }
}

CRect ShapeTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}

// ---------------------------------------------------------------------------
// 공통 진입점
// ---------------------------------------------------------------------------

void ShapeTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    const ShapeKind kind = m_phase != PathPhase::None ? m_kind : context.settings.shapeKind;
    if (IsPathShape(kind))
    {
        PathDown(context, point, button);
        return;
    }

    // 드래그하는 동안 패널 설정이 바뀌어도 시작할 때의 모양을 유지합니다.
    m_mouseDown = true;
    m_start = point;
    m_button = button;
    m_kind = kind;
    LoadStyle(context.settings);
    UpdateShapePreview(context, point);
}

void ShapeTool::OnMove(ToolContext& context, CPoint point)
{
    if (!m_mouseDown)
    {
        return;
    }
    if (m_phase != PathPhase::None)
    {
        PathMove(context, point);
    }
    else
    {
        UpdateShapePreview(context, point);
    }
}

void ShapeTool::OnUp(ToolContext& context, CPoint point)
{
    if (!m_mouseDown)
    {
        return;
    }
    if (m_phase != PathPhase::None)
    {
        PathUp(context, point);
        return;
    }
    m_mouseDown = false;
    UpdateShapePreview(context, point);
    context.document.Apply(std::move(m_preview));
}

void ShapeTool::Cancel()
{
    // 곡선/다각형도 마우스를 누른 채 취소하면(오른쪽 클릭, 캡처 상실) 그리던 모양 전체를 버립니다.
    m_mouseDown = false;
    SetPreview(nullptr);
    ResetPath();
}

void ShapeTool::Commit(ToolContext& context)
{
    if (m_phase != PathPhase::None && !m_mouseDown)
    {
        FinishPath(context);
    }
}

void ShapeTool::OnSettingsChanged(ToolContext& context)
{
    // 그리다 만 곡선/다각형은 그림판처럼 바뀐 색과 두께를 바로 반영합니다.
    if (m_phase != PathPhase::None)
    {
        LoadStyle(context.settings);
        UpdatePathPreview();
    }
}

void ShapeTool::UpdateShapePreview(ToolContext& context, CPoint point)
{
    const CPoint end = context.shift ? ConstrainShapeEnd(m_kind, m_start, point) : point;
    SetPreview(std::make_unique<ShapeCommand>(m_kind, m_start, end, m_style));
}

// ---------------------------------------------------------------------------
// 곡선과 다각형
// ---------------------------------------------------------------------------

void ShapeTool::PathDown(ToolContext& context, CPoint point, MouseButton button)
{
    m_mouseDown = true;
    switch (m_phase)
    {
    case PathPhase::None:
        m_kind = context.settings.shapeKind;
        m_button = button;
        LoadStyle(context.settings);
        m_start = point;
        m_points.assign(m_kind == ShapeKind::Curve ? 4 : 2, point);
        m_phase = PathPhase::Line;
        break;
    case PathPhase::Vertex:
        m_points.push_back(point);
        break;
    default:
        break;
    }
    PathMove(context, point);
}

void ShapeTool::PathMove(ToolContext& context, CPoint point)
{
    switch (m_phase)
    {
    case PathPhase::Line:
    {
        const CPoint end = context.shift ? ConstrainShapeEnd(ShapeKind::Line, m_start, point) : point;
        if (m_kind == ShapeKind::Curve)
        {
            // 아직 구부리지 않은 곡선은 조절점이 양 끝에 있는 직선입니다.
            m_points = {m_start, m_start, end, end};
        }
        else
        {
            m_points[1] = end;
        }
        break;
    }
    case PathPhase::Bend1:
        m_points[1] = point;
        m_points[2] = point;
        break;
    case PathPhase::Bend2:
        m_points[2] = point;
        break;
    case PathPhase::Vertex:
    {
        const CPoint previous = m_points[m_points.size() - 2];
        m_points.back() = context.shift ? ConstrainShapeEnd(ShapeKind::Line, previous, point) : point;
        break;
    }
    default:
        return;
    }
    UpdatePathPreview();
}

void ShapeTool::PathUp(ToolContext& context, CPoint point)
{
    PathMove(context, point);
    m_mouseDown = false;

    switch (m_phase)
    {
    case PathPhase::Line:
        if (m_points.front() == m_points.back())
        {
            // 끌지 않고 클릭만 했으면 아무것도 그리지 않습니다.
            SetPreview(nullptr);
            ResetPath();
            return;
        }
        m_phase = m_kind == ShapeKind::Curve ? PathPhase::Bend1 : PathPhase::Vertex;
        break;
    case PathPhase::Bend1:
        m_phase = PathPhase::Bend2;
        break;
    case PathPhase::Bend2:
        FinishPath(context);
        break;
    case PathPhase::Vertex:
    {
        const size_t n = m_points.size();
        const int tolerance = (std::max)(kCloseTolerance, m_style.width / 2);
        if (n >= 4 && IsNear(m_points.back(), m_points.front(), tolerance))
        {
            // 시작점을 클릭해서 닫음
            m_points.pop_back();
            FinishPath(context);
        }
        else if (IsNear(m_points.back(), m_points[n - 2], 1))
        {
            // 같은 자리를 다시 클릭(두 번 클릭)하면 닫습니다.
            m_points.pop_back();
            if (m_points.size() >= 3)
            {
                FinishPath(context);
            }
            else
            {
                UpdatePathPreview();
            }
        }
        break;
    }
    default:
        break;
    }
}

void ShapeTool::UpdatePathPreview()
{
    if (m_phase == PathPhase::None)
    {
        return;
    }
    const PathKind kind = m_kind == ShapeKind::Curve ? PathKind::Curve : PathKind::Polyline;
    SetPreview(std::make_unique<PathCommand>(kind, m_points, m_style));
}

void ShapeTool::FinishPath(ToolContext& context)
{
    SetPreview(nullptr);
    std::unique_ptr<Command> command;
    if (m_kind == ShapeKind::Curve)
    {
        command = std::make_unique<PathCommand>(PathKind::Curve, m_points, m_style);
    }
    else if (m_points.size() >= 3)
    {
        command = std::make_unique<PathCommand>(PathKind::Polygon, m_points, m_style);
    }
    else if (m_points.size() == 2)
    {
        command = std::make_unique<PathCommand>(PathKind::Polyline, m_points, m_style); // 변 하나뿐이면 선
    }
    ResetPath();
    if (command)
    {
        context.document.Apply(std::move(command));
    }
}

void ShapeTool::ResetPath()
{
    m_phase = PathPhase::None;
    m_points.clear();
}
