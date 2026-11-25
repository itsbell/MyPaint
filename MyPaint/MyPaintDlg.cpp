/**
 * @file MyPaintDlg.cpp
 * @brief 주 대화 상자 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"

#include <algorithm>
#include <cmath>

#include <shlwapi.h>

#include "Core/ImageCommands.h"
#include "Tools/FreehandTool.h"
#include "Tools/SelectTool.h"
#include "Tools/ShapeTool.h"
#include "Tools/SimpleTools.h"
#include "Tools/TextTool.h"
#include "UI/Clipboard.h"
#include "UI/ImageDialogs.h"
#include "UI/ImageFile.h"
#include "UI/ShareDialogs.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr int kDefaultCanvasWidth = 800;
constexpr int kDefaultCanvasHeight = 600;

/** @brief 창 최소 높이 (96 DPI 기준). 최소 폭은 도구 패널 폭에 맞춥니다. */
constexpr int kMinWindowHeight = 480;

/** @brief 상태 표시줄 칸 */
enum StatusPart
{
    kPartCursor,
    kPartSelection,
    kPartCanvasSize,
    kPartZoom,
    kPartShare,
    kPartCount,
};

/** @brief 네트워크 스레드가 받은 일이 있다고 알리는 메시지 */
constexpr UINT WM_APP_NET = WM_APP + 1;

/** @brief 공유가 끝났다는 안내를 (네트워크 처리가 끝난 뒤) 보여 주는 메시지 */
constexpr UINT WM_APP_SHARE_NOTICE = WM_APP + 2;

/** @brief 방장이 미리보기와 커서를 보내는 주기 (약 30fps) */
constexpr UINT_PTR kShareTimer = 1;
constexpr UINT kShareIntervalMs = 33;

CString SizeText(CSize size)
{
    CString text;
    text.Format(L"%d × %dpx", size.cx, size.cy);
    return text;
}
} // namespace

BEGIN_MESSAGE_MAP(CMyPaintDlg, CDialog)
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
    ON_WM_INITMENUPOPUP()
    ON_WM_DROPFILES()
    ON_COMMAND(ID_FILE_NEW, &CMyPaintDlg::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, &CMyPaintDlg::OnFileOpen)
    ON_COMMAND(ID_FILE_SAVE, &CMyPaintDlg::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, &CMyPaintDlg::OnFileSaveAs)
    ON_COMMAND(ID_APP_EXIT, &CMyPaintDlg::OnAppExit)
    ON_COMMAND(ID_EDIT_UNDO, &CMyPaintDlg::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &CMyPaintDlg::OnEditRedo)
    ON_COMMAND(ID_EDIT_CUT, &CMyPaintDlg::OnEditCut)
    ON_COMMAND(ID_EDIT_COPY, &CMyPaintDlg::OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE, &CMyPaintDlg::OnEditPaste)
    ON_COMMAND(ID_EDIT_CLEAR, &CMyPaintDlg::OnEditDelete)
    ON_COMMAND(ID_EDIT_SELECT_ALL, &CMyPaintDlg::OnEditSelectAll)
    ON_COMMAND(ID_IMAGE_CROP, &CMyPaintDlg::OnImageCrop)
    ON_COMMAND(ID_IMAGE_RESIZE, &CMyPaintDlg::OnImageResize)
    ON_COMMAND(ID_IMAGE_PROPERTIES, &CMyPaintDlg::OnImageProperties)
    ON_COMMAND_RANGE(ID_IMAGE_ROTATE_RIGHT, ID_IMAGE_FLIP_V, &CMyPaintDlg::OnImageTransform)
    ON_COMMAND(ID_IMAGE_INVERT, &CMyPaintDlg::OnImageInvert)
    ON_COMMAND(ID_VIEW_ZOOM_IN, &CMyPaintDlg::OnViewZoomIn)
    ON_COMMAND(ID_VIEW_ZOOM_OUT, &CMyPaintDlg::OnViewZoomOut)
    ON_COMMAND(ID_VIEW_ZOOM_100, &CMyPaintDlg::OnViewZoom100)
    ON_COMMAND(ID_SHARE_START, &CMyPaintDlg::OnShareStart)
    ON_COMMAND(ID_SHARE_JOIN, &CMyPaintDlg::OnShareJoin)
    ON_COMMAND(ID_SHARE_STOP, &CMyPaintDlg::OnShareStop)
    ON_COMMAND(ID_SHARE_CHAT, &CMyPaintDlg::OnShareChatToggle)
    ON_WM_TIMER()
    ON_MESSAGE(WM_APP_NET, &CMyPaintDlg::OnNetMessage)
    ON_MESSAGE(WM_APP_SHARE_NOTICE, &CMyPaintDlg::OnShareNotice)
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
    tool(ToolId::Select) = std::make_unique<::SelectTool>();
    tool(ToolId::Text) = std::make_unique<::TextTool>();
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
    DragAcceptFiles(TRUE);

    m_statusBar.Create(WS_CHILD | WS_VISIBLE | CCS_BOTTOM | SBARS_SIZEGRIP, CRect(0, 0, 0, 0), this, IDC_STATUSBAR);
    m_toolPanel.Create(this, IDC_TOOLPANEL);
    m_toolPanel.SetSettings(&m_toolSettings);
    m_toolPanel.SetObserver(this);
    m_canvas.Create(this, IDC_CANVAS);
    m_canvas.SetObserver(this);
    m_sharePanel.Create(this, IDC_SHAREPANEL);
    m_sharePanel.SetInputEnabled(false);
    m_sharePanel.SetSendHandler([this](const std::wstring& text) {
        if (m_shareMode == ShareMode::Hosting && m_shareHost)
        {
            m_shareHost->SendChat(text);
        }
        else if (m_shareMode == ShareMode::Viewing && m_shareViewer)
        {
            m_shareViewer->SendChat(text);
        }
    });

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

    // 명령줄로 받은 파일을 엽니다. (탐색기에서 "연결 프로그램"으로 연 경우 등)
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv != nullptr)
    {
        if (argc > 1)
        {
            OpenDocument(argv[1]);
        }
        ::LocalFree(argv);
    }

    UpdateTitle();
    UpdateStatusBar();

    m_canvas.SetFocus();
    return FALSE; // 포커스를 직접 지정했습니다.
}

BOOL CMyPaintDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message >= WM_KEYFIRST && pMsg->message <= WM_KEYLAST)
    {
        // 채팅 입력 중에는 Enter로 보내고, Ctrl+C/V 같은 키는 입력 칸이 쓰도록 단축키를 건너뜁니다.
        if (m_sharePanelVisible && m_sharePanel.HandleKey(pMsg))
        {
            return TRUE;
        }
        if (m_sharePanelVisible && m_sharePanel.HasFocusInside())
        {
            return CDialog::PreTranslateMessage(pMsg);
        }
        if (m_hAccel != nullptr && ::TranslateAccelerator(m_hWnd, m_hAccel, pMsg))
        {
            return TRUE;
        }
        if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE)
        {
            m_canvas.HandleEscape();
            UpdateSelectionStatus();
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
    if (m_shareMode == ShareMode::Hosting)
    {
        if (AfxMessageBox(L"그림을 공유하는 중입니다. 공유를 끝내고 닫으시겠습니까?", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }
        StopHosting();
    }
    CommitTool();
    if (ConfirmSaveChanges())
    {
        EndDialog(IDCANCEL);
    }
}

void CMyPaintDlg::OnDestroy()
{
    KillTimer(kShareTimer);
    // 공유 개체는 문서를 참조하므로 문서보다 먼저 정리합니다.
    if (m_shareHost)
    {
        m_shareHost->Stop();
        m_shareHost.reset();
    }
    if (m_shareViewer)
    {
        m_shareViewer->Stop();
        m_shareViewer.reset();
    }
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
    // 도구 패널이 잘리지 않을 만큼의 폭 (창 테두리 여유 포함)
    const int panelWidth = m_toolPanel.GetSafeHwnd() != nullptr ? m_toolPanel.PreferredWidth() : 0;
    lpMMI->ptMinTrackSize.x = (std::max)(ScaleForDpi(640), panelWidth + ScaleForDpi(24));
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

    // 좌표, 선택 크기, 캔버스 크기, 배율은 고정 폭이고 공유 상태 칸이 남는 폭을 씁니다.
    const int partWidth = ScaleForDpi(160);
    int edges[kPartCount] = {partWidth, partWidth * 2, partWidth * 3, partWidth * 3 + ScaleForDpi(80), -1};
    m_statusBar.SetParts(kPartCount, edges);

    const int panelHeight = m_toolPanel.PreferredHeight();
    m_toolPanel.MoveWindow(0, 0, client.Width(), panelHeight);

    CRect canvasRect = client;
    canvasRect.top = panelHeight;
    canvasRect.bottom -= statusRect.Height();
    if (m_sharePanelVisible && m_sharePanel.GetSafeHwnd() != nullptr)
    {
        // 오른쪽에 참가자/채팅 창
        const int width = (std::min)(m_sharePanel.PreferredWidth(), canvasRect.Width() / 2);
        m_sharePanel.MoveWindow(canvasRect.right - width, canvasRect.top, width, canvasRect.Height());
        canvasRect.right -= width;
    }
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
    auto enable = [pPopupMenu](UINT id, bool on) {
        pPopupMenu->EnableMenuItem(id, MF_BYCOMMAND | (on ? MF_ENABLED : MF_GRAYED));
    };
    const bool hasSelection = m_currentTool == ToolId::Select && Selection().HasSelection();
    const bool textEditing = m_currentTool == ToolId::Text && Text().IsEditing();
    enable(ID_EDIT_UNDO, m_document.CanUndo() || textEditing);
    enable(ID_EDIT_REDO, m_document.CanRedo());
    enable(ID_EDIT_CUT, hasSelection);
    enable(ID_EDIT_COPY, hasSelection);
    enable(ID_EDIT_CLEAR, hasSelection);
    enable(ID_IMAGE_CROP, hasSelection);
    enable(ID_EDIT_PASTE,
           Clipboard::HasImage() || (textEditing && ::IsClipboardFormatAvailable(CF_UNICODETEXT)));

    // 공유 메뉴
    enable(ID_SHARE_START, m_shareMode != ShareMode::Hosting);
    enable(ID_SHARE_JOIN, m_shareMode != ShareMode::Hosting);
    enable(ID_SHARE_STOP, m_shareMode == ShareMode::Hosting || m_shareMode == ShareMode::Viewing);
    pPopupMenu->CheckMenuItem(ID_SHARE_CHAT, MF_BYCOMMAND | (m_sharePanelVisible ? MF_CHECKED : MF_UNCHECKED));

    // 보는 중에는 그림을 바꾸는 메뉴를 모두 막습니다.
    if (IsViewOnly())
    {
        for (int i = 0; i < pPopupMenu->GetMenuItemCount(); ++i)
        {
            const UINT id = pPopupMenu->GetMenuItemID(i);
            if (id != 0 && id != static_cast<UINT>(-1) && !IsCommandAllowedWhileViewing(id))
            {
                enable(id, false);
            }
        }
    }
}

void CMyPaintDlg::OnDropFiles(HDROP hDropInfo)
{
    const UINT length = ::DragQueryFileW(hDropInfo, 0, nullptr, 0);
    CString path;
    if (length > 0)
    {
        ::DragQueryFileW(hDropInfo, 0, path.GetBuffer(static_cast<int>(length) + 1), length + 1);
        path.ReleaseBuffer();
    }
    ::DragFinish(hDropInfo);

    if (!path.IsEmpty() && LeaveViewForDocumentChange())
    {
        CommitTool();
        if (ConfirmSaveChanges())
        {
            OpenDocument(path);
        }
    }
}

// ---------------------------------------------------------------------------
// 상태 표시
// ---------------------------------------------------------------------------

void CMyPaintDlg::UpdateTitle()
{
    const CString name = m_filePath.IsEmpty() ? CString(L"제목 없음") : CString(::PathFindFileNameW(m_filePath));
    CString title;
    switch (m_shareMode)
    {
    case ShareMode::Viewing:
        title = m_viewRoom.empty() ? CString(L"연결 중... - MyPaint")
                                   : CString((m_viewRoom + L" (보는 중) - MyPaint").c_str());
        break;
    case ShareMode::ViewEnded:
        title = (m_viewRoom.empty() ? L"공유" : m_viewRoom.c_str());
        title += L" (공유 끝남) - MyPaint";
        break;
    default:
        title.Format(L"%s%s - MyPaint", m_document.IsModified() ? L"*" : L"", name.GetString());
        break;
    }
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
    m_statusBar.SetText(SizeText(m_document.Size()), kPartCanvasSize, 0);

    CString text;
    text.Format(L"%d%%", static_cast<int>(std::lround(m_canvas.Zoom() * 100.0)));
    m_statusBar.SetText(text, kPartZoom, 0);
    m_statusBar.SetText(ShareStatusText(), kPartShare, 0);
    UpdateSelectionStatus();
}

void CMyPaintDlg::UpdateSelectionStatus()
{
    if (m_statusBar.GetSafeHwnd() == nullptr)
    {
        return;
    }
    const CRect overlay = m_tools[static_cast<size_t>(m_currentTool)]->OverlayRect();
    m_statusBar.SetText(overlay.IsRectEmpty() ? CString() : SizeText(overlay.Size()), kPartSelection, 0);
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
    UpdateSelectionStatus();
    if (m_shareMode == ShareMode::Hosting && m_shareHost)
    {
        m_shareHost->SetCursor(inside, docPoint);
    }
}

void CMyPaintDlg::OnCanvasPreviewChanged()
{
    if (m_shareMode == ShareMode::Hosting && m_shareHost)
    {
        m_shareHost->MarkPreviewDirty();
    }
}

void CMyPaintDlg::OnCanvasZoomChanged(double /*zoom*/)
{
    UpdateStatusBar();
}

void CMyPaintDlg::OnCanvasResizing(bool active, CSize newSize)
{
    if (active)
    {
        m_statusBar.SetText(SizeText(newSize), kPartCanvasSize, 0); // 끄는 동안 새 크기를 보여 줌
    }
    else
    {
        UpdateStatusBar();
    }
}

void CMyPaintDlg::OnCanvasResizeRequested(CSize newSize)
{
    // 새로 생기는 부분은 이미지 속성 대화 상자와 같이 색 2로 채웁니다.
    ApplyCommand(std::make_unique<CanvasSizeCommand>(newSize, CPoint(0, 0), m_toolSettings.color2));
}

// ---------------------------------------------------------------------------
// 도구
// ---------------------------------------------------------------------------

::SelectTool& CMyPaintDlg::Selection()
{
    return *static_cast<::SelectTool*>(m_tools[static_cast<size_t>(ToolId::Select)].get());
}

::TextTool& CMyPaintDlg::Text()
{
    return *static_cast<::TextTool*>(m_tools[static_cast<size_t>(ToolId::Text)].get());
}

ToolContext CMyPaintDlg::MakeToolContext()
{
    return ToolContext{m_document, m_toolSettings, *this, false};
}

void CMyPaintDlg::CommitTool()
{
    m_canvas.CommitTool();
    UpdateSelectionStatus();
}

void CMyPaintDlg::ApplyCommand(std::unique_ptr<Command> command)
{
    CommitTool();
    m_document.Apply(std::move(command));
}

void CMyPaintDlg::ChooseTool(ToolId tool)
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
    else if (tool == ToolId::Shape)
    {
        m_canvas.CommitTool(); // 다른 도형을 고르면 그리다 만 곡선/다각형을 확정합니다.
    }
    m_toolPanel.SetCurrentTool(tool);
    UpdateSelectionStatus();
}

void CMyPaintDlg::SetToolColor(MouseButton button, COLORREF color)
{
    (button == MouseButton::Left ? m_toolSettings.color1 : m_toolSettings.color2) = color;
    m_toolPanel.Invalidate(FALSE);
    m_canvas.NotifySettingsChanged();
}

void CMyPaintDlg::RestorePreviousTool()
{
    ChooseTool(m_previousTool);
}

void CMyPaintDlg::ZoomAt(CPoint docPoint, bool zoomIn)
{
    m_canvas.ZoomAtDoc(docPoint, zoomIn);
}

void CMyPaintDlg::OnPanelToolSelected(ToolId tool)
{
    ChooseTool(tool);
}

void CMyPaintDlg::OnPanelSettingsChanged()
{
    // 대부분의 설정은 도구가 드래그를 시작할 때 읽지만, 입력 중인 텍스트는 바로 바뀌어야 합니다.
    m_canvas.NotifySettingsChanged();
}

void CMyPaintDlg::OnPanelCommand(UINT commandId)
{
    SendMessage(WM_COMMAND, commandId);
    m_canvas.SetFocus(); // 단축키와 텍스트 입력이 계속 캔버스로 가도록
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
    m_canvas.NotifySettingsChanged();
}

void CMyPaintDlg::OnPanelEditFont()
{
    // 글꼴 대화 상자는 화면 DPI로 크기를 다루므로 포인트 ↔ 논리 단위를 화면 기준으로 바꿉니다.
    CClientDC screen(this);
    const int dpiY = screen.GetDeviceCaps(LOGPIXELSY);

    LOGFONT lf{};
    lf.lfHeight = -::MulDiv(m_toolSettings.textPointSize, dpiY, 72);
    lf.lfWeight = m_toolSettings.textBold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = m_toolSettings.textItalic ? TRUE : FALSE;
    lf.lfUnderline = m_toolSettings.textUnderline ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(lf.lfFaceName, m_toolSettings.textFace.c_str(), _TRUNCATE);

    CFontDialog dialog(&lf, CF_SCREENFONTS | CF_EFFECTS, nullptr, this);
    dialog.m_cf.rgbColors = m_toolSettings.color1; // 글자 색 = 색 1
    if (dialog.DoModal() != IDOK)
    {
        return;
    }
    m_toolSettings.textFace = dialog.GetFaceName().GetString();
    m_toolSettings.textPointSize = (std::max)(1, dialog.GetSize() / 10);
    m_toolSettings.textBold = dialog.IsBold() != FALSE;
    m_toolSettings.textItalic = dialog.IsItalic() != FALSE;
    m_toolSettings.textUnderline = dialog.IsUnderline() != FALSE;
    m_toolSettings.color1 = dialog.GetColor();
    m_toolPanel.Invalidate(FALSE);
    m_canvas.NotifySettingsChanged();
    m_canvas.SetFocus();
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
// 파일
// ---------------------------------------------------------------------------

bool CMyPaintDlg::ConfirmSaveChanges()
{
    if (!m_document.IsModified())
    {
        return true;
    }
    const CString name = m_filePath.IsEmpty() ? CString(L"제목 없음") : CString(::PathFindFileNameW(m_filePath));
    CString message;
    message.Format(L"%s의 변경 내용을 저장하시겠습니까?", name.GetString());
    switch (AfxMessageBox(message, MB_YESNOCANCEL | MB_ICONQUESTION))
    {
    case IDYES:
        return SaveDocument();
    case IDNO:
        return true;
    default:
        return false;
    }
}

bool CMyPaintDlg::OpenDocument(const CString& path)
{
    Surface surface;
    CString error;
    if (!ImageFile::Load(path, surface, error))
    {
        AfxMessageBox(error, MB_ICONERROR);
        return false;
    }
    m_canvas.CommitTool();
    Selection().Discard();
    m_filePath = path;
    m_document.Reset(std::move(surface));
    m_canvas.SetZoom(1.0);
    UpdateTitle();
    return true;
}

bool CMyPaintDlg::SaveDocument()
{
    CommitTool();
    if (m_filePath.IsEmpty())
    {
        return SaveDocumentAs();
    }
    CString error;
    if (!ImageFile::Save(m_filePath, m_document.GetSurface(), error))
    {
        AfxMessageBox(error, MB_ICONERROR);
        return false;
    }
    m_document.MarkSaved();
    UpdateTitle();
    return true;
}

bool CMyPaintDlg::SaveDocumentAs()
{
    CommitTool();
    const CString defaultName =
        m_filePath.IsEmpty() ? CString(L"제목 없음.png") : CString(::PathFindFileNameW(m_filePath));
    CFileDialog dialog(FALSE, L"png", defaultName, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
                       ImageFile::kSaveFilter, this);

    // 지금 파일의 형식을 기본 필터로 고릅니다.
    CString ext = ::PathFindExtensionW(defaultName);
    ext.MakeLower();
    dialog.m_ofn.nFilterIndex = (ext == L".jpg" || ext == L".jpeg") ? 2 : (ext == L".bmp") ? 3 : (ext == L".gif") ? 4 : 1;

    if (dialog.DoModal() != IDOK)
    {
        return false;
    }
    CString path = dialog.GetPathName();
    if (!ImageFile::IsSupportedExtension(path))
    {
        path += ImageFile::ExtensionForSaveFilter(static_cast<int>(dialog.m_ofn.nFilterIndex));
    }

    CString error;
    if (!ImageFile::Save(path, m_document.GetSurface(), error))
    {
        AfxMessageBox(error, MB_ICONERROR);
        return false;
    }
    m_filePath = path;
    m_document.MarkSaved();
    UpdateTitle();
    return true;
}

void CMyPaintDlg::OnFileNew()
{
    if (!LeaveViewForDocumentChange())
    {
        return;
    }
    CommitTool();
    if (ConfirmSaveChanges())
    {
        Selection().Discard();
        m_filePath.Empty();
        m_document.New(kDefaultCanvasWidth, kDefaultCanvasHeight);
        m_canvas.SetZoom(1.0);
        UpdateTitle();
    }
}

void CMyPaintDlg::OnFileOpen()
{
    if (!LeaveViewForDocumentChange())
    {
        return;
    }
    CommitTool();
    if (!ConfirmSaveChanges())
    {
        return;
    }
    CFileDialog dialog(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, ImageFile::kOpenFilter, this);
    if (dialog.DoModal() == IDOK)
    {
        OpenDocument(dialog.GetPathName());
    }
}

void CMyPaintDlg::OnFileSave()
{
    SaveDocument();
}

void CMyPaintDlg::OnFileSaveAs()
{
    SaveDocumentAs();
}

void CMyPaintDlg::OnAppExit()
{
    PostMessage(WM_CLOSE);
}

// ---------------------------------------------------------------------------
// 편집
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnEditUndo()
{
    // 떠 있는 선택 영역이나 입력 중인 텍스트는 먼저 확정되므로, 실행 취소하면 그 작업이 먼저 되돌아갑니다.
    CommitTool();
    m_document.Undo();
}

void CMyPaintDlg::OnEditRedo()
{
    CommitTool();
    m_document.Redo();
}

void CMyPaintDlg::OnEditCopy()
{
    if (m_currentTool != ToolId::Select)
    {
        return;
    }
    CSize size;
    std::vector<Pixel> pixels;
    if (Selection().GetSelectionPixels(m_document, size, pixels))
    {
        Clipboard::PutImage(m_hWnd, size, pixels);
    }
}

void CMyPaintDlg::OnEditCut()
{
    if (m_currentTool != ToolId::Select || !Selection().HasSelection())
    {
        return;
    }
    OnEditCopy();
    ToolContext context = MakeToolContext();
    Selection().DeleteSelection(context);
    m_canvas.InvalidateToolPreview();
    UpdateSelectionStatus();
}

void CMyPaintDlg::OnEditPaste()
{
    // 텍스트를 입력하는 중이면 글자를 붙여넣습니다.
    if (m_currentTool == ToolId::Text && Text().IsEditing())
    {
        std::wstring text;
        if (Clipboard::GetText(m_hWnd, text))
        {
            ToolContext context = MakeToolContext();
            Text().InsertText(context, text);
            m_canvas.InvalidateToolPreview();
        }
        return;
    }

    CSize size;
    std::vector<Pixel> pixels;
    if (!Clipboard::GetImage(m_hWnd, size, pixels))
    {
        return;
    }
    CommitTool();

    // 붙여넣을 그림이 캔버스보다 크면 캔버스를 늘립니다. (그림판과 같은 동작)
    const CSize canvas = m_document.Size();
    if (size.cx > canvas.cx || size.cy > canvas.cy)
    {
        const CSize enlarged((std::max)(size.cx, canvas.cx), (std::max)(size.cy, canvas.cy));
        m_document.Apply(std::make_unique<CanvasSizeCommand>(enlarged, CPoint(0, 0), m_toolSettings.color2));
    }

    ChooseTool(ToolId::Select);
    // 화면에 보이는 왼쪽 위에 붙이되, 캔버스 안에 들어가도록 당깁니다.
    CPoint at = m_canvas.VisibleDocOrigin();
    const CSize current = m_document.Size();
    at.x = (std::min)(at.x, (std::max)(0L, current.cx - size.cx));
    at.y = (std::min)(at.y, (std::max)(0L, current.cy - size.cy));

    ToolContext context = MakeToolContext();
    Selection().PasteFloating(context, size, std::move(pixels), at);
    m_canvas.InvalidateToolPreview();
    UpdateSelectionStatus();
}

void CMyPaintDlg::OnEditDelete()
{
    if (m_currentTool != ToolId::Select || !Selection().HasSelection())
    {
        return;
    }
    ToolContext context = MakeToolContext();
    Selection().DeleteSelection(context);
    m_canvas.InvalidateToolPreview();
    UpdateSelectionStatus();
}

void CMyPaintDlg::OnEditSelectAll()
{
    ChooseTool(ToolId::Select);
    ToolContext context = MakeToolContext();
    Selection().SelectAll(context);
    m_canvas.InvalidateToolPreview();
    UpdateSelectionStatus();
}

// ---------------------------------------------------------------------------
// 이미지
// ---------------------------------------------------------------------------

void CMyPaintDlg::OnImageCrop()
{
    if (m_currentTool != ToolId::Select || !Selection().HasSelection())
    {
        return;
    }
    const CRect area = Selection().SelectionRect();
    // 떠 있는 내용을 먼저 확정한 뒤, 그 자리로 캔버스를 자릅니다.
    ApplyCommand(std::make_unique<CanvasSizeCommand>(area.Size(), CPoint(-area.left, -area.top),
                                                     m_toolSettings.color2));
}

void CMyPaintDlg::OnImageResize()
{
    CommitTool();
    CResizeDlg dialog(m_document.Size(), this);
    if (dialog.DoModal() != IDOK)
    {
        return;
    }
    if (dialog.NewSize() == m_document.Size() && dialog.SkewHorizontal() == 0 && dialog.SkewVertical() == 0)
    {
        return;
    }
    ApplyCommand(std::make_unique<ResizeSkewCommand>(dialog.NewSize(), dialog.SkewHorizontal(),
                                                     dialog.SkewVertical(), m_toolSettings.color2));
}

void CMyPaintDlg::OnImageProperties()
{
    CommitTool();
    CPropertiesDlg dialog(m_document.Size(), this);
    if (dialog.DoModal() == IDOK && dialog.NewSize() != m_document.Size())
    {
        ApplyCommand(std::make_unique<CanvasSizeCommand>(dialog.NewSize(), CPoint(0, 0), m_toolSettings.color2));
    }
}

void CMyPaintDlg::OnImageTransform(UINT commandId)
{
    TransformKind kind = TransformKind::RotateRight;
    switch (commandId)
    {
    case ID_IMAGE_ROTATE_LEFT: kind = TransformKind::RotateLeft; break;
    case ID_IMAGE_ROTATE_180: kind = TransformKind::Rotate180; break;
    case ID_IMAGE_FLIP_H: kind = TransformKind::FlipHorizontal; break;
    case ID_IMAGE_FLIP_V: kind = TransformKind::FlipVertical; break;
    default: break;
    }
    ApplyCommand(std::make_unique<TransformCommand>(kind));
}

void CMyPaintDlg::OnImageInvert()
{
    // 선택 영역이 있으면 그 영역만, 없으면 이미지 전체를 반전합니다.
    if (m_currentTool == ToolId::Select && Selection().HasSelection())
    {
        const CRect area = Selection().SelectionRect();
        ApplyCommand(std::make_unique<InvertCommand>(area));
        return;
    }
    ApplyCommand(std::make_unique<InvertCommand>(m_document.GetSurface().Bounds()));
}

// ---------------------------------------------------------------------------
// 보기
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 실시간 공유
// ---------------------------------------------------------------------------

BOOL CMyPaintDlg::OnCommand(WPARAM wParam, LPARAM lParam)
{
    // 메뉴와 단축키(lParam == 0)로 온 명령만 거릅니다. 컨트롤 알림은 그대로 둡니다.
    if (lParam == 0 && IsViewOnly() && !IsCommandAllowedWhileViewing(LOWORD(wParam)))
    {
        return TRUE;
    }
    return CDialog::OnCommand(wParam, lParam);
}

bool CMyPaintDlg::IsCommandAllowedWhileViewing(UINT id) const
{
    switch (id)
    {
    case IDOK:
    case IDCANCEL:
    case ID_APP_EXIT:
    case ID_FILE_NEW:  // 공유에서 나갈지 물어본 뒤 실행
    case ID_FILE_OPEN: // 공유에서 나갈지 물어본 뒤 실행
    case ID_VIEW_ZOOM_IN:
    case ID_VIEW_ZOOM_OUT:
    case ID_VIEW_ZOOM_100:
    case ID_SHARE_START:
    case ID_SHARE_JOIN:
    case ID_SHARE_STOP:
    case ID_SHARE_CHAT:
        return true;
    case ID_FILE_SAVE:
    case ID_FILE_SAVE_AS:
        return m_shareMode == ShareMode::ViewEnded; // 공유가 끝난 뒤에는 마지막 그림을 저장할 수 있습니다.
    default:
        return false;
    }
}

void CMyPaintDlg::EnterViewMode()
{
    CommitTool();
    Selection().Discard();
    m_remoteTool.Clear();
    m_canvas.SetTool(&m_remoteTool, &m_toolSettings, this);
    m_canvas.SetReadOnly(true);
    m_toolPanel.SetReadOnly(true);
    m_filePath.Empty();
}

void CMyPaintDlg::LeaveViewMode()
{
    m_remoteTool.Clear();
    m_canvas.SetRemoteCursor(false, CPoint(), CString());
    m_canvas.SetReadOnly(false);
    m_toolPanel.SetReadOnly(false);
    m_canvas.SetTool(m_tools[static_cast<size_t>(m_currentTool)].get(), &m_toolSettings, this);
    m_shareMode = ShareMode::None;
    m_viewRoom.clear();
    m_viewHost.clear();
    m_sharePanel.SetInputEnabled(false);
    UpdateShareUi();
}

bool CMyPaintDlg::LeaveViewForDocumentChange()
{
    if (m_shareMode == ShareMode::Viewing)
    {
        if (AfxMessageBox(L"공유 화면을 보는 중입니다. 공유에서 나가시겠습니까?", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return false;
        }
        StopViewing();
    }
    if (m_shareMode == ShareMode::ViewEnded)
    {
        LeaveViewMode();
    }
    return true;
}

void CMyPaintDlg::ShowSharePanel(bool show)
{
    if (m_sharePanelVisible == show)
    {
        return;
    }
    m_sharePanelVisible = show;
    m_sharePanel.ShowWindow(show ? SW_SHOW : SW_HIDE);
    m_toolPanel.SetChatVisible(show);
    LayoutControls();
}

CString CMyPaintDlg::ShareStatusText() const
{
    CString text;
    switch (m_shareMode)
    {
    case ShareMode::Hosting:
    {
        const std::vector<std::wstring> addresses = Share::LocalAddresses();
        text.Format(L"● 공유 중 · %s:%u · %d명 보는 중", addresses.empty() ? L"내 PC" : addresses.front().c_str(),
                    m_sharePort, (std::max)(0, m_participantCount - 1));
        break;
    }
    case ShareMode::Viewing:
        if (m_viewHost.empty())
        {
            text = L"● 연결 중...";
        }
        else
        {
            text.Format(L"● %s 님의 화면을 보는 중", m_viewHost.c_str());
        }
        break;
    case ShareMode::ViewEnded:
        text = L"공유가 끝났습니다 (보기 전용, 다른 이름으로 저장 가능)";
        break;
    default:
        break;
    }
    return text;
}

void CMyPaintDlg::UpdateShareUi()
{
    PanelShareState state = PanelShareState::Off;
    if (m_shareMode == ShareMode::Hosting)
    {
        state = PanelShareState::Hosting;
    }
    else if (m_shareMode == ShareMode::Viewing)
    {
        state = PanelShareState::Viewing;
    }
    m_toolPanel.SetShareState(state);
    UpdateTitle();
    UpdateStatusBar();
}

void CMyPaintDlg::OnShareStart()
{
    if (m_shareMode == ShareMode::Hosting)
    {
        return;
    }
    if (m_shareMode == ShareMode::Viewing)
    {
        if (AfxMessageBox(L"공유 화면을 보는 중입니다. 공유에서 나가고 내 그림을 공유하시겠습니까?",
                          MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }
        StopViewing();
    }
    if (m_shareMode == ShareMode::ViewEnded)
    {
        LeaveViewMode();
    }

    CShareStartDlg dialog(this);
    dialog.m_name = m_shareName;
    dialog.m_room = m_shareRoom;
    if (m_sharePort != 0)
    {
        dialog.m_port = m_sharePort;
    }
    if (dialog.DoModal() != IDOK)
    {
        return;
    }

    CommitTool();
    m_shareHost = std::make_unique<ShareHost>(m_document, *this);
    m_shareName = dialog.m_name;
    m_shareRoom = dialog.m_room;
    m_sharePort = dialog.m_port;
    m_participantCount = 1;
    m_sharePanel.Clear();
    m_shareMode = ShareMode::Hosting; // Start 안에서 참가자 목록 알림이 오므로 먼저 바꿉니다.
    std::wstring error;
    if (!m_shareHost->Start(m_shareName, m_shareRoom, m_sharePort, m_hWnd, WM_APP_NET, error))
    {
        m_shareMode = ShareMode::None;
        m_shareHost.reset();
        AfxMessageBox(error.c_str(), MB_ICONERROR);
        UpdateShareUi();
        return;
    }
    SetTimer(kShareTimer, kShareIntervalMs, nullptr);
    m_sharePanel.SetInputEnabled(true);
    CString notice;
    notice.Format(L"'%s' 공유를 시작했습니다. (포트 %u)", m_shareRoom.c_str(), m_sharePort);
    m_sharePanel.AppendChat(std::wstring(), notice.GetString(), true);
    ShowSharePanel(true);
    UpdateShareUi();
    m_canvas.SetFocus();
}

void CMyPaintDlg::OnShareJoin()
{
    if (m_shareMode == ShareMode::Hosting)
    {
        return;
    }
    if (m_shareMode == ShareMode::Viewing)
    {
        if (AfxMessageBox(L"지금 보는 공유에서 나가고 다른 방에 참가하시겠습니까?", MB_YESNO | MB_ICONQUESTION) != IDYES)
        {
            return;
        }
        StopViewing();
    }
    if (m_shareMode == ShareMode::None)
    {
        // 참가하면 지금 그림이 방장의 그림으로 바뀌므로 먼저 저장할지 묻습니다.
        CommitTool();
        if (!ConfirmSaveChanges())
        {
            return;
        }
    }

    CShareJoinDlg dialog(this);
    dialog.m_name = m_shareName;
    dialog.m_address = m_joinAddress;
    if (m_joinPort != 0)
    {
        dialog.m_port = m_joinPort;
    }
    if (dialog.DoModal() != IDOK)
    {
        return;
    }
    m_shareName = dialog.m_name;
    m_joinAddress = dialog.m_address;
    m_joinPort = dialog.m_port;

    if (m_shareMode != ShareMode::ViewEnded)
    {
        EnterViewMode();
    }
    m_remoteTool.Clear();
    m_canvas.InvalidateToolPreview();
    m_canvas.SetRemoteCursor(false, CPoint(), CString());
    m_shareMode = ShareMode::Viewing;
    m_viewRoom.clear();
    m_viewHost.clear();
    m_participantCount = 0;
    m_sharePanel.Clear();
    m_sharePanel.SetInputEnabled(false);
    CString notice;
    notice.Format(L"%s:%u 에 연결하는 중...", m_joinAddress.c_str(), m_joinPort);
    m_sharePanel.AppendChat(std::wstring(), notice.GetString(), true);
    ShowSharePanel(true);
    UpdateShareUi();

    m_shareViewer = std::make_unique<ShareViewer>(m_document, *this);
    m_shareViewer->Start(m_joinAddress, m_joinPort, m_shareName, m_hWnd, WM_APP_NET);
}

void CMyPaintDlg::OnShareStop()
{
    if (m_shareMode == ShareMode::Hosting)
    {
        StopHosting();
    }
    else if (m_shareMode == ShareMode::Viewing)
    {
        StopViewing();
    }
}

void CMyPaintDlg::StopHosting()
{
    KillTimer(kShareTimer);
    if (m_shareHost)
    {
        m_shareHost->Stop();
    }
    m_shareMode = ShareMode::None;
    m_participantCount = 0;
    m_sharePanel.SetInputEnabled(false);
    m_sharePanel.AppendChat(std::wstring(), L"공유를 끝냈습니다.", true);
    UpdateShareUi();
}

void CMyPaintDlg::StopViewing()
{
    const bool joined = m_shareViewer && m_shareViewer->IsJoined();
    if (m_shareViewer)
    {
        m_shareViewer->Stop();
    }
    if (joined)
    {
        // 들어가 있었으면 마지막 그림을 보기 전용으로 남깁니다.
        m_shareMode = ShareMode::ViewEnded;
        m_remoteTool.Clear();
        m_canvas.InvalidateToolPreview();
        m_canvas.SetRemoteCursor(false, CPoint(), CString());
        m_sharePanel.SetInputEnabled(false);
        m_sharePanel.AppendChat(std::wstring(), L"공유에서 나왔습니다.", true);
        UpdateShareUi();
    }
    else
    {
        // 연결하기 전이면 원래 그림 그대로 평소 상태로 돌아갑니다.
        m_sharePanel.AppendChat(std::wstring(), L"연결을 취소했습니다.", true);
        LeaveViewMode();
    }
}

void CMyPaintDlg::OnShareChatToggle()
{
    ShowSharePanel(!m_sharePanelVisible);
    if (m_sharePanelVisible)
    {
        m_sharePanel.FocusInput();
    }
    else
    {
        m_canvas.SetFocus();
    }
}

void CMyPaintDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kShareTimer)
    {
        if (m_shareMode == ShareMode::Hosting && m_shareHost)
        {
            m_shareHost->Tick(m_tools[static_cast<size_t>(m_currentTool)].get());
        }
        return;
    }
    CDialog::OnTimer(nIDEvent);
}

LRESULT CMyPaintDlg::OnNetMessage(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (m_shareMode == ShareMode::Hosting && m_shareHost)
    {
        m_shareHost->OnNetMessage();
    }
    else if (m_shareMode == ShareMode::Viewing && m_shareViewer)
    {
        m_shareViewer->OnNetMessage();
    }
    return 0;
}

LRESULT CMyPaintDlg::OnShareNotice(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (!m_pendingNotice.IsEmpty())
    {
        const CString notice = m_pendingNotice;
        m_pendingNotice.Empty();
        AfxMessageBox(notice, MB_ICONINFORMATION);
    }
    return 0;
}

void CMyPaintDlg::OnShareParticipants(const std::vector<std::wstring>& names)
{
    m_participantCount = static_cast<int>(names.size());
    m_sharePanel.SetParticipants(names, m_shareName);
    UpdateStatusBar();
}

void CMyPaintDlg::OnShareChat(const std::wstring& sender, const std::wstring& text, bool notice)
{
    m_sharePanel.AppendChat(sender, text, notice);
}

void CMyPaintDlg::OnViewerJoined(const std::wstring& room, const std::wstring& host, const std::wstring& myName)
{
    m_viewRoom = room;
    m_viewHost = host;
    m_shareName = myName; // 이름이 겹치면 방장이 "준호 (2)"처럼 바꿔 줍니다.
    m_sharePanel.SetInputEnabled(true);
    CString notice;
    notice.Format(L"'%s' 방에 들어왔습니다. (%s 님의 그림)", room.c_str(), host.c_str());
    m_sharePanel.AppendChat(std::wstring(), notice.GetString(), true);
    UpdateShareUi();
}

void CMyPaintDlg::OnViewerPreview(std::unique_ptr<Command> preview, const CRect& overlay)
{
    m_remoteTool.SetPreview(std::move(preview), overlay);
    m_canvas.InvalidateToolPreview();
}

void CMyPaintDlg::OnViewerCursor(bool visible, CPoint docPoint)
{
    m_canvas.SetRemoteCursor(visible, docPoint, CString(m_viewHost.c_str()));
}

void CMyPaintDlg::OnShareEnded(ShareEndReason reason, const std::wstring& detail)
{
    // 네트워크 메시지를 처리하는 중이므로 안내 대화 상자는 나중에(OnShareNotice) 띄웁니다.
    const bool joined = !m_viewHost.empty();
    CString notice;
    switch (reason)
    {
    case ShareEndReason::HostEnded:
        notice = L"방장이 공유를 끝냈습니다.\n마지막 그림은 그대로 남아 있으며 [다른 이름으로 저장]으로 저장할 수 있습니다.";
        break;
    case ShareEndReason::ConnectFailed:
        notice = L"방에 연결할 수 없습니다. 주소와 포트를 확인하세요.";
        break;
    case ShareEndReason::Rejected:
        notice = detail.empty() ? CString(L"방장이 참가를 거절했습니다.") : CString(detail.c_str());
        break;
    default:
        notice = L"방장과의 연결이 끊겼습니다.";
        if (!detail.empty())
        {
            notice += L"\n";
            notice += detail.c_str();
        }
        if (joined)
        {
            notice += L"\n마지막 그림은 그대로 남아 있으며 [다른 이름으로 저장]으로 저장할 수 있습니다.";
        }
        break;
    }

    m_sharePanel.AppendChat(std::wstring(), joined ? L"공유가 끝났습니다." : L"참가하지 못했습니다.", true);
    if (joined)
    {
        m_shareMode = ShareMode::ViewEnded;
        m_remoteTool.Clear();
        m_canvas.InvalidateToolPreview();
        m_canvas.SetRemoteCursor(false, CPoint(), CString());
        m_sharePanel.SetInputEnabled(false);
        UpdateShareUi();
    }
    else
    {
        LeaveViewMode();
    }
    m_pendingNotice = notice;
    PostMessage(WM_APP_SHARE_NOTICE);
}
