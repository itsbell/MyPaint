/**
 * @file ToolPanel.cpp
 * @brief CToolPanel 구현
 */
#include "pch.h"
#include "UI/ToolPanel.h"

#include <iterator>

#include "Resource.h"

#pragma comment(lib, "msimg32.lib") // AlphaBlend

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
// 색 (그림판과 비슷한 밝은 테마)
constexpr COLORREF kPanelBack = RGB(245, 246, 247);
constexpr COLORREF kPanelBorder = RGB(218, 220, 224);
constexpr COLORREF kText = RGB(32, 32, 32);
constexpr COLORREF kLabel = RGB(96, 96, 96);
constexpr COLORREF kHoverFill = RGB(232, 239, 247);
constexpr COLORREF kHoverBorder = RGB(164, 206, 249);
constexpr COLORREF kSelectedFill = RGB(201, 224, 247);
constexpr COLORREF kSelectedBorder = RGB(98, 162, 228);
constexpr COLORREF kSwatchBorder = RGB(160, 160, 160);
constexpr COLORREF kEmptySwatch = RGB(250, 250, 250);

// Segoe MDL2 Assets 글리프 (Windows 10/11 기본 글꼴). 코드 포인트로 적어 둡니다.
constexpr wchar_t kGlyphPencil[] = {static_cast<wchar_t>(0xE70F), 0};
constexpr wchar_t kGlyphEraser[] = {static_cast<wchar_t>(0xE75C), 0};
constexpr wchar_t kGlyphPicker[] = {static_cast<wchar_t>(0xEF3C), 0};
constexpr wchar_t kGlyphMagnifier[] = {static_cast<wchar_t>(0xE71E), 0};
constexpr wchar_t kGlyphBrush[] = {static_cast<wchar_t>(0xEE56), 0};
constexpr wchar_t kGlyphPalette[] = {static_cast<wchar_t>(0xE790), 0};
constexpr wchar_t kGlyphText[] = {static_cast<wchar_t>(0xE8D2), 0};
constexpr wchar_t kGlyphSelect[] = {static_cast<wchar_t>(0xE8B3), 0};
constexpr wchar_t kGlyphPaste[] = {static_cast<wchar_t>(0xE77F), 0};
constexpr wchar_t kGlyphCut[] = {static_cast<wchar_t>(0xE8C6), 0};
constexpr wchar_t kGlyphCopy[] = {static_cast<wchar_t>(0xE8C8), 0};
constexpr wchar_t kGlyphCrop[] = {static_cast<wchar_t>(0xE7A8), 0};
constexpr wchar_t kGlyphResize[] = {static_cast<wchar_t>(0xE740), 0};
constexpr wchar_t kGlyphRotate[] = {static_cast<wchar_t>(0xE7AD), 0};
constexpr wchar_t kGlyphShare[] = {static_cast<wchar_t>(0xE72D), 0};
constexpr wchar_t kGlyphView[] = {static_cast<wchar_t>(0xE7B3), 0};
constexpr wchar_t kGlyphChat[] = {static_cast<wchar_t>(0xE8BD), 0};

// 공유 버튼 상태 색
constexpr COLORREF kHostingFill = RGB(220, 242, 224);
constexpr COLORREF kHostingBorder = RGB(76, 160, 96);
constexpr COLORREF kHostingText = RGB(24, 110, 48);
constexpr COLORREF kViewingFill = RGB(219, 234, 252);
constexpr COLORREF kViewingBorder = RGB(78, 140, 214);
constexpr COLORREF kViewingText = RGB(22, 86, 160);

/** @brief 기본 팔레트 (위 줄 10색, 아래 줄 10색) */
constexpr COLORREF kPalette[] = {
    RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),    RGB(237, 28, 36),   RGB(255, 127, 39),
    RGB(255, 242, 0),   RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),   RGB(163, 73, 164),
    RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
    RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231),
};
constexpr int kPaletteColumns = 10;

/** @brief 패널 높이 (96 DPI 기준) */
constexpr int kPanelHeight = 98;

/** @brief 도형 그룹과 텍스트 그룹의 폭 (도구를 바꿔도 오른쪽 그룹이 움직이지 않게 같게 둡니다) */
constexpr int kContextGroupWidth = 250;

/** @brief 회전 메뉴 항목과 명령 */
struct RotateChoice
{
    const wchar_t* label;
    UINT command;
};
constexpr RotateChoice kRotateChoices[] = {
    {L"오른쪽으로 90° 회전", ID_IMAGE_ROTATE_RIGHT}, {L"왼쪽으로 90° 회전", ID_IMAGE_ROTATE_LEFT},
    {L"180° 회전", ID_IMAGE_ROTATE_180},            {L"좌우 대칭", ID_IMAGE_FLIP_H},
    {L"상하 대칭", ID_IMAGE_FLIP_V},
};

const wchar_t* GlyphForCommand(UINT id)
{
    switch (id)
    {
    case ID_EDIT_PASTE: return kGlyphPaste;
    case ID_EDIT_CUT: return kGlyphCut;
    case ID_EDIT_COPY: return kGlyphCopy;
    case ID_IMAGE_CROP: return kGlyphCrop;
    case ID_IMAGE_RESIZE: return kGlyphResize;
    default: return L"";
    }
}

CString ColorTip(COLORREF color)
{
    CString tip;
    tip.Format(L"RGB(%d, %d, %d)", GetRValue(color), GetGValue(color), GetBValue(color));
    return tip;
}
} // namespace

BEGIN_MESSAGE_MAP(CToolPanel, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

BOOL CToolPanel::Create(CWnd* parent, UINT id)
{
    const CString className = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::Create(className, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, CRect(0, 0, 0, 0), parent, id))
    {
        return FALSE;
    }
    m_tooltip.Create(this, TTS_ALWAYSTIP);
    m_tooltip.Activate(TRUE);
    Layout();
    return TRUE;
}

void CToolPanel::SetCurrentTool(ToolId tool)
{
    // 텍스트 도구일 때만 도형 그룹 자리에 텍스트 그룹이 나오므로 다시 배치합니다.
    const bool relayout = (tool == ToolId::Text) != (m_currentTool == ToolId::Text);
    m_currentTool = tool;
    if (GetSafeHwnd() != nullptr)
    {
        if (relayout)
        {
            m_hover = -1;
            Layout();
        }
        Invalidate(FALSE);
    }
}

UINT CToolPanel::Dpi() const
{
    return GetSafeHwnd() != nullptr ? ::GetDpiForWindow(m_hWnd) : USER_DEFAULT_SCREEN_DPI;
}

int CToolPanel::Scale(int value) const
{
    return ::MulDiv(value, static_cast<int>(Dpi()), USER_DEFAULT_SCREEN_DPI);
}

int CToolPanel::PreferredHeight() const
{
    return Scale(kPanelHeight);
}

int CToolPanel::PreferredWidth() const
{
    return m_contentWidth;
}

void CToolPanel::EnsureFonts()
{
    const UINT dpi = Dpi();
    if (dpi == m_fontDpi)
    {
        return;
    }
    m_fontDpi = dpi;
    m_textFont.DeleteObject();
    m_glyphFont.DeleteObject();
    m_bigGlyphFont.DeleteObject();

    auto makeFont = [this](CFont& font, int pixelHeight, const wchar_t* face) {
        LOGFONT lf{};
        lf.lfHeight = -Scale(pixelHeight);
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, face);
        font.CreateFontIndirect(&lf);
    };
    makeFont(m_textFont, 12, L"Segoe UI");
    makeFont(m_glyphFont, 16, L"Segoe MDL2 Assets");
    makeFont(m_bigGlyphFont, 26, L"Segoe MDL2 Assets");
}

// ---------------------------------------------------------------------------
// 배치
// ---------------------------------------------------------------------------

void CToolPanel::Layout()
{
    EnsureFonts();
    m_items.clear();
    m_groups.clear();

    const int labelTop = Scale(76);
    const int labelBottom = Scale(94);
    const int bigTop = Scale(8);
    const int bigBottom = Scale(70);
    int x = Scale(8);
    int groupStart = x;

    auto add = [this](ItemKind kind, int value, const CRect& rect, const CString& tip, const CString& label = CString()) {
        m_items.push_back({kind, value, rect, tip, label});
    };
    auto endGroup = [&](const wchar_t* label) {
        m_groups.push_back({label, CRect(groupStart, labelTop, x, labelBottom), x + Scale(8)});
        x += Scale(17);
        groupStart = x;
    };

    // 클립보드: 붙여넣기(큰 버튼), 잘라내기, 복사
    add(ItemKind::Command, ID_EDIT_PASTE, CRect(x, bigTop, x + Scale(56), bigBottom), L"붙여넣기 (Ctrl+V)", L"붙여넣기");
    x += Scale(58);
    add(ItemKind::Command, ID_EDIT_CUT, CRect(x, Scale(12), x + Scale(80), Scale(36)), L"잘라내기 (Ctrl+X)", L"잘라내기");
    add(ItemKind::Command, ID_EDIT_COPY, CRect(x, Scale(42), x + Scale(80), Scale(66)), L"복사 (Ctrl+C)", L"복사");
    x += Scale(80);
    endGroup(L"클립보드");

    // 이미지: 선택(큰 버튼), 자르기, 크기 조정, 회전
    add(ItemKind::Tool, static_cast<int>(ToolId::Select), CRect(x, bigTop, x + Scale(48), bigBottom),
        L"선택 (Ctrl+A: 모두 선택)", L"선택");
    x += Scale(50);
    add(ItemKind::Command, ID_IMAGE_CROP, CRect(x, Scale(8), x + Scale(96), Scale(29)), L"선택 영역으로 자르기 (Ctrl+Shift+X)",
        L"자르기");
    add(ItemKind::Command, ID_IMAGE_RESIZE, CRect(x, Scale(30), x + Scale(96), Scale(51)),
        L"크기 조정 및 기울이기 (Ctrl+W)", L"크기 조정");
    add(ItemKind::RotateMenu, 0, CRect(x, Scale(52), x + Scale(96), Scale(73)), L"회전 / 대칭", L"회전  ▾");
    x += Scale(96);
    endGroup(L"이미지");

    // 도구: 2줄 x 3칸
    {
        const int cell = Scale(28);
        const int size = Scale(26);
        struct Slot
        {
            ToolId tool;
            int col;
            int row;
            const wchar_t* tip;
        };
        const Slot slots[] = {
            {ToolId::Pencil, 0, 0, L"연필"},
            {ToolId::Fill, 1, 0, L"채우기"},
            {ToolId::Text, 2, 0, L"텍스트"},
            {ToolId::Eraser, 0, 1, L"지우개 (색 2로 지움)"},
            {ToolId::Picker, 1, 1, L"색 선택"},
            {ToolId::Magnifier, 2, 1, L"돋보기 (왼쪽: 확대, 오른쪽: 축소)"},
        };
        for (const Slot& s : slots)
        {
            const CPoint topLeft(x + s.col * cell, Scale(10) + s.row * Scale(32));
            add(ItemKind::Tool, static_cast<int>(s.tool), CRect(topLeft, CSize(size, size)), s.tip);
        }
        x += 3 * cell;
        endGroup(L"도구");
    }

    // 브러시
    add(ItemKind::Tool, static_cast<int>(ToolId::Brush), CRect(x, bigTop, x + Scale(48), bigBottom), L"브러시");
    x += Scale(48);
    endGroup(L"브러시");

    if (m_currentTool == ToolId::Text)
    {
        // 텍스트: 글꼴, 배경
        add(ItemKind::Font, 0, CRect(x, Scale(12), x + Scale(kContextGroupWidth), Scale(36)), L"글꼴 바꾸기");
        add(ItemKind::TextBackground, 0, CRect(x, Scale(42), x + Scale(kContextGroupWidth), Scale(66)),
            L"텍스트 배경 (불투명이면 색 2로 채움)");
        x += Scale(kContextGroupWidth);
        endGroup(L"텍스트");
    }
    else
    {
        // 도형: 7열 격자 + 윤곽선/채우기
        const int cell = Scale(22);
        constexpr int kColumns = 7;
        const int start = x;
        static_assert(std::size(kShapeDisplayOrder) == static_cast<size_t>(ShapeKind::Count),
                      "모든 도형이 패널에 나와야 합니다.");
        int i = 0;
        for (const ShapeKind kind : kShapeDisplayOrder)
        {
            const CPoint topLeft(x + (i % kColumns) * cell, Scale(9) + (i / kColumns) * cell);
            add(ItemKind::Shape, static_cast<int>(kind), CRect(topLeft, CSize(cell - Scale(1), cell - Scale(1))),
                ShapeKindName(kind));
            ++i;
        }
        x += kColumns * cell + Scale(8);
        const int right = start + Scale(kContextGroupWidth);
        add(ItemKind::Outline, 0, CRect(x, Scale(12), right, Scale(36)), L"도형 윤곽선");
        add(ItemKind::Fill, 0, CRect(x, Scale(42), right, Scale(66)), L"도형 채우기 (색 2)");
        x = right;
        endGroup(L"도형");
    }

    // 크기
    add(ItemKind::Size, 0, CRect(x, bigTop, x + Scale(48), bigBottom), L"크기 (선 두께)");
    x += Scale(48);
    endGroup(L"크기");

    // 색: 색 1, 색 2, 팔레트, 색 편집
    {
        add(ItemKind::ColorSlot, 0, CRect(x, bigTop, x + Scale(48), bigBottom), L"색 1 (왼쪽 버튼, 텍스트 색)");
        x += Scale(50);
        add(ItemKind::ColorSlot, 1, CRect(x, bigTop, x + Scale(48), bigBottom),
            L"색 2 (오른쪽 버튼, 지우개, 도형 채우기, 텍스트 배경)");
        x += Scale(56);

        const int cell = Scale(22);
        const int swatch = Scale(20);
        for (int i = 0; i < static_cast<int>(std::size(kPalette)); ++i)
        {
            const CPoint topLeft(x + (i % kPaletteColumns) * cell, Scale(9) + (i / kPaletteColumns) * cell);
            add(ItemKind::Palette, i, CRect(topLeft, CSize(swatch, swatch)), ColorTip(kPalette[i]));
        }
        for (int i = 0; i < kCustomColorCount; ++i)
        {
            const CPoint topLeft(x + i * cell, Scale(9) + 2 * cell);
            add(ItemKind::Custom, i, CRect(topLeft, CSize(swatch, swatch)), L"사용자 지정 색");
        }
        x += kPaletteColumns * cell + Scale(6);
        add(ItemKind::EditColors, 0, CRect(x, bigTop, x + Scale(52), bigBottom), L"색 편집", L"색 편집");
        x += Scale(52);
        endGroup(L"색");
    }

    // 공유: 공유 시작/참가/끝내기, 채팅 창
    m_shareGroupLeft = x - Scale(8);
    add(ItemKind::Share, 0, CRect(x, bigTop, x + Scale(64), bigBottom), L"실시간 공유", L"공유");
    x += Scale(66);
    add(ItemKind::Chat, 0, CRect(x, bigTop, x + Scale(48), bigBottom), L"참가자와 채팅 창 보이기/숨기기", L"채팅");
    x += Scale(48);
    endGroup(L"공유");
    m_contentWidth = x;

    // 툴팁은 항목 사각형마다 하나씩 다시 등록합니다.
    if (m_tooltip.GetSafeHwnd() != nullptr)
    {
        for (size_t i = 0; i < m_tooltipCount; ++i)
        {
            m_tooltip.DelTool(this, static_cast<UINT_PTR>(i + 1));
        }
        for (size_t i = 0; i < m_items.size(); ++i)
        {
            m_tooltip.AddTool(this, m_items[i].tip, m_items[i].rect, static_cast<UINT_PTR>(i + 1));
        }
        m_tooltipCount = m_items.size();
    }
}

void CToolPanel::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    if (Dpi() != m_fontDpi)
    {
        Layout(); // DPI가 바뀐 경우에만 다시 배치합니다.
    }
    Invalidate(FALSE);
}

BOOL CToolPanel::PreTranslateMessage(MSG* pMsg)
{
    if (m_tooltip.GetSafeHwnd() != nullptr)
    {
        m_tooltip.RelayEvent(pMsg);
    }
    return CWnd::PreTranslateMessage(pMsg);
}

// ---------------------------------------------------------------------------
// 그리기
// ---------------------------------------------------------------------------

BOOL CToolPanel::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CToolPanel::OnPaint()
{
    CPaintDC paintDc(this);
    CRect client;
    GetClientRect(&client);
    if (client.IsRectEmpty())
    {
        return;
    }
    EnsureFonts();

    CDC dc;
    dc.CreateCompatibleDC(&paintDc);
    CBitmap buffer;
    buffer.CreateCompatibleBitmap(&paintDc, client.Width(), client.Height());
    CBitmap* oldBitmap = dc.SelectObject(&buffer);

    dc.FillSolidRect(client, kPanelBack);
    dc.FillSolidRect(0, client.bottom - 1, client.Width(), 1, kPanelBorder);
    dc.SetBkMode(TRANSPARENT);

    CFont* oldFont = dc.SelectObject(&m_textFont);
    dc.SetTextColor(kLabel);
    for (const Group& g : m_groups)
    {
        CRect label = g.labelRect;
        dc.DrawText(g.label, label, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        dc.FillSolidRect(g.separatorX, Scale(8), 1, Scale(86), kPanelBorder);
    }

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        DrawItem(dc, m_items[i], static_cast<int>(i) == m_hover);
    }
    dc.SelectObject(oldFont);

    if (m_readOnly)
    {
        // 보기 전용: 공유 그룹을 뺀 나머지를 패널 배경색으로 반쯤 덮어 흐리게 보이게 합니다.
        CDC wash;
        wash.CreateCompatibleDC(&dc);
        CBitmap pixel;
        pixel.CreateCompatibleBitmap(&dc, 1, 1);
        CBitmap* oldPixel = wash.SelectObject(&pixel);
        wash.SetPixelV(0, 0, kPanelBack);
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 180, 0};
        ::AlphaBlend(dc.GetSafeHdc(), 0, 0, m_shareGroupLeft, client.Height() - 1, wash.GetSafeHdc(), 0, 0, 1, 1,
                     blend);
        wash.SelectObject(oldPixel);
    }

    paintDc.BitBlt(0, 0, client.Width(), client.Height(), &dc, 0, 0, SRCCOPY);
    dc.SelectObject(oldBitmap);
}

bool CToolPanel::IsSelected(const Item& item) const
{
    switch (item.kind)
    {
    case ItemKind::Tool:
        return static_cast<int>(m_currentTool) == item.value;
    case ItemKind::Shape:
        return m_currentTool == ToolId::Shape && m_settings != nullptr &&
               static_cast<int>(m_settings->shapeKind) == item.value;
    case ItemKind::ColorSlot:
        return m_settings != nullptr && m_settings->activeColorSlot == item.value;
    case ItemKind::Chat:
        return m_chatVisible;
    default:
        return false;
    }
}

void CToolPanel::SetReadOnly(bool readOnly)
{
    if (m_readOnly != readOnly)
    {
        m_readOnly = readOnly;
        m_hover = -1;
        if (GetSafeHwnd() != nullptr)
        {
            Invalidate(FALSE);
        }
    }
}

void CToolPanel::SetShareState(PanelShareState state)
{
    if (m_shareState != state)
    {
        m_shareState = state;
        if (GetSafeHwnd() != nullptr)
        {
            Invalidate(FALSE);
        }
    }
}

void CToolPanel::SetChatVisible(bool visible)
{
    if (m_chatVisible != visible)
    {
        m_chatVisible = visible;
        if (GetSafeHwnd() != nullptr)
        {
            Invalidate(FALSE);
        }
    }
}

void CToolPanel::DrawShareButton(CDC& dc, const CRect& rect, bool hover)
{
    const wchar_t* glyph = kGlyphShare;
    const wchar_t* label = L"공유  ▾";
    COLORREF fill = kPanelBack;
    COLORREF border = kPanelBack;
    COLORREF text = kText;
    switch (m_shareState)
    {
    case PanelShareState::Hosting:
        label = L"공유 중  ▾";
        fill = kHostingFill;
        border = kHostingBorder;
        text = kHostingText;
        break;
    case PanelShareState::Viewing:
        glyph = kGlyphView;
        label = L"보는 중  ▾";
        fill = kViewingFill;
        border = kViewingBorder;
        text = kViewingText;
        break;
    default:
        if (hover)
        {
            fill = kHoverFill;
            border = kHoverBorder;
        }
        break;
    }
    if (fill != kPanelBack || border != kPanelBack)
    {
        CBrush brush(fill);
        CPen pen(PS_SOLID, 1, border);
        CBrush* oldBrush = dc.SelectObject(&brush);
        CPen* oldPen = dc.SelectObject(&pen);
        dc.Rectangle(rect);
        dc.SelectObject(oldPen);
        dc.SelectObject(oldBrush);
    }
    dc.SetTextColor(text);
    DrawBigButton(dc, rect, glyph, label);
    dc.SetTextColor(kText);
}

void CToolPanel::DrawItem(CDC& dc, const Item& item, bool hover)
{
    if (item.kind == ItemKind::Share)
    {
        DrawShareButton(dc, item.rect, hover);
        return;
    }
    const bool selected = IsSelected(item);
    const bool isSwatch = item.kind == ItemKind::Palette || item.kind == ItemKind::Custom;

    if (!isSwatch && (selected || hover))
    {
        CBrush fill(selected ? kSelectedFill : kHoverFill);
        CPen border(PS_SOLID, 1, selected ? kSelectedBorder : kHoverBorder);
        CBrush* oldBrush = dc.SelectObject(&fill);
        CPen* oldPen = dc.SelectObject(&border);
        dc.Rectangle(item.rect);
        dc.SelectObject(oldPen);
        dc.SelectObject(oldBrush);
    }

    dc.SetTextColor(kText);
    CRect r = item.rect;
    switch (item.kind)
    {
    case ItemKind::Tool:
    {
        switch (static_cast<ToolId>(item.value))
        {
        case ToolId::Pencil: DrawGlyph(dc, r, kGlyphPencil, m_glyphFont); break;
        case ToolId::Fill: DrawFillIcon(dc, r); break;
        case ToolId::Text: DrawGlyph(dc, r, kGlyphText, m_glyphFont); break;
        case ToolId::Eraser: DrawGlyph(dc, r, kGlyphEraser, m_glyphFont); break;
        case ToolId::Picker: DrawGlyph(dc, r, kGlyphPicker, m_glyphFont); break;
        case ToolId::Magnifier: DrawGlyph(dc, r, kGlyphMagnifier, m_glyphFont); break;
        case ToolId::Brush: DrawGlyph(dc, r, kGlyphBrush, m_bigGlyphFont); break;
        case ToolId::Select: DrawBigButton(dc, r, kGlyphSelect, item.label); break;
        default: break;
        }
        break;
    }
    case ItemKind::Command:
        if (item.value == ID_EDIT_PASTE)
        {
            DrawBigButton(dc, r, GlyphForCommand(ID_EDIT_PASTE), item.label);
        }
        else
        {
            DrawSmallButton(dc, r, GlyphForCommand(static_cast<UINT>(item.value)), item.label);
        }
        break;
    case ItemKind::RotateMenu:
        DrawSmallButton(dc, r, kGlyphRotate, item.label);
        break;
    case ItemKind::Shape:
    {
        CRect icon = r;
        icon.DeflateRect(Scale(3), Scale(3));
        ShapeStyle style;
        style.outlineColor = kText;
        DrawShape(dc.GetSafeHdc(), static_cast<ShapeKind>(item.value), icon.TopLeft(),
                  icon.BottomRight() - CPoint(1, 1), style);
        break;
    }
    case ItemKind::Outline:
    case ItemKind::Fill:
    {
        const bool on = m_settings != nullptr &&
                        (item.kind == ItemKind::Outline ? m_settings->shapeOutline : m_settings->shapeFill);
        CString text;
        text.Format(L"%s: %s  ▾", item.kind == ItemKind::Outline ? L"윤곽선" : L"채우기", on ? L"단색" : L"없음");
        r.DeflateRect(Scale(6), 0);
        dc.DrawText(text, r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case ItemKind::Font:
    {
        CString text;
        if (m_settings != nullptr)
        {
            text.Format(L"글꼴: %s, %dpt%s%s%s", m_settings->textFace.c_str(), m_settings->textPointSize,
                        m_settings->textBold ? L", 굵게" : L"", m_settings->textItalic ? L", 기울임" : L"",
                        m_settings->textUnderline ? L", 밑줄" : L"");
        }
        r.DeflateRect(Scale(6), 0);
        dc.DrawText(text, r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
        break;
    }
    case ItemKind::TextBackground:
    {
        const bool opaque = m_settings != nullptr && m_settings->textOpaque;
        CString text;
        text.Format(L"배경: %s  ▾", opaque ? L"불투명" : L"투명");
        r.DeflateRect(Scale(6), 0);
        dc.DrawText(text, r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        break;
    }
    case ItemKind::Size:
        DrawSizeIcon(dc, r);
        break;
    case ItemKind::ColorSlot:
    {
        const int side = item.value == 0 ? Scale(30) : Scale(22);
        const CPoint center(r.CenterPoint().x, r.top + Scale(22));
        const CRect swatch(center.x - side / 2, center.y - side / 2, center.x - side / 2 + side,
                           center.y - side / 2 + side);
        DrawSwatch(dc, swatch, m_settings == nullptr ? RGB(0, 0, 0)
                                                     : (item.value == 0 ? m_settings->color1 : m_settings->color2));
        CRect label(r.left, r.bottom - Scale(22), r.right, r.bottom - Scale(4));
        dc.DrawText(item.value == 0 ? L"색 1" : L"색 2", label, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case ItemKind::Palette:
        DrawSwatch(dc, r, kPalette[item.value]);
        if (hover)
        {
            dc.Draw3dRect(r, kSelectedBorder, kSelectedBorder);
        }
        break;
    case ItemKind::Custom:
    {
        const COLORREF color = m_settings != nullptr ? m_settings->customColors[item.value] : CLR_INVALID;
        if (color == CLR_INVALID)
        {
            dc.FillSolidRect(r, kEmptySwatch);
            dc.Draw3dRect(r, kPanelBorder, kPanelBorder);
        }
        else
        {
            DrawSwatch(dc, r, color);
        }
        if (hover)
        {
            dc.Draw3dRect(r, kSelectedBorder, kSelectedBorder);
        }
        break;
    }
    case ItemKind::EditColors:
        DrawBigButton(dc, r, kGlyphPalette, item.label);
        break;
    case ItemKind::Chat:
        DrawBigButton(dc, r, kGlyphChat, item.label);
        break;
    case ItemKind::Share:
        break; // 위에서 그림
    }
}

void CToolPanel::DrawGlyph(CDC& dc, const CRect& rect, const wchar_t* glyph, CFont& font)
{
    CFont* oldFont = dc.SelectObject(&font);
    CRect r = rect;
    dc.DrawText(glyph, 1, r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    dc.SelectObject(oldFont);
}

void CToolPanel::DrawBigButton(CDC& dc, const CRect& rect, const wchar_t* glyph, const CString& label)
{
    CRect icon(rect.left, rect.top + Scale(4), rect.right, rect.top + Scale(38));
    DrawGlyph(dc, icon, glyph, m_bigGlyphFont);
    CRect text(rect.left, rect.bottom - Scale(26), rect.right, rect.bottom - Scale(4));
    dc.DrawText(label, text, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

void CToolPanel::DrawSmallButton(CDC& dc, const CRect& rect, const wchar_t* glyph, const CString& label)
{
    CRect icon(rect.left + Scale(2), rect.top, rect.left + Scale(24), rect.bottom);
    DrawGlyph(dc, icon, glyph, m_glyphFont);
    CRect text(icon.right + Scale(4), rect.top, rect.right, rect.bottom);
    dc.DrawText(label, text, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

void CToolPanel::DrawFillIcon(CDC& dc, const CRect& rect)
{
    // 기울어진 통과 떨어지는 물방울
    const double s = (std::min)(rect.Width(), rect.Height());
    const double cx = rect.CenterPoint().x - s * 0.06;
    const double cy = rect.CenterPoint().y + s * 0.02;
    auto pt = [&](double dx, double dy) { return CPoint(static_cast<int>(cx + dx * s), static_cast<int>(cy + dy * s)); };
    const POINT bucket[] = {pt(-0.30, -0.02), pt(-0.04, -0.28), pt(0.22, -0.02), pt(-0.04, 0.24)};

    CPen pen(PS_SOLID, (std::max)(1, Scale(1)), kText);
    CPen* oldPen = dc.SelectObject(&pen);
    CBrush* oldBrush = static_cast<CBrush*>(dc.SelectStockObject(NULL_BRUSH));
    dc.Polygon(bucket, static_cast<int>(std::size(bucket)));
    dc.MoveTo(pt(-0.17, -0.15));
    dc.LineTo(pt(-0.26, -0.33));

    CBrush drop(kText);
    dc.SelectObject(&drop);
    const CPoint d = pt(0.27, 0.16);
    const int radius = (std::max)(2, static_cast<int>(s * 0.08));
    dc.Ellipse(d.x - radius, d.y - radius, d.x + radius, d.y + radius + radius / 2);

    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);
}

void CToolPanel::DrawSizeIcon(CDC& dc, const CRect& rect)
{
    // 두께가 다른 가로줄 네 개
    const int left = rect.left + Scale(10);
    const int right = rect.right - Scale(10);
    int y = rect.top + Scale(14);
    for (int i = 1; i <= kSizeLevels; ++i)
    {
        const int thickness = Scale(i);
        const bool current = m_settings != nullptr && m_settings->sizeLevel == i - 1;
        dc.FillSolidRect(left, y, right - left, thickness, current ? kSelectedBorder : kText);
        y += thickness + Scale(7);
    }
}

void CToolPanel::DrawSwatch(CDC& dc, const CRect& rect, COLORREF color)
{
    dc.FillSolidRect(rect, color);
    dc.Draw3dRect(rect, kSwatchBorder, kSwatchBorder);
}

// ---------------------------------------------------------------------------
// 마우스
// ---------------------------------------------------------------------------

int CToolPanel::HitTest(CPoint point) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
    {
        const Item& item = m_items[i];
        if (m_readOnly && item.kind != ItemKind::Share && item.kind != ItemKind::Chat)
        {
            continue; // 보기 전용일 때는 공유 그룹만 누를 수 있습니다.
        }
        if (item.rect.PtInRect(point))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CToolPanel::OnMouseMove(UINT nFlags, CPoint point)
{
    if (!m_trackingLeave)
    {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = GetSafeHwnd();
        m_trackingLeave = ::TrackMouseEvent(&tme) != FALSE;
    }
    const int hover = HitTest(point);
    if (hover != m_hover)
    {
        m_hover = hover;
        Invalidate(FALSE);
    }
    CWnd::OnMouseMove(nFlags, point);
}

void CToolPanel::OnMouseLeave()
{
    m_trackingLeave = false;
    if (m_hover != -1)
    {
        m_hover = -1;
        Invalidate(FALSE);
    }
}

void CToolPanel::OnLButtonDown(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitTest(point);
    if (hit >= 0)
    {
        // 항목 처리 중에 다시 배치될 수 있으므로(텍스트 도구 전환) 복사해 둡니다.
        const Item item = m_items[static_cast<size_t>(hit)];
        Activate(item, MouseButton::Left);
    }
}

void CToolPanel::OnRButtonDown(UINT /*nFlags*/, CPoint point)
{
    const int hit = HitTest(point);
    if (hit >= 0)
    {
        const Item item = m_items[static_cast<size_t>(hit)];
        Activate(item, MouseButton::Right);
    }
}

WidthKind CToolPanel::CurrentWidthKind() const
{
    switch (m_currentTool)
    {
    case ToolId::Pencil: return WidthKind::Pencil;
    case ToolId::Eraser: return WidthKind::Eraser;
    default: return WidthKind::Brush;
    }
}

int CToolPanel::ShowChoiceMenu(const Item& item, const std::vector<CString>& choices, int checked)
{
    CMenu menu;
    menu.CreatePopupMenu();
    for (size_t i = 0; i < choices.size(); ++i)
    {
        menu.AppendMenu(MF_STRING | (static_cast<int>(i) == checked ? MF_CHECKED : 0), i + 1, choices[i]);
    }
    CPoint at(item.rect.left, item.rect.bottom);
    ClientToScreen(&at);
    const UINT chosen = menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY, at.x, at.y,
                                            this);
    return static_cast<int>(chosen) - 1;
}

void CToolPanel::Activate(const Item& item, MouseButton button)
{
    if (m_settings == nullptr || m_observer == nullptr)
    {
        return;
    }

    switch (item.kind)
    {
    case ItemKind::Tool:
        m_observer->OnPanelToolSelected(static_cast<ToolId>(item.value));
        return;
    case ItemKind::Shape:
        m_settings->shapeKind = static_cast<ShapeKind>(item.value);
        m_observer->OnPanelToolSelected(ToolId::Shape);
        return;
    case ItemKind::Command:
        m_observer->OnPanelCommand(static_cast<UINT>(item.value));
        return;
    case ItemKind::RotateMenu:
    {
        std::vector<CString> choices;
        for (const RotateChoice& c : kRotateChoices)
        {
            choices.push_back(c.label);
        }
        const int choice = ShowChoiceMenu(item, choices, -1);
        if (choice >= 0)
        {
            m_observer->OnPanelCommand(kRotateChoices[choice].command);
        }
        return;
    }
    case ItemKind::Font:
        m_observer->OnPanelEditFont();
        return;
    case ItemKind::TextBackground:
    {
        const int choice = ShowChoiceMenu(item, {L"투명", L"불투명"}, m_settings->textOpaque ? 1 : 0);
        if (choice < 0)
        {
            return;
        }
        m_settings->textOpaque = choice == 1;
        break;
    }
    case ItemKind::Outline:
    case ItemKind::Fill:
    {
        bool& flag = item.kind == ItemKind::Outline ? m_settings->shapeOutline : m_settings->shapeFill;
        const int choice = ShowChoiceMenu(item, {L"없음", L"단색"}, flag ? 1 : 0);
        if (choice < 0)
        {
            return;
        }
        flag = choice == 1;
        break;
    }
    case ItemKind::Size:
    {
        std::vector<CString> choices;
        for (int i = 0; i < kSizeLevels; ++i)
        {
            CString text;
            text.Format(L"%dpx", ToolSettings::WidthAt(CurrentWidthKind(), i));
            choices.push_back(text);
        }
        const int choice = ShowChoiceMenu(item, choices, m_settings->sizeLevel);
        if (choice < 0)
        {
            return;
        }
        m_settings->sizeLevel = choice;
        break;
    }
    case ItemKind::ColorSlot:
        m_settings->activeColorSlot = item.value;
        break;
    case ItemKind::Palette:
    case ItemKind::Custom:
    {
        const COLORREF color =
            item.kind == ItemKind::Palette ? kPalette[item.value] : m_settings->customColors[item.value];
        if (color == CLR_INVALID)
        {
            return; // 비어 있는 사용자 지정 칸
        }
        // 오른쪽 클릭은 항상 색 2, 왼쪽 클릭은 선택된 칸에 넣습니다.
        if (button == MouseButton::Right || m_settings->activeColorSlot == 1)
        {
            m_settings->color2 = color;
        }
        else
        {
            m_settings->color1 = color;
        }
        break;
    }
    case ItemKind::EditColors:
        m_observer->OnPanelEditColors();
        return;
    case ItemKind::Chat:
        m_observer->OnPanelCommand(ID_SHARE_CHAT);
        return;
    case ItemKind::Share:
    {
        // 상태에 따라 고를 수 있는 일이 다릅니다.
        std::vector<CString> choices;
        std::vector<UINT> commands;
        if (m_shareState == PanelShareState::Off)
        {
            choices = {L"공유 시작...", L"공유 참가..."};
            commands = {ID_SHARE_START, ID_SHARE_JOIN};
        }
        else
        {
            choices = {m_shareState == PanelShareState::Hosting ? L"공유 끝내기" : L"공유에서 나가기"};
            commands = {ID_SHARE_STOP};
        }
        const int choice = ShowChoiceMenu(item, choices, -1);
        if (choice >= 0)
        {
            m_observer->OnPanelCommand(commands[static_cast<size_t>(choice)]);
        }
        return;
    }
    }

    Invalidate(FALSE);
    m_observer->OnPanelSettingsChanged();
}
