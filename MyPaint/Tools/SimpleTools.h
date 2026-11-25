#pragma once

/**
 * @file SimpleTools.h
 * @brief 한 번 클릭으로 동작하는 도구들: 채우기, 스포이트, 돋보기
 */

#include "Tools/Tool.h"

/**
 * @brief 페인트 통. 누른 버튼의 색으로 이어진 영역을 채웁니다.
 */
class FillTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& /*context*/, CPoint /*point*/) override {}
    void OnUp(ToolContext& /*context*/, CPoint /*point*/) override {}
};

/**
 * @brief 스포이트. 누르고 끄는 동안 색을 집고, 놓으면 이전 도구로 돌아갑니다.
 */
class PickerTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override { m_active = false; }
    bool IsActive() const override { return m_active; }

private:
    void Pick(ToolContext& context, CPoint point);

    bool m_active = false;
    MouseButton m_button = MouseButton::Left;
};

/**
 * @brief 돋보기. 왼쪽 클릭은 확대, 오른쪽 클릭은 축소합니다.
 */
class MagnifierTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& /*context*/, CPoint /*point*/) override {}
    void OnUp(ToolContext& /*context*/, CPoint /*point*/) override {}
};
