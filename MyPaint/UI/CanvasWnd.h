#pragma once

/**
 * @file CanvasWnd.h
 * @brief 문서를 보여 주고 마우스 입력을 도구로 넘기는 캔버스 창
 */

#include "Core/Document.h"
#include "Core/Surface.h"
#include "Tools/Tool.h"

/**
 * @brief 캔버스 상태 변화를 받는 쪽 (상태 표시줄 갱신용)
 */
class ICanvasObserver
{
public:
    virtual ~ICanvasObserver() = default;

    /** @brief 마우스가 움직였습니다. inside가 false면 이미지 밖입니다. */
    virtual void OnCanvasCursor(bool inside, CPoint docPoint) = 0;

    /** @brief 확대 배율이 바뀌었습니다. */
    virtual void OnCanvasZoomChanged(double zoom) = 0;

    /** @brief 크기 조절 핸들을 끄는 중입니다. active가 false면 끝났거나 취소됐습니다. */
    virtual void OnCanvasResizing(bool active, CSize newSize) = 0;

    /** @brief 크기 조절 핸들을 놓았습니다. 캔버스를 newSize로 바꿔 달라는 요청입니다. */
    virtual void OnCanvasResizeRequested(CSize newSize) = 0;
};

/**
 * @brief 캔버스 창
 *
 * - 문서 위에 도구 미리보기를 합성한 사본(m_composite)을 확대/축소해 그립니다.
 * - 확대/축소, 스크롤 같은 보기 상태는 여기에만 있고 문서에는 없습니다.
 */
class CCanvasWnd : public CWnd, public IDocumentListener
{
public:
    CCanvasWnd() = default;
    ~CCanvasWnd() override;

    /** @brief 자식 창으로 만듭니다. */
    BOOL Create(CWnd* parent, UINT id);

    /** @brief 보여 줄 문서를 연결합니다. */
    void SetDocument(Document* document);

    /** @brief 현재 도구, 설정, 도구가 부를 호스트를 연결합니다. 이전 도구의 작업은 확정됩니다. */
    void SetTool(Tool* tool, const ToolSettings* settings, IToolHost* host);

    void SetObserver(ICanvasObserver* observer) { m_observer = observer; }

    /** @brief 진행 중인 드래그를 취소합니다. */
    void CancelTool();

    /** @brief 드래그를 취소하고, 남아 있는 작업(선택 영역, 텍스트)을 문서에 확정합니다. */
    void CommitTool();

    /** @brief Esc: 드래그 중이면 취소, 아니면 남은 작업을 확정합니다. */
    void HandleEscape();

    /** @brief 도구 설정이 바뀌었음을 도구에 알리고 미리보기를 다시 그립니다. */
    void NotifySettingsChanged();

    /** @brief 도구 상태가 밖에서 바뀌었을 때(선택 영역 붙여넣기 등) 미리보기를 다시 그립니다. */
    void InvalidateToolPreview() { InvalidatePreview(); }

    /** @brief 화면에 보이는 영역의 왼쪽 위 문서 좌표 (0 이상) */
    CPoint VisibleDocOrigin() const;

    double Zoom() const { return m_zoom; }
    /** @brief 한 단계 확대/축소합니다. anchor는 SetZoom과 같습니다. */
    void ZoomIn(const CPoint* anchor = nullptr);
    void ZoomOut(const CPoint* anchor = nullptr);

    /**
     * @brief 확대 배율을 바꿉니다.
     * @param anchor 화면에서 고정할 클라이언트 좌표 (nullptr이면 창 가운데)
     */
    void SetZoom(double zoom, const CPoint* anchor = nullptr);

    /** @brief 문서 좌표 docPoint가 화면에서 제자리에 있도록 한 단계 확대/축소합니다. */
    void ZoomAtDoc(CPoint docPoint, bool zoomIn);

    // IDocumentListener
    void OnDocumentChanged(const CRect& dirty) override;
    void OnDocumentReset() override;

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnMouseLeave();
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg UINT OnGetDlgCode();
    afx_msg LRESULT OnImeStartComposition(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnImeComposition(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnImeEndComposition(WPARAM wParam, LPARAM lParam);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnDestroy();
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 이미지 오른쪽/아래/모서리의 캔버스 크기 조절 핸들 */
    enum class ResizeHandle
    {
        None,
        Right,
        Bottom,
        Corner,
    };

    /** @brief 핸들의 클라이언트 사각형 */
    CRect HandleRect(ResizeHandle handle) const;

    /** @brief client 위치에 있는 핸들 (없으면 None) */
    ResizeHandle HitHandle(CPoint client) const;

    /** @brief 핸들을 그리고, 끄는 중이면 새 크기를 점선으로 보여 줍니다. */
    void DrawResizeHandles(CDC& dc);

    void BeginResize(ResizeHandle handle);
    void UpdateResize(CPoint client);
    void EndResize(bool apply);

    void BeginDrag(CPoint clientPoint, MouseButton button, bool shift);
    void EndDrag(CPoint clientPoint, bool shift);
    void InvalidatePreview();

    /** @brief 도구에 넘길 문맥 (Shift 상태 포함) */
    ToolContext MakeContext(bool shift) const;

    /** @brief 드래그 중 Shift가 바뀌면 마지막 위치로 이동을 다시 알려 미리보기를 갱신합니다. */
    void RefreshDragForModifiers();

    /** @brief 문서 영역을 합성 사본에 복사하고 미리보기를 얹습니다. */
    void UpdateComposite(const CRect& docRect);

    /** @brief 선택 영역/텍스트 상자의 점선 테두리를 그립니다. (클라이언트 좌표 DC) */
    void DrawOverlay(CDC& dc);

    /** @brief 클라이언트 좌표 → 문서 좌표 (픽셀 단위로 내림) */
    CPoint ClientToDoc(CPoint client) const;

    /** @brief 문서 좌표 x(또는 y)가 시작되는 클라이언트 좌표 */
    int DocToClientX(double x) const;
    int DocToClientY(double y) const;

    /** @brief 문서 사각형 → 클라이언트 사각형 */
    CRect DocToClient(const CRect& docRect) const;

    /** @brief 이미지 바깥 여백까지 포함한 전체 내용 크기 (클라이언트 픽셀) */
    CSize ContentSize() const;

    void InvalidateDoc(const CRect& docRect);
    void UpdateScrollBars();
    void ScrollTo(CPoint position);
    void HandleScroll(int bar, UINT code);
    void ReportCursor(CPoint clientPoint);

    Document* m_document = nullptr;
    Tool* m_tool = nullptr;
    const ToolSettings* m_settings = nullptr;
    IToolHost* m_host = nullptr;
    ICanvasObserver* m_observer = nullptr;

    Surface m_composite;     ///< 문서 + 미리보기 합성 사본 (문서와 같은 크기)
    double m_zoom = 1.0;     ///< 확대 배율 (1.0 = 100%)
    CPoint m_scroll;         ///< 스크롤 위치 (클라이언트 픽셀)
    bool m_dragging = false; ///< 도구 드래그 중인지
    MouseButton m_dragButton = MouseButton::Left;
    CPoint m_lastDragPoint;  ///< 드래그 중 마지막 마우스 위치 (클라이언트 좌표)
    bool m_trackingLeave = false;

    ResizeHandle m_resizing = ResizeHandle::None; ///< 끄는 중인 크기 조절 핸들
    CSize m_resizeSize;                           ///< 끄는 중인 새 캔버스 크기
};
