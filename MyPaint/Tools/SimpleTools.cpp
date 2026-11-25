/**
 * @file SimpleTools.cpp
 * @brief 채우기, 스포이트, 돋보기 도구 구현
 */
#include "pch.h"
#include "Tools/SimpleTools.h"

#include "Core/Document.h"
#include "Core/FillCommand.h"

void FillTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    context.document.Apply(std::make_unique<FillCommand>(point, context.settings.ColorFor(button)));
}

void PickerTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    m_active = true;
    m_button = button;
    Pick(context, point);
}

void PickerTool::OnMove(ToolContext& context, CPoint point)
{
    if (m_active)
    {
        Pick(context, point);
    }
}

void PickerTool::OnUp(ToolContext& context, CPoint point)
{
    if (!m_active)
    {
        return;
    }
    Pick(context, point);
    m_active = false;
    context.host.RestorePreviousTool();
}

void PickerTool::Pick(ToolContext& context, CPoint point)
{
    const Surface& surface = context.document.GetSurface();
    if (surface.Bounds().PtInRect(point))
    {
        context.host.SetToolColor(m_button, ToColorRef(surface.GetPixel(point.x, point.y)));
    }
}

void MagnifierTool::OnDown(ToolContext& context, CPoint point, MouseButton button)
{
    context.host.ZoomAt(point, button == MouseButton::Left);
}
