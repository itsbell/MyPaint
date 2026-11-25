#pragma once

/**
 * @file MyPaintDlg.h
 * @brief 주 대화 상자 클래스 선언
 */

#include <array>
#include <memory>

#include "Core/Document.h"
#include "Tools/Tool.h"
#include "UI/CanvasWnd.h"
#include "UI/ToolPanel.h"

/**
 * @brief 주 대화 상자
 *
 * 메뉴, 도구 패널, 캔버스, 상태 표시줄을 배치하고 명령을 연결합니다. 그리기 로직은 갖지 않습니다.
 */
class CMyPaintDlg : public CDialog,
                    public IDocumentListener,
                    public ICanvasObserver,
                    public IToolHost,
                    public IToolPanelObserver
{
public:
    /**
     * @brief 생성자
     * @param pParent 부모 창 (없으면 nullptr)
     */
    explicit CMyPaintDlg(CWnd* pParent = nullptr);
    ~CMyPaintDlg() override;

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MYPAINT_DIALOG };
#endif

    // IDocumentListener
    void OnDocumentChanged(const CRect& dirty) override;
    void OnDocumentReset() override;

    // ICanvasObserver
    void OnCanvasCursor(bool inside, CPoint docPoint) override;
    void OnCanvasZoomChanged(double zoom) override;

    // IToolHost
    void SetToolColor(MouseButton button, COLORREF color) override;
    void RestorePreviousTool() override;
    void ZoomAt(CPoint docPoint, bool zoomIn) override;

    // IToolPanelObserver
    void OnPanelToolSelected(ToolId tool) override;
    void OnPanelSettingsChanged() override;
    void OnPanelEditColors() override;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    BOOL PreTranslateMessage(MSG* pMsg) override;

    /** @brief Enter 키로 창이 닫히지 않도록 막습니다. */
    void OnOK() override {}

    /** @brief Esc 키로 창이 닫히지 않도록 막습니다. 종료는 OnClose에서 처리합니다. */
    void OnCancel() override {}

    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnFileNew();
    afx_msg void OnAppExit();
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnViewZoomIn();
    afx_msg void OnViewZoomOut();
    afx_msg void OnViewZoom100();
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 자식 창 위치를 다시 계산합니다. */
    void LayoutControls();

    /** @brief 저장하지 않은 변경 내용이 있으면 계속할지 묻습니다. */
    bool ConfirmDiscardChanges();

    void UpdateTitle();
    void UpdateStatusBar();

    /** @brief 현재 도구를 바꿉니다. 진행 중인 작업은 취소됩니다. */
    void SelectTool(ToolId tool);

    /** @brief 사용자 지정 색 칸에 색을 넣습니다. (이미 있으면 넣지 않음, 꽉 차면 가장 오래된 색을 밀어냄) */
    void AddCustomColor(COLORREF color);

    /** @brief 96 DPI 기준 값을 현재 DPI로 바꿉니다. */
    int ScaleForDpi(int value) const;

    HICON m_hIcon = nullptr;  ///< 창 아이콘
    HACCEL m_hAccel = nullptr; ///< 단축키 표

    // 선언 순서 주의: 문서가 캔버스보다 먼저 만들어지고 나중에 없어져야 합니다.
    Document m_document;
    ToolSettings m_toolSettings;
    std::array<std::unique_ptr<Tool>, static_cast<size_t>(ToolId::Count)> m_tools;
    ToolId m_currentTool = ToolId::Pencil;
    ToolId m_previousTool = ToolId::Pencil; ///< 스포이트를 쓰고 돌아갈 도구
    CToolPanel m_toolPanel;
    CCanvasWnd m_canvas;
    CStatusBarCtrl m_statusBar;

    /** @brief 색 편집 대화 상자(CColorDialog)의 사용자 지정 색 16칸 */
    std::array<COLORREF, 16> m_colorDialogCustom{};

    CString m_title; ///< 마지막으로 설정한 창 제목 (불필요한 갱신 방지)
};
