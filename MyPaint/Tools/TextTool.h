#pragma once

/**
 * @file TextTool.h
 * @brief 텍스트 도구
 */

#include <string>

#include "Tools/Tool.h"

/**
 * @brief 텍스트 상자의 모양 (도구 설정에서 복사해 둔 값)
 */
struct TextStyle
{
    std::wstring face;
    int pointSize = 11;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool opaque = false;
    COLORREF color = RGB(0, 0, 0);
    COLORREF background = RGB(255, 255, 255);

    static TextStyle From(const ToolSettings& settings);
};

/**
 * @brief 텍스트 도구
 *
 * 끌어서 상자를 만들고 글자를 입력합니다. 입력 중인 글자는 미리보기로만 그려지고,
 * 상자 밖을 클릭하거나 도구를 바꾸면 결과 픽셀이 PatchCommand로 확정됩니다.
 * (글꼴은 PC마다 다를 수 있으므로 글자 대신 픽셀을 기록합니다.)
 *
 * 한글은 IME 조합 중인 글자도 상자 안에 바로 보여 줍니다.
 */
class TextTool : public Tool
{
public:
    void OnDown(ToolContext& context, CPoint point, MouseButton button) override;
    void OnMove(ToolContext& context, CPoint point) override;
    void OnUp(ToolContext& context, CPoint point) override;
    void Cancel() override;
    void Commit(ToolContext& context) override;
    bool IsActive() const override { return m_dragging; }
    void DrawPreview(Surface& surface) const override;
    std::unique_ptr<Command> MakeSharedPreview(const Surface& document) const override;
    CRect TakePreviewDirty() override;
    CRect OverlayRect() const override { return (m_editing || m_dragging) ? m_box : CRect(); }
    HCURSOR CursorAt(CPoint point) const override;
    bool WantsTextInput() const override { return m_editing; }
    void OnChar(ToolContext& context, wchar_t ch) override;
    void OnComposition(ToolContext& context, const std::wstring& composing) override;
    void OnSettingsChanged(ToolContext& context) override;

    /** @brief 입력 중인지 */
    bool IsEditing() const { return m_editing; }

    /** @brief 입력 중인 상자에 글자를 붙입니다. (클립보드 붙여넣기) */
    void InsertText(ToolContext& context, const std::wstring& text);

private:
    /** @brief 상자 영역의 문서 픽셀 위에 text를 그린 결과 픽셀 명령 (그릴 곳이 없으면 nullptr) */
    std::unique_ptr<Command> Rasterize(const Surface& document, const std::wstring& text) const;

    /** @brief 글자가 상자 아래로 넘치면 상자를 늘립니다. */
    void FitHeight();

    /** @brief 상자 영역을 다시 그리도록 표시합니다. */
    void MarkDirty();

    void Reset();

    bool m_dragging = false;
    bool m_editing = false;
    CPoint m_anchor;
    CRect m_box;
    std::wstring m_text;
    std::wstring m_composing;
    TextStyle m_style;
    CRect m_dirty;
};
