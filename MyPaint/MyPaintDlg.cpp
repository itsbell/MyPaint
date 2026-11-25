/**
 * @file MyPaintDlg.cpp
 * @brief 주 대화 상자 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"

#include <cmath>

#include <algorithm>

#include "Tools/FreehandTool.h"
#include "Tools/ShapeTool.h"
#include "Tools/SimpleTools.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr int kDefaultCanvasWidth = 800;
constexpr int kDefaultCanvasHeight = 600;

/** @brief 창 최소 크기 (96 DPI 기준). 폭은 도구 패널이 다 보이는 정도 */
constexpr int kMinWindowWidth = 960;
constexpr int kMinWindowHeight = 480;

/** @brief 상태 표시줄 칸 */
enum StatusPart
{
    kPartCursor,
    kPartSelection,
    kPartCanvasSize,
    kPartZoom,
    kPartCount,
};
} // namespace

BEGIN_MESSAGE_MAP(CMyPaintDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_INITMENUPOPUP()
    ON_COMMAND(ID_FILE_NEW, &CMyPaintDlg::OnFileNew)
    ON_COMMAND(ID_APP_EXIT, &CMyPaintDlg::OnAppExit)
    ON_COMMAND(ID_EDIT_UNDO, &CMyPaintDlg::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &CMyPaintDlg::OnEditRedo)
    ON_COMMAND(ID_VIEW_ZOOM_IN, &CMyPaintDlg::OnViewZoomIn)
    ON_COMMAND(ID_VIEW_ZOOM_OUT, &CMyPaintDlg::OnViewZoomOut)
    ON_COMMAND(ID_VIEW_ZOOM_100, &CMyPaintDlg::OnViewZoom100)
END_MESSAGE_MAP()

CMyPaintDlg::CMyPaintDlg(CWnd* pParent) : CDialog(IDD_MYPAINT_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_colorDialogCustom.fill(RGB(255, 255, 255));

    auto tool = [this](ToolId id) -> std::unique_ptr<Tool>& { return m_tools[static_cast<size_t>(id)]; };
    tool(ToolId::Pencil) = std::make_unique<FreehandTool>(StrokeStyle::Pencil);
    tool(ToolId::Brush) = std::make_unique<FreehandTool>(StrokeStyle::Brush);
    tool(ToolId::Eraser) = std::make_unique<FreehandTool>(StrokeStyle::Eraser);
    tool(ToolId::Fill) = std::make_unique<FillTool>();
    tool(ToolId::Picker) = std::make_unique<PickerTool>();
    tool(ToolId::Magnifier) = std::make_unique<MagnifierTool>();
    tool(ToolId::Shape) = std::make_unique<ShapeTool>();
}

CMyPaintDlg::~CMyPaintDlg()
{
    m_document.RemoveListener(this);
}

void CMyPaintDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BOOL CMyPaintDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    SetIcon(m_hIcon, TRUE);  // 큰 아이콘
    SetIcon(m_hIcon, FALSE); // 작은 아이콘
    m_hAccel = ::LoadAccelerators(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR));

    m_statusBar.Create(WS_CHILD | WS_VISIBLE | CCS_BOTTOM | SBARS_SIZEGRIP, CRect(0, 0, 0, 0), this, IDC_STATUSBAR);
    m_toolPanel.Create(this, IDC_TOOLPANEL);
    m_toolPanel.SetSettings(&m_toolSettings);
    m_toolPanel.SetObserver(this);
    m_canvas.Create(this, IDC_CANVAS);
    m_canvas.SetObserver(this);

    m_canvas.SetTool(m_tools[static_cast<size_t>(m_currentTool)].get(), &m_toolSettings, this);
    m_toolPanel.SetCurrentTool(m_currentTool);

    m_document.AddListener(this);
    m_document.New(kDefaultCanvasWidth, kDefaultCanvasHeight);
    m_canvas.SetDocument(&m_document);

    // 작업 영역의 80% 크기로 가운데에 띄웁니다.
    CRect work;
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    const int width = work.Width() * 4 / 5;
    const int height = work.Height() * 4 / 5;
    MoveWindow(work.left + (work.Width() - width) / 2, work.top + (work.Height() - height) / 2, width, height);
    LayoutControls();

    UpdateTitle();
    UpdateStatusBar();

    m_canvas.SetFocus();
    return FALSE; // 포커스를 직접 지정했습니다.
}

BOOL CMyPaintDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
        if (m_hAccel != nullptr && ::TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
        {
            return TRUE;
        }
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
        {
            m_canvas.CancelTool();
            return TRUE;
        }
    }
    return CDialog::PreTranslateMessage(pMsg);
}

// ---------------------------------------------------------------------------
// 창 관리
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnClose()
{
    m_canvas.CancelTool();
    if (ConfirmDiscardChanges())
    {
        EndDialog(IDCANCEL);
    }
}

void CMyPaintDlg::OnDestroy()
{
    m_document.RemoveListener(this);
    CDialog::OnDestroy();
}

void CMyPaintDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialog::OnSize(nType, cx, cy);
    if (nType != SIZE_MINIMIZED)
    {
        LayoutControls();
    }
}

void CMyPaintDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = ScaleForDpi(kMinWindowWidth);
    lpMMI->ptMinTrackSize.y = ScaleForDpi(kMinWindowHeight);
    CDialog::OnGetMinMaxInfo(lpMMI);
}

void CMyPaintDlg::LayoutControls()
{
    if (m_canvas.GetSafeHwnd() == nullptr || m_statusBar.GetSafeHwnd() == nullptr ||
        m_toolPanel.GetSafeHwnd() == nullptr)
    {
        return;
    }

    CRect client;
    GetClientRect(&client);

    // 상태 표시줄은 WM_SIZE를 받으면 스스로 아래에 붙습니다.
    m_statusBar.SendMessage(WM_SIZE);
    CRect statusRect;
    m_statusBar.GetWindowRect(&statusRect);

    // 좌표, 선택 크기, 캔버스 크기는 고정 폭이고 배율 칸이 남는 폭을 씁니다.
    const int partWidth = ScaleForDpi(160);
    int edges[kPartCount] = {partWidth, partWidth * 2, partWidth * 3, -1};
    m_statusBar.SetParts(kPartCount, edges);

    const int panelHeight = m_toolPanel.PreferredHeight();
    m_toolPanel.MoveWindow(0, 0, client.Width(), panelHeight);

    CRect canvasRect = client;
    canvasRect.top = panelHeight;
    canvasRect.bottom -= statusRect.Height();
    m_canvas.MoveWindow(canvasRect);
}

int CMyPaintDlg::ScaleForDpi(int value) const
{
    const UINT dpi = GetSafeHwnd() != nullptr ? ::GetDpiForWindow(m_hWnd) : USER_DEFAULT_SCREEN_DPI;
    return ::MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

void CMyPaintDlg::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);
    if (bSysMenu)
    {
        return;
    }
    // 대화 상자에는 ON_UPDATE_COMMAND_UI가 메뉴에 자동으로 적용되지 않으므로 직접 갱신합니다.
    pPopupMenu->EnableMenuItem(ID_EDIT_UNDO, MF_BYCOMMAND | (m_document.CanUndo() ? MF_ENABLED : MF_GRAYED));
    pPopupMenu->EnableMenuItem(ID_EDIT_REDO, MF_BYCOMMAND | (m_document.CanRedo() ? MF_ENABLED : MF_GRAYED));
}

bool CMyPaintDlg::ConfirmDiscardChanges()
{
    // 파일 저장 기능을 넣으면 [저장] [저장 안 함] [취소] 세 가지로 바꿉니다.
    if (!m_document.IsModified())
    {
        return true;
    }
    return AfxMessageBox(L"변경 내용이 저장되지 않았습니다.\n저장하지 않고 계속할까요?", MB_YESNO | MB_ICONWARNING) ==
           IDYES;
}

void CMyPaintDlg::UpdateTitle()
{
    CString title;
    title.Format(L"%s제목 없음 - MyPaint", m_document.IsModified() ? L"*" : L"");
    if (title != m_title)
    {
        m_title = title;
        SetWindowText(m_title);
    }
}

void CMyPaintDlg::UpdateStatusBar()
{
    if (m_statusBar.GetSafeHwnd() == nullptr)
    {
        return;
    }
    const CSize size = m_document.Size();
    CString text;
    text.Format(L"%d × %dpx", size.cx, size.cy);
    m_statusBar.SetText(text, kPartCanvasSize, 0);

    text.Format(L"%d%%", static_cast<int>(std::lround(m_canvas.Zoom() * 100.0)));
    m_statusBar.SetText(text, kPartZoom, 0);
}

// ---------------------------------------------------------------------------
// 알림
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnDocumentChanged(const CRect& /*dirty*/)
{
    UpdateTitle();
}

void CMyPaintDlg::OnDocumentReset()
{
    UpdateTitle();
    UpdateStatusBar();
}

void CMyPaintDlg::OnCanvasCursor(bool inside, CPoint docPoint)
{
    CString text;
    if (inside)
    {
        text.Format(L"%d, %dpx", docPoint.x, docPoint.y);
    }
    m_statusBar.SetText(text, kPartCursor, 0);
}

void CMyPaintDlg::OnCanvasZoomChanged(double /*zoom*/)
{
    UpdateStatusBar();
}

// ---------------------------------------------------------------------------
// 도구
// ---------------------------------------------------------------------------

void CMyPaintDlg::SelectTool(ToolId tool)
{
    if (tool != m_currentTool)
    {
        // 스포이트는 쓰고 나면 돌아가야 하므로, 스포이트로 바꿀 때만 이전 도구를 기억합니다.
        if (tool == ToolId::Picker)
        {
            m_previousTool = m_currentTool;
        }
        m_currentTool = tool;
        m_canvas.SetTool(m_tools[static_cast<size_t>(tool)].get(), &m_toolSettings, this);
    }
    m_toolPanel.SetCurrentTool(tool);
}

void CMyPaintDlg::SetToolColor(MouseButton button, COLORREF color)
{
    (button == MouseButton::Left ? m_toolSettings.color1 : m_toolSettings.color2) = color;
    m_toolPanel.Invalidate(FALSE);
}

void CMyPaintDlg::RestorePreviousTool()
{
    SelectTool(m_previousTool);
}

void CMyPaintDlg::ZoomAt(CPoint docPoint, bool zoomIn)
{
    m_canvas.ZoomAtDoc(docPoint, zoomIn);
}

void CMyPaintDlg::OnPanelToolSelected(ToolId tool)
{
    SelectTool(tool);
}

void CMyPaintDlg::OnPanelSettingsChanged()
{
    // 설정은 도구가 드래그를 시작할 때 읽으므로 따로 할 일이 없습니다.
}

void CMyPaintDlg::OnPanelEditColors()
{
    COLORREF& target = m_toolSettings.activeColorSlot == 0 ? m_toolSettings.color1 : m_toolSettings.color2;
    CColorDialog dialog(target, CC_FULLOPEN | CC_RGBINIT, this);
    dialog.m_cc.lpCustColors = m_colorDialogCustom.data();
    if (dialog.DoModal() != IDOK)
    {
        return;
    }
    target = dialog.GetColor();
    AddCustomColor(target);
    m_toolPanel.Invalidate(FALSE);
}

void CMyPaintDlg::AddCustomColor(COLORREF color)
{
    auto& colors = m_toolSettings.customColors;
    if (std::find(colors.begin(), colors.end(), color) != colors.end())
    {
        return;
    }
    const auto empty = std::find(colors.begin(), colors.end(), CLR_INVALID);
    if (empty != colors.end())
    {
        *empty = color;
        return;
    }
    std::rotate(colors.begin(), colors.begin() + 1, colors.end());
    colors.back() = color;
}

// ---------------------------------------------------------------------------
// 명령
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnFileNew()
{
    m_canvas.CancelTool();
    if (ConfirmDiscardChanges())
    {
        m_document.New(kDefaultCanvasWidth, kDefaultCanvasHeight);
        m_canvas.SetZoom(1.0);
    }
}

void CMyPaintDlg::OnAppExit()
{
    PostMessage(WM_CLOSE);
}

void CMyPaintDlg::OnEditUndo()
{
    m_canvas.CancelTool();
    m_document.Undo();
}

void CMyPaintDlg::OnEditRedo()
{
    m_canvas.CancelTool();
    m_document.Redo();
}

void CMyPaintDlg::OnViewZoomIn()
{
    m_canvas.ZoomIn();
}

void CMyPaintDlg::OnViewZoomOut()
{
    m_canvas.ZoomOut();
}

void CMyPaintDlg::OnViewZoom100()
{
    m_canvas.SetZoom(1.0);
}
