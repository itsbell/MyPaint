#pragma once

/**
 * @file ToolPanel.h
 * @brief 그림판 리본을 단순화한 도구 패널 (직접 그리는 창)
 */

#include <vector>

#include "Tools/Tool.h"

/**
 * @brief 도구 패널에서 일어난 일을 받는 쪽 (주 대화 상자)
 */
class IToolPanelObserver
{
public:
    virtual ~IToolPanelObserver() = default;

    /** @brief 도구 버튼(또는 도형)을 눌렀습니다. */
    virtual void OnPanelToolSelected(ToolId tool) = 0;

    /** @brief 패널이 ToolSettings를 바꿨습니다. (색, 두께, 윤곽선/채우기 등) */
    virtual void OnPanelSettingsChanged() = 0;

    /** @brief [색 편집]을 눌렀습니다. */
    virtual void OnPanelEditColors() = 0;

    /** @brief [글꼴]을 눌렀습니다. (텍스트 도구) */
    virtual void OnPanelEditFont() = 0;

    /** @brief 메뉴와 같은 명령 버튼(붙여넣기, 자르기, 회전 등)을 눌렀습니다. */
    virtual void OnPanelCommand(UINT commandId) = 0;
};

/**
 * @brief 도구 패널
 *
 * 컨트롤을 여러 개 두는 대신 한 창이 모든 버튼을 직접 그리고 클릭 위치로 판별합니다.
 * 표시할 상태는 ToolSettings와 현재 도구에서 읽고, 바꿀 때는 설정을 고친 뒤 관찰자에게 알립니다.
 */
class CToolPanel : public CWnd
{
public:
    BOOL Create(CWnd* parent, UINT id);

    void SetSettings(ToolSettings* settings) { m_settings = settings; }
    void SetObserver(IToolPanelObserver* observer) { m_observer = observer; }

    /** @brief 선택 표시할 현재 도구 */
    void SetCurrentTool(ToolId tool);

    /** @brief 패널 높이 (현재 DPI 기준 픽셀) */
    int PreferredHeight() const;

    /** @brief 패널 전체 내용 폭 (현재 DPI 기준 픽셀) */
    int PreferredWidth() const;

    BOOL PreTranslateMessage(MSG* pMsg) override;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    enum class ItemKind
    {
        Tool,       ///< value = ToolId
        Shape,      ///< value = ShapeKind
        Outline,    ///< 도형 윤곽선 선택
        Fill,       ///< 도형 채우기 선택
        Size,       ///< 두께 선택
        ColorSlot,  ///< value = 0(색 1) / 1(색 2)
        Palette,    ///< value = 기본 팔레트 번호
        Custom,     ///< value = 사용자 지정 색 번호
        EditColors, ///< 색 편집 대화 상자
        Command,    ///< value = 명령 ID (붙여넣기, 잘라내기, 복사, 자르기, 크기 조정)
        RotateMenu, ///< 회전/대칭 선택 메뉴
        Font,       ///< 텍스트 글꼴 대화 상자
        TextBackground, ///< 텍스트 배경 투명/불투명
    };

    struct Item
    {
        ItemKind kind;
        int value;
        CRect rect;
        CString tip;   ///< 툴팁
        CString label; ///< 버튼에 쓰는 글자 (없으면 아이콘만)
    };

    struct Group
    {
        CString label;
        CRect labelRect;
        int separatorX;
    };

    void Layout();
    void EnsureFonts();
    int Scale(int value) const;
    UINT Dpi() const;
    int HitTest(CPoint point) const;
    bool IsSelected(const Item& item) const;
    void Activate(const Item& item, MouseButton button);

    /** @brief 항목 아래에 선택 메뉴를 띄우고 고른 번호를 돌려줍니다. (취소하면 -1) */
    int ShowChoiceMenu(const Item& item, const std::vector<CString>& choices, int checked);

    void DrawItem(CDC& dc, const Item& item, bool hover);
    void DrawGlyph(CDC& dc, const CRect& rect, const wchar_t* glyph, CFont& font);

    /** @brief 큰 버튼: 위에 아이콘, 아래에 글자 */
    void DrawBigButton(CDC& dc, const CRect& rect, const wchar_t* glyph, const CString& label);

    /** @brief 작은 버튼: 왼쪽에 아이콘, 오른쪽에 글자 */
    void DrawSmallButton(CDC& dc, const CRect& rect, const wchar_t* glyph, const CString& label);
    void DrawFillIcon(CDC& dc, const CRect& rect);
    void DrawSizeIcon(CDC& dc, const CRect& rect);
    void DrawSwatch(CDC& dc, const CRect& rect, COLORREF color);
    WidthKind CurrentWidthKind() const;

    ToolSettings* m_settings = nullptr;
    IToolPanelObserver* m_observer = nullptr;
    ToolId m_currentTool = ToolId::Pencil;

    std::vector<Item> m_items;
    std::vector<Group> m_groups;
    int m_contentWidth = 0;
    int m_hover = -1;
    bool m_trackingLeave = false;

    CToolTipCtrl m_tooltip;
    size_t m_tooltipCount = 0;

    UINT m_fontDpi = 0;
    CFont m_textFont;
    CFont m_glyphFont;
    CFont m_bigGlyphFont;
};
