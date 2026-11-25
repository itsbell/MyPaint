/**
 * @file CanvasWnd.cpp
 * @brief CCanvasWnd 구현
 */
#include "pch.h"
#include "UI/CanvasWnd.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
/** @brief 이미지 둘레 여백 (클라이언트 픽셀) */
constexpr int kMargin = 5;

/** @brief 스크롤 버튼 한 번에 움직이는 양 */
constexpr int kLineScroll = 30;

/** @brief 캔버스 바깥 배경색 */
constexpr COLORREF kBackground = RGB(201, 211, 226);

/** @brief 확대/축소 단계 */
constexpr double kZoomLevels[] = {0.125, 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
constexpr double kZoomEpsilon = 1e-6;
} // namespace

BEGIN_MESSAGE_MAP(CCanvasWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_WM_CAPTURECHANGED()
    ON_WM_SETCURSOR()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

CCanvasWnd::~CCanvasWnd()
{
    if (m_document != nullptr)
    {
        m_document->RemoveListener(this);
    }
}

BOOL CCanvasWnd::Create(CWnd* parent, UINT id)
{
    const CString className = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    return CWnd::Create(className, nullptr, WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_CLIPSIBLINGS,
                        CRect(0, 0, 0, 0), parent, id);
}

void CCanvasWnd::SetDocument(Document* document)
{
    if (m_document != nullptr)
    {
        m_document->RemoveListener(this);
    }
    m_document = document;
    if (m_document != nullptr)
    {
        m_document->AddListener(this);
        OnDocumentReset();
    }
}

void CCanvasWnd::SetTool(Tool* tool, const ToolSettings* settings, IToolHost* host)
{
    CancelTool();
    m_tool = tool;
    m_settings = settings;
    m_host = host;
}

ToolContext CCanvasWnd::MakeContext(bool shift) const
{
    return ToolContext{*m_document, *m_settings, *m_host, shift};
}

void CCanvasWnd::CancelTool()
{
    if (m_tool != nullptr && m_tool->IsActive())
    {
        m_tool->Cancel();
        InvalidatePreview();
    }
    if (m_dragging)
    {
        m_dragging = false;
        if (GetCapture() == this)
        {
            ReleaseCapture();
        }
    }
}

// ---------------------------------------------------------------------------
// 좌표 변환
// ---------------------------------------------------------------------------

int CCanvasWnd::DocToClientX(double x) const
{
    return kMargin + static_cast<int>(std::floor(x * m_zoom)) - m_scroll.x;
}

int CCanvasWnd::DocToClientY(double y) const
{
    return kMargin + static_cast<int>(std::floor(y * m_zoom)) - m_scroll.y;
}

CRect CCanvasWnd::DocToClient(const CRect& docRect) const
{
    return CRect(DocToClientX(docRect.left), DocToClientY(docRect.top), DocToClientX(docRect.right),
                 DocToClientY(docRect.bottom));
}

CPoint CCanvasWnd::ClientToDoc(CPoint client) const
{
    return CPoint(static_cast<int>(std::floor((client.x + m_scroll.x - kMargin) / m_zoom)),
                  static_cast<int>(std::floor((client.y + m_scroll.y - kMargin) / m_zoom)));
}

CSize CCanvasWnd::ContentSize() const
{
    const CSize size = m_document != nullptr ? m_document->Size() : CSize(0, 0);
    return CSize(static_cast<int>(std::ceil(size.cx * m_zoom)) + 2 * kMargin,
                 static_cast<int>(std::ceil(size.cy * m_zoom)) + 2 * kMargin);
}

// ---------------------------------------------------------------------------
// 문서 알림
// ---------------------------------------------------------------------------

void CCanvasWnd::OnDocumentChanged(const CRect& dirty)
{
    InvalidateDoc(dirty);
}

void CCanvasWnd::OnDocumentReset()
{
    if (m_document != nullptr)
    {
        const CSize size = m_document->Size();
        m_composite.Create(size.cx, size.cy, RGB(255, 255, 255));
    }
    if (GetSafeHwnd() != nullptr)
    {
        UpdateScrollBars();
        Invalidate(FALSE);
    }
}

void CCanvasWnd::InvalidateDoc(const CRect& docRect)
{
    if (GetSafeHwnd() == nullptr || docRect.IsRectEmpty())
    {
        return;
    }
    CRect r = DocToClient(docRect);
    r.InflateRect(1, 1); // 축소 배율에서 반올림으로 생기는 틈 방지
    InvalidateRect(r, FALSE);
}

void CCanvasWnd::InvalidatePreview()
{
    if (m_tool != nullptr)
    {
        InvalidateDoc(m_tool->TakePreviewDirty());
    }
}

// ---------------------------------------------------------------------------
// 그리기
// ---------------------------------------------------------------------------

BOOL CCanvasWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE; // OnPaint에서 전부 그립니다.
}

void CCanvasWnd::OnPaint()
{
    CPaintDC dc(this);
    const CRect clip(dc.m_ps.rcPaint);
    if (clip.IsRectEmpty())
    {
        return;
    }

    // 더블 버퍼: 무효 영역 크기만큼만 메모리 비트맵을 만듭니다.
    CDC mem;
    mem.CreateCompatibleDC(&dc);
    CBitmap buffer;
    buffer.CreateCompatibleBitmap(&dc, clip.Width(), clip.Height());
    CBitmap* oldBitmap = mem.SelectObject(&buffer);
    mem.SetViewportOrg(-clip.left, -clip.top);
    mem.FillSolidRect(clip, kBackground);

    if (m_document != nullptr && m_composite.IsValid())
    {
        const CRect docBounds(CPoint(0, 0), m_document->Size());
        CRect visible;
        if (visible.IntersectRect(DocToClient(docBounds), clip))
        {
            CRect docRect(ClientToDoc(visible.TopLeft()), ClientToDoc(visible.BottomRight() - CPoint(1, 1)) + CPoint(1, 1));
            docRect &= docBounds;
            UpdateComposite(docRect);

            const CRect dest = DocToClient(docRect);
            if (m_zoom < 1.0)
            {
                mem.SetStretchBltMode(HALFTONE);
                ::SetBrushOrgEx(mem.GetSafeHdc(), 0, 0, nullptr);
            }
            else
            {
                mem.SetStretchBltMode(COLORONCOLOR);
            }
            ::StretchBlt(mem.GetSafeHdc(), dest.left, dest.top, dest.Width(), dest.Height(), m_composite.DC(),
                         docRect.left, docRect.top, docRect.Width(), docRect.Height(), SRCCOPY);
        }
    }

    dc.BitBlt(clip.left, clip.top, clip.Width(), clip.Height(), &mem, clip.left, clip.top, SRCCOPY);
    mem.SelectObject(oldBitmap);
}

void CCanvasWnd::UpdateComposite(const CRect& docRect)
{
    m_composite.CopyFrom(m_document->GetSurface(), docRect);

    if (m_tool == nullptr || !m_tool->IsActive())
    {
        return;
    }
    if (const Command* preview = m_tool->PreviewCommand())
    {
        // 확정될 때와 같은 코드로 그려서 미리보기와 결과가 픽셀 단위로 같게 합니다.
        HDC dc = m_composite.DC();
        const int saved = ::SaveDC(dc);
        ::IntersectClipRect(dc, docRect.left, docRect.top, docRect.right, docRect.bottom);
        preview->Execute(m_composite);
        ::RestoreDC(dc, saved);
    }
}

// ---------------------------------------------------------------------------
// 스크롤과 확대/축소
// ---------------------------------------------------------------------------

void CCanvasWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScrollBars();
    Invalidate(FALSE);
}

void CCanvasWnd::UpdateScrollBars()
{
    if (GetSafeHwnd() == nullptr)
    {
        return;
    }
    CRect client;
    GetClientRect(&client);
    const CSize content = ContentSize();
    m_scroll.x = std::clamp(m_scroll.x, 0L, (std::max)(0L, content.cx - client.Width()));
    m_scroll.y = std::clamp(m_scroll.y, 0L, (std::max)(0L, content.cy - client.Height()));

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    si.nMin = 0;

    si.nMax = content.cx - 1;
    si.nPage = static_cast<UINT>(client.Width());
    si.nPos = m_scroll.x;
    SetScrollInfo(SB_HORZ, &si, TRUE);

    si.nMax = content.cy - 1;
    si.nPage = static_cast<UINT>(client.Height());
    si.nPos = m_scroll.y;
    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CCanvasWnd::ScrollTo(CPoint position)
{
    CRect client;
    GetClientRect(&client);
    const CSize content = ContentSize();
    position.x = std::clamp(position.x, 0L, (std::max)(0L, content.cx - client.Width()));
    position.y = std::clamp(position.y, 0L, (std::max)(0L, content.cy - client.Height()));
    if (position == m_scroll)
    {
        return;
    }
    m_scroll = position;
    SetScrollPos(SB_HORZ, m_scroll.x);
    SetScrollPos(SB_VERT, m_scroll.y);
    Invalidate(FALSE);
}

void CCanvasWnd::HandleScroll(int bar, UINT code)
{
    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    GetScrollInfo(bar, &si, SIF_ALL);

    int pos = si.nPos;
    switch (code)
    {
    case SB_LINEUP:
        pos -= kLineScroll;
        break;
    case SB_LINEDOWN:
        pos += kLineScroll;
        break;
    case SB_PAGEUP:
        pos -= static_cast<int>(si.nPage);
        break;
    case SB_PAGEDOWN:
        pos += static_cast<int>(si.nPage);
        break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
        pos = si.nTrackPos;
        break;
    case SB_TOP:
        pos = 0;
        break;
    case SB_BOTTOM:
        pos = si.nMax;
        break;
    default:
        return;
    }

    CPoint next = m_scroll;
    (bar == SB_HORZ ? next.x : next.y) = pos;
    ScrollTo(next);
}

void CCanvasWnd::OnHScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
    HandleScroll(SB_HORZ, nSBCode);
}

void CCanvasWnd::OnVScroll(UINT nSBCode, UINT /*nPos*/, CScrollBar* /*pScrollBar*/)
{
    HandleScroll(SB_VERT, nSBCode);
}

BOOL CCanvasWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if ((nFlags & MK_CONTROL) != 0)
    {
        // Ctrl + 휠: 마우스 위치를 기준으로 확대/축소
        ScreenToClient(&pt);
        if (zDelta > 0)
        {
            ZoomIn(&pt);
        }
        else if (zDelta < 0)
        {
            ZoomOut(&pt);
        }
        return TRUE;
    }

    const int amount = -zDelta * kLineScroll * 3 / WHEEL_DELTA;
    if ((nFlags & MK_SHIFT) != 0)
    {
        ScrollTo(m_scroll + CPoint(amount, 0));
    }
    else
    {
        ScrollTo(m_scroll + CPoint(0, amount));
    }
    return TRUE;
}

void CCanvasWnd::ZoomIn(const CPoint* anchor)
{
    for (double z : kZoomLevels)
    {
        if (z > m_zoom + kZoomEpsilon)
        {
            SetZoom(z, anchor);
            return;
        }
    }
}

void CCanvasWnd::ZoomOut(const CPoint* anchor)
{
    for (auto it = std::rbegin(kZoomLevels); it != std::rend(kZoomLevels); ++it)
    {
        if (*it < m_zoom - kZoomEpsilon)
        {
            SetZoom(*it, anchor);
            return;
        }
    }
}

void CCanvasWnd::SetZoom(double zoom, const CPoint* anchor)
{
    zoom = std::clamp(zoom, kZoomLevels[0], kZoomLevels[std::size(kZoomLevels) - 1]);
    if (std::abs(zoom - m_zoom) < kZoomEpsilon)
    {
        return;
    }

    CRect client;
    GetClientRect(&client);
    const CPoint a = anchor != nullptr ? *anchor : client.CenterPoint();

    // 기준점 아래에 있던 문서 위치가 확대 후에도 같은 자리에 오도록 스크롤을 맞춥니다.
    const double docX = (a.x + m_scroll.x - kMargin) / m_zoom;
    const double docY = (a.y + m_scroll.y - kMargin) / m_zoom;
    m_zoom = zoom;
    m_scroll.x = static_cast<int>(std::lround(docX * m_zoom + kMargin - a.x));
    m_scroll.y = static_cast<int>(std::lround(docY * m_zoom + kMargin - a.y));

    UpdateScrollBars();
    Invalidate(FALSE);
    if (m_observer != nullptr)
    {
        m_observer->OnCanvasZoomChanged(m_zoom);
    }
}

void CCanvasWnd::ZoomAtDoc(CPoint docPoint, bool zoomIn)
{
    // 픽셀의 가운데를 기준으로 삼아 확대 후에도 같은 픽셀이 마우스 아래에 오게 합니다.
    const CPoint anchor(DocToClientX(docPoint.x + 0.5), DocToClientY(docPoint.y + 0.5));
    if (zoomIn)
    {
        ZoomIn(&anchor);
    }
    else
    {
        ZoomOut(&anchor);
    }
}

// ---------------------------------------------------------------------------
// 마우스
// ---------------------------------------------------------------------------

void CCanvasWnd::BeginDrag(CPoint clientPoint, MouseButton button, bool shift)
{
    if (m_tool == nullptr || m_document == nullptr || m_settings == nullptr || m_host == nullptr)
    {
        return;
    }
    SetFocus();
    m_dragging = true;
    m_dragButton = button;
    m_lastDragPoint = clientPoint;
    SetCapture();

    ToolContext context = MakeContext(shift);
    m_tool->OnDown(context, ClientToDoc(clientPoint), button);
    InvalidatePreview();
}

void CCanvasWnd::EndDrag(CPoint clientPoint, bool shift)
{
    // ReleaseCapture가 OnCaptureChanged를 부르므로, 먼저 드래그 상태를 끕니다.
    // 도구가 OnUp에서 호스트를 통해 도구를 바꿀 수 있으므로(스포이트) 이후에는 m_tool이 달라질 수 있습니다.
    m_dragging = false;
    ToolContext context = MakeContext(shift);
    m_tool->OnUp(context, ClientToDoc(clientPoint));
    ReleaseCapture();
    InvalidatePreview();
}

void CCanvasWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (!m_dragging)
    {
        BeginDrag(point, MouseButton::Left, (nFlags & MK_SHIFT) != 0);
    }
}

void CCanvasWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_dragging && m_dragButton == MouseButton::Left)
    {
        EndDrag(point, (nFlags & MK_SHIFT) != 0);
    }
}

void CCanvasWnd::OnRButtonDown(UINT nFlags, CPoint point)
{
    if (m_dragging)
    {
        // 그림판처럼, 왼쪽 드래그 중 오른쪽 버튼을 누르면 취소합니다.
        if (m_dragButton == MouseButton::Left)
        {
            CancelTool();
        }
        return;
    }
    BeginDrag(point, MouseButton::Right, (nFlags & MK_SHIFT) != 0);
}

void CCanvasWnd::OnRButtonUp(UINT nFlags, CPoint point)
{
    if (m_dragging && m_dragButton == MouseButton::Right)
    {
        EndDrag(point, (nFlags & MK_SHIFT) != 0);
    }
}

void CCanvasWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_trackingLeave)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = GetSafeHwnd();
        m_trackingLeave = ::TrackMouseEvent(&tme) != FALSE;
    }
    ReportCursor(point);

    if (m_dragging)
    {
        m_lastDragPoint = point;
        ToolContext context = MakeContext((nFlags & MK_SHIFT) != 0);
        m_tool->OnMove(context, ClientToDoc(point));
        InvalidatePreview();
    }
}

void CCanvasWnd::RefreshDragForModifiers()
{
    if (m_dragging && m_tool != nullptr)
    {
        ToolContext context = MakeContext((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
        m_tool->OnMove(context, ClientToDoc(m_lastDragPoint));
        InvalidatePreview();
    }
}

void CCanvasWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_SHIFT)
    {
        RefreshDragForModifiers();
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CCanvasWnd::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    if (nChar == VK_SHIFT)
    {
        RefreshDragForModifiers();
    }
    CWnd::OnKeyUp(nChar, nRepCnt, nFlags);
}

void CCanvasWnd::OnMouseLeave()
{
    m_trackingLeave = false;
    if (m_observer != nullptr)
    {
        m_observer->OnCanvasCursor(false, CPoint());
    }
}

void CCanvasWnd::OnCaptureChanged(CWnd* pWnd)
{
    if (m_dragging && pWnd != this)
    {
        CancelTool(); // 다른 창이 캡처를 가져가면 진행 중인 작업을 버립니다.
    }
    CWnd::OnCaptureChanged(pWnd);
}

BOOL CCanvasWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest == HTCLIENT && m_tool != nullptr)
    {
        ::SetCursor(m_tool->Cursor());
        return TRUE;
    }
    return CWnd::OnSetCursor(pWnd, nHitTest, message);
}

void CCanvasWnd::ReportCursor(CPoint clientPoint)
{
    if (m_observer == nullptr || m_document == nullptr)
    {
        return;
    }
    const CPoint docPoint = ClientToDoc(clientPoint);
    const CRect docBounds(CPoint(0, 0), m_document->Size());
    m_observer->OnCanvasCursor(docBounds.PtInRect(docPoint) != FALSE, docPoint);
}

void CCanvasWnd::OnDestroy()
{
    CancelTool();
    if (m_document != nullptr)
    {
        m_document->RemoveListener(this);
        m_document = nullptr;
    }
    CWnd::OnDestroy();
}
