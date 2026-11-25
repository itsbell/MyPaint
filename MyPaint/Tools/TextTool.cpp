/**
 * @file TextTool.cpp
 * @brief TextTool 구현
 */
#include "pch.h"
#include "Tools/TextTool.h"

#include "Core/Document.h"
#include "Core/ImageCommands.h"

namespace
{
/** @brief 클릭만 했을 때 만드는 상자의 최소 폭 */
constexpr int kMinBoxWidth = 120;

/** @brief 미리보기에서 입력 위치를 보여 주는 표시 */
constexpr wchar_t kCaret = L'|';

constexpr UINT kDrawFlags = DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL | DT_EXPANDTABS;

HFONT CreateTextFont(const TextStyle& style)
{
    LOGFONTW lf{};
    lf.lfHeight = -::MulDiv(style.pointSize, USER_DEFAULT_SCREEN_DPI, 72); // 문서는 96 DPI로 봅니다.
    lf.lfWeight = style.bold ? FW_BOLD : FW_NORMAL;
    lf.lfItalic = style.italic ? TRUE : FALSE;
    lf.lfUnderline = style.underline ? TRUE : FALSE;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = ANTIALIASED_QUALITY;
    wcsncpy_s(lf.lfFaceName, style.face.c_str(), _TRUNCATE);
    return ::CreateFontIndirectW(&lf);
}

/** @brief 상자 안에 글자를 그립니다. (불투명이면 상자를 배경색으로 먼저 채움) */
void RenderText(HDC dc, const CRect& box, const TextStyle& style, const std::wstring& text)
{
    if (style.opaque)
    {
        HBRUSH brush = ::CreateSolidBrush(style.background);
        ::FillRect(dc, &box, brush);
        ::DeleteObject(brush);
    }
    if (text.empty())
    {
        return;
    }
    HFONT font = CreateTextFont(style);
    HGDIOBJ oldFont = ::SelectObject(dc, font);
    const int oldMode = ::SetBkMode(dc, TRANSPARENT);
    const COLORREF oldColor = ::SetTextColor(dc, style.color);
    CRect r = box;
    ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, kDrawFlags);
    ::SetTextColor(dc, oldColor);
    ::SetBkMode(dc, oldMode);
    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

/** @brief 폭 width 안에서 줄바꿈했을 때 글자가 차지하는 높이 */
int MeasureHeight(const TextStyle& style, const std::wstring& text, int width)
{
    HDC dc = ::CreateCompatibleDC(nullptr);
    HFONT font = CreateTextFont(style);
    HGDIOBJ oldFont = ::SelectObject(dc, font);
    CRect r(0, 0, width, 0);
    const std::wstring measured = text.empty() ? std::wstring(1, kCaret) : text;
    ::DrawTextW(dc, measured.c_str(), static_cast<int>(measured.size()), &r, kDrawFlags | DT_CALCRECT);
    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
    ::DeleteDC(dc);
    return r.Height();
}
} // namespace

TextStyle TextStyle::From(const ToolSettings& settings)
{
    TextStyle style;
    style.face = settings.textFace;
    style.pointSize = settings.textPointSize;
    style.bold = settings.textBold;
    style.italic = settings.textItalic;
    style.underline = settings.textUnderline;
    style.opaque = settings.textOpaque;
    style.color = settings.color1;
    style.background = settings.color2;
    return style;
}

void TextTool::OnDown(ToolContext& context, CPoint point, MouseButton /*button*/)
{
    if (m_editing && m_box.PtInRect(point))
    {
        return; // 입력 중인 상자 안 클릭은 무시합니다.
    }
    Commit(context);
    m_style = TextStyle::From(context.settings);
    m_dragging = true;
    m_anchor = point;
    m_box = CRect(point, CSize(1, 1));
    MarkDirty();
}

void TextTool::OnMove(ToolContext& /*context*/, CPoint point)
{
    if (!m_dragging)
    {
        return;
    }
    MarkDirty();
    m_box = CRect((std::min)(m_anchor.x, point.x), (std::min)(m_anchor.y, point.y),
                  (std::max)(m_anchor.x, point.x) + 1, (std::max)(m_anchor.y, point.y) + 1);
    MarkDirty();
}

void TextTool::OnUp(ToolContext& context, CPoint point)
{
    if (!m_dragging)
    {
        return;
    }
    OnMove(context, point);
    m_dragging = false;
    if (m_box.Width() < kMinBoxWidth)
    {
        m_box.right = m_box.left + kMinBoxWidth; // 클릭만 해도 글자를 넣을 수 있게
    }
    m_editing = true;
    m_text.clear();
    m_composing.clear();
    FitHeight();
    MarkDirty();
}

void TextTool::Cancel()
{
    if (m_dragging)
    {
        MarkDirty();
        Reset();
    }
}

void TextTool::Commit(ToolContext& context)
{
    if (m_editing && !m_text.empty())
    {
        if (auto command = Rasterize(context.document.GetSurface(), m_text))
        {
            context.document.Apply(std::move(command));
        }
    }
    MarkDirty();
    Reset();
}

std::unique_ptr<Command> TextTool::Rasterize(const Surface& document, const std::wstring& text) const
{
    // 상자 영역의 현재 문서 픽셀 위에 글자를 그려 결과 픽셀을 만듭니다.
    CRect clip;
    if (!clip.IntersectRect(m_box, document.Bounds()))
    {
        return nullptr;
    }
    PixelBlock block = document.ReadBlock(clip);
    Surface scratch;
    if (!scratch.Create(clip.Width(), clip.Height(), RGB(255, 255, 255)))
    {
        return nullptr;
    }
    scratch.WritePixels(CPoint(0, 0), clip.Width(), clip.Height(), block.pixels.data());
    CRect local = m_box;
    local.OffsetRect(-clip.left, -clip.top);
    RenderText(scratch.DC(), local, m_style, text);
    PixelBlock result = scratch.ReadBlock(scratch.Bounds());
    return std::make_unique<PatchCommand>(CRect(), 0, clip.TopLeft(), clip.Size(),
                                          std::make_shared<const std::vector<Pixel>>(std::move(result.pixels)));
}

std::unique_ptr<Command> TextTool::MakeSharedPreview(const Surface& document) const
{
    // 보는 사람에게는 커서(|) 없이 입력 중인 글자와 조합 중인 글자를 보여 줍니다.
    if (!m_editing || (m_text.empty() && m_composing.empty()))
    {
        return nullptr;
    }
    return Rasterize(document, m_text + m_composing);
}

void TextTool::Reset()
{
    m_dragging = false;
    m_editing = false;
    m_text.clear();
    m_composing.clear();
    m_box.SetRectEmpty();
}

void TextTool::DrawPreview(Surface& surface) const
{
    if (m_editing)
    {
        RenderText(surface.DC(), m_box, m_style, m_text + m_composing + kCaret);
    }
}

void TextTool::OnChar(ToolContext& /*context*/, wchar_t ch)
{
    if (!m_editing)
    {
        return;
    }
    switch (ch)
    {
    case L'\b':
        if (m_text.size() >= 2 && m_text.compare(m_text.size() - 2, 2, L"\r\n") == 0)
        {
            m_text.resize(m_text.size() - 2);
        }
        else if (!m_text.empty())
        {
            m_text.pop_back();
        }
        break;
    case L'\r':
        m_text += L"\r\n";
        break;
    case L'\t':
        m_text += ch;
        break;
    default:
        if (ch >= 0x20)
        {
            m_text += ch;
        }
        break;
    }
    FitHeight();
    MarkDirty();
}

void TextTool::OnComposition(ToolContext& /*context*/, const std::wstring& composing)
{
    if (!m_editing)
    {
        return;
    }
    m_composing = composing;
    FitHeight();
    MarkDirty();
}

void TextTool::InsertText(ToolContext& context, const std::wstring& text)
{
    for (const wchar_t ch : text)
    {
        if (ch != L'\n') // 줄바꿈은 \r 하나로 받습니다.
        {
            OnChar(context, ch);
        }
    }
}

void TextTool::OnSettingsChanged(ToolContext& context)
{
    if (m_editing)
    {
        m_style = TextStyle::From(context.settings);
        FitHeight();
        MarkDirty();
    }
}

void TextTool::FitHeight()
{
    const int needed = MeasureHeight(m_style, m_text + m_composing + kCaret, m_box.Width()) + 2;
    if (m_box.Height() < needed)
    {
        MarkDirty();
        m_box.bottom = m_box.top + needed;
    }
}

void TextTool::MarkDirty()
{
    if (!m_box.IsRectEmpty())
    {
        CRect r = m_box;
        r.InflateRect(2, 2);
        m_dirty |= r;
    }
}

CRect TextTool::TakePreviewDirty()
{
    const CRect dirty = m_dirty;
    m_dirty.SetRectEmpty();
    return dirty;
}

HCURSOR TextTool::CursorAt(CPoint point) const
{
    return ::LoadCursor(nullptr, (m_editing && m_box.PtInRect(point)) ? IDC_IBEAM : IDC_CROSS);
}
