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

class SelectTool;
class TextTool;

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
    void OnCanvasResizing(bool active, CSize newSize) override;
    void OnCanvasResizeRequested(CSize newSize) override;

    // IToolHost
    void SetToolColor(MouseButton button, COLORREF color) override;
    void RestorePreviousTool() override;
    void ZoomAt(CPoint docPoint, bool zoomIn) override;

    // IToolPanelObserver
    void OnPanelToolSelected(ToolId tool) override;
    void OnPanelSettingsChanged() override;
    void OnPanelEditColors() override;
    void OnPanelEditFont() override;
    void OnPanelCommand(UINT commandId) override;

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
    afx_msg void OnDropFiles(HDROP hDropInfo);

    // 파일
    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();
    afx_msg void OnAppExit();

    // 편집
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnEditCut();
    afx_msg void OnEditCopy();
    afx_msg void OnEditPaste();
    afx_msg void OnEditDelete();
    afx_msg void OnEditSelectAll();

    // 이미지
    afx_msg void OnImageCrop();
    afx_msg void OnImageResize();
    afx_msg void OnImageProperties();
    afx_msg void OnImageTransform(UINT commandId);
    afx_msg void OnImageInvert();

    // 보기
    afx_msg void OnViewZoomIn();
    afx_msg void OnViewZoomOut();
    afx_msg void OnViewZoom100();
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 자식 창 위치를 다시 계산합니다. */
    void LayoutControls();

    /**
     * @brief 저장하지 않은 변경 내용이 있으면 저장할지 묻습니다.
     * @return 계속해도 되면 true ([저장] 후 저장 성공, [저장 안 함]), [취소]나 저장 실패면 false
     */
    bool ConfirmSaveChanges();

    /** @brief 현재 경로에 저장합니다. 경로가 없으면 다른 이름으로 저장합니다. */
    bool SaveDocument();

    /** @brief 파일 이름을 물어서 저장합니다. */
    bool SaveDocumentAs();

    /** @brief 파일을 엽니다. (저장 확인은 호출하는 쪽에서) */
    bool OpenDocument(const CString& path);

    void UpdateTitle();
    void UpdateStatusBar();
    void UpdateSelectionStatus();

    /** @brief 현재 도구를 바꿉니다. 이전 도구의 작업은 확정됩니다. */
    void ChooseTool(ToolId tool);

    /** @brief 사용자 지정 색 칸에 색을 넣습니다. (이미 있으면 넣지 않음, 꽉 차면 가장 오래된 색을 밀어냄) */
    void AddCustomColor(COLORREF color);

    /** @brief 도구에 넘길 문맥 */
    ToolContext MakeToolContext();

    /** @brief 남아 있는 도구 작업(선택 영역, 텍스트)을 확정합니다. */
    void CommitTool();

    /** @brief 문서 명령을 실행하기 전에 도구 작업을 확정하고 적용합니다. */
    void ApplyCommand(std::unique_ptr<Command> command);

    ::SelectTool& Selection();
    ::TextTool& Text();

    /** @brief 96 DPI 기준 값을 현재 DPI로 바꿉니다. */
    int ScaleForDpi(int value) const;

    HICON m_hIcon = nullptr;   ///< 창 아이콘
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

    CString m_filePath; ///< 열거나 저장한 파일 경로 (새 그림이면 비어 있음)
    CString m_title;    ///< 마지막으로 설정한 창 제목 (불필요한 갱신 방지)

    /** @brief 색 편집 대화 상자(CColorDialog)의 사용자 지정 색 16칸 */
    std::array<COLORREF, 16> m_colorDialogCustom{};
};
