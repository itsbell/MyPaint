/**
 * @file SharePanel.cpp
 * @brief CSharePanel 구현
 */
#include "pch.h"
#include "UI/SharePanel.h"

#include "Net/Protocol.h"
#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr COLORREF kBorder = RGB(218, 220, 224);

/** @brief 채팅 기록이 이 글자 수를 넘으면 앞부분을 지웁니다. */
constexpr int kMaxLogChars = 60000;
} // namespace

BEGIN_MESSAGE_MAP(CSharePanel, CWnd)
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_BN_CLICKED(IDC_CHAT_SEND, &CSharePanel::OnSend)
END_MESSAGE_MAP()

BOOL CSharePanel::Create(CWnd* parent, UINT id)
{
    const CString className = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    // WS_EX_CONTROLPARENT: 대화 상자가 Tab 키로 안쪽 컨트롤을 오갈 수 있게 합니다.
    if (!CWnd::CreateEx(WS_EX_CONTROLPARENT, className, nullptr, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                        CRect(0, 0, 0, 0), parent, id))
    {
        return FALSE;
    }

    LOGFONT lf{};
    lf.lfHeight = -Scale(12);
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Malgun Gothic");
    m_font.CreateFontIndirect(&lf);

    const CRect zero(0, 0, 0, 0);
    m_participantsLabel.Create(L"참가자", WS_CHILD | WS_VISIBLE | SS_LEFT, zero, this);
    m_participants.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOINTEGRALHEIGHT | LBS_NOSEL, zero,
                          this, IDC_CHAT_PARTICIPANTS);
    m_chatLabel.Create(L"채팅", WS_CHILD | WS_VISIBLE | SS_LEFT, zero, this);
    m_log.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, zero,
                 this, IDC_CHAT_LOG);
    m_input.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, zero, this, IDC_CHAT_INPUT);
    m_send.Create(L"보내기", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, zero, this, IDC_CHAT_SEND);
    m_input.SetLimitText(Share::kMaxChatLength);
    m_log.SetLimitText(0);
    m_input.SetCueBanner(L"메시지 입력");

    for (CWnd* child : std::initializer_list<CWnd*>{&m_participantsLabel, &m_participants, &m_chatLabel, &m_log,
                                                    &m_input, &m_send})
    {
        child->SetFont(&m_font);
    }
    return TRUE;
}

int CSharePanel::Scale(int value) const
{
    const UINT dpi = GetSafeHwnd() != nullptr ? ::GetDpiForWindow(m_hWnd)
                                              : (GetParent() != nullptr ? ::GetDpiForWindow(GetParent()->m_hWnd)
                                                                        : USER_DEFAULT_SCREEN_DPI);
    return ::MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

int CSharePanel::PreferredWidth() const
{
    return Scale(220);
}

void CSharePanel::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout();
}

void CSharePanel::Layout()
{
    if (m_send.GetSafeHwnd() == nullptr)
    {
        return;
    }
    CRect client;
    GetClientRect(&client);
    const int pad = Scale(8);
    const int left = pad + 1; // 왼쪽 경계선
    const int right = client.right - pad;
    const int labelHeight = Scale(18);
    const int rowHeight = Scale(26);
    int y = pad;

    m_participantsLabel.MoveWindow(left, y, right - left, labelHeight);
    y += labelHeight;
    const int listHeight = Scale(120);
    m_participants.MoveWindow(left, y, right - left, listHeight);
    y += listHeight + pad;

    m_chatLabel.MoveWindow(left, y, right - left, labelHeight);
    y += labelHeight;

    const int inputTop = client.bottom - pad - rowHeight;
    m_log.MoveWindow(left, y, right - left, (std::max)(Scale(40), inputTop - pad - y));
    const int buttonWidth = Scale(56);
    m_input.MoveWindow(left, inputTop, right - left - buttonWidth - Scale(4), rowHeight);
    m_send.MoveWindow(right - buttonWidth, inputTop, buttonWidth, rowHeight);
}

BOOL CSharePanel::OnEraseBkgnd(CDC* /*pDC*/)
{
    return TRUE;
}

void CSharePanel::OnPaint()
{
    CPaintDC dc(this);
    CRect client;
    GetClientRect(&client);
    dc.FillSolidRect(client, ::GetSysColor(COLOR_BTNFACE));
    dc.FillSolidRect(0, 0, 1, client.Height(), kBorder);
}

void CSharePanel::SetParticipants(const std::vector<std::wstring>& names, const std::wstring& myName)
{
    m_participants.ResetContent();
    for (size_t i = 0; i < names.size(); ++i)
    {
        std::wstring line = names[i];
        if (i == 0)
        {
            line += L" (방장)";
        }
        if (names[i] == myName)
        {
            line += L" (나)";
        }
        m_participants.AddString(line.c_str());
    }
    CString label;
    // 방장을 뺀 보는 사람 수 / 최대
    label.Format(L"참가자  (보는 사람 %d/%d)", names.empty() ? 0 : static_cast<int>(names.size()) - 1,
                 Share::kMaxViewers);
    m_participantsLabel.SetWindowText(label);
}

void CSharePanel::AppendChat(const std::wstring& sender, const std::wstring& text, bool notice)
{
    std::wstring line = notice ? L"· " + text : sender + L": " + text;
    if (m_log.GetWindowTextLength() > 0)
    {
        line = L"\r\n" + line;
    }
    if (m_log.GetWindowTextLength() > kMaxLogChars)
    {
        m_log.SetSel(0, kMaxLogChars / 2);
        m_log.ReplaceSel(L"");
    }
    const int end = m_log.GetWindowTextLength();
    m_log.SetSel(end, end);
    m_log.ReplaceSel(line.c_str());
    m_log.LineScroll(m_log.GetLineCount());
}

void CSharePanel::Clear()
{
    m_log.SetWindowText(L"");
    m_input.SetWindowText(L"");
    m_participants.ResetContent();
    m_participantsLabel.SetWindowText(L"참가자");
}

void CSharePanel::SetInputEnabled(bool enabled)
{
    m_input.EnableWindow(enabled ? TRUE : FALSE);
    m_send.EnableWindow(enabled ? TRUE : FALSE);
}

void CSharePanel::FocusInput()
{
    if (m_input.IsWindowEnabled())
    {
        m_input.SetFocus();
    }
}

bool CSharePanel::HasFocusInside() const
{
    const HWND focus = ::GetFocus();
    return focus != nullptr && (focus == m_input.GetSafeHwnd() || focus == m_log.GetSafeHwnd());
}

bool CSharePanel::HandleKey(const MSG* msg)
{
    if (msg->message == WM_KEYDOWN && msg->wParam == VK_RETURN && msg->hwnd == m_input.GetSafeHwnd())
    {
        SendCurrent();
        return true;
    }
    return false;
}

void CSharePanel::OnSend()
{
    SendCurrent();
    FocusInput();
}

void CSharePanel::SendCurrent()
{
    CString text;
    m_input.GetWindowText(text);
    const std::wstring clean = Share::CleanText(text.GetString(), Share::kMaxChatLength);
    if (clean.empty())
    {
        return;
    }
    m_input.SetWindowText(L"");
    if (m_onSend)
    {
        m_onSend(clean);
    }
}
