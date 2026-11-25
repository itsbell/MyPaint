/**
 * @file ShareDialogs.cpp
 * @brief 실시간 공유 시작/참가 대화 상자 구현
 */
#include "pch.h"
#include "UI/ShareDialogs.h"

#include <ws2tcpip.h>

#include "Resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

namespace
{
constexpr UINT_PTR kPollTimer = 1;
constexpr UINT kPollIntervalMs = 100;

/** @brief 방 찾기를 시작한 뒤 "찾는 중"으로 보여 줄 시간 */
constexpr DWORD kSearchWindowMs = 1500;

std::wstring GetText(CWnd& dialog, int id)
{
    CString text;
    dialog.GetDlgItemText(id, text);
    return text.GetString();
}

/** @brief 포트 입력 확인 (1~65535). 잘못되면 메시지를 보여 주고 false */
bool ReadPort(CWnd& dialog, int id, uint16_t& port)
{
    BOOL ok = FALSE;
    const UINT value = dialog.GetDlgItemInt(id, &ok, FALSE);
    if (!ok || value == 0 || value > 65535)
    {
        AfxMessageBox(L"포트는 1부터 65535 사이의 숫자로 입력하세요.", MB_ICONWARNING);
        dialog.GetDlgItem(id)->SetFocus();
        return false;
    }
    port = static_cast<uint16_t>(value);
    return true;
}

/** @brief 닉네임 확인. 비어 있으면 메시지를 보여 주고 false */
bool ReadName(CWnd& dialog, int id, std::wstring& name)
{
    name = Share::CleanText(GetText(dialog, id), Share::kMaxNameLength);
    if (name.empty())
    {
        AfxMessageBox(L"닉네임을 입력하세요.", MB_ICONWARNING);
        dialog.GetDlgItem(id)->SetFocus();
        return false;
    }
    return true;
}
} // namespace

std::wstring DefaultShareName()
{
    wchar_t name[256] = {};
    DWORD size = static_cast<DWORD>(std::size(name));
    if (::GetUserNameW(name, &size))
    {
        return Share::CleanText(name, Share::kMaxNameLength);
    }
    return L"방장";
}

// ===========================================================================
// 공유 시작
// ===========================================================================

CShareStartDlg::CShareStartDlg(CWnd* parent) : CDialog(IDD_SHARE_START, parent)
{
}

BOOL CShareStartDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (m_name.empty())
    {
        m_name = DefaultShareName();
    }
    if (m_room.empty())
    {
        m_room = m_name + L"의 그림";
    }
    SetDlgItemText(IDC_SHARE_NAME, m_name.c_str());
    SetDlgItemText(IDC_SHARE_ROOM, m_room.c_str());
    SetDlgItemInt(IDC_SHARE_PORT, m_port, FALSE);
    static_cast<CEdit*>(GetDlgItem(IDC_SHARE_NAME))->SetLimitText(Share::kMaxNameLength);
    static_cast<CEdit*>(GetDlgItem(IDC_SHARE_ROOM))->SetLimitText(Share::kMaxRoomLength);

    // 직접 입력해서 참가할 사람을 위해 이 PC의 주소를 보여 줍니다.
    std::wstring addresses;
    for (const std::wstring& address : Share::LocalAddresses())
    {
        addresses += (addresses.empty() ? L"" : L", ") + address;
    }
    CString info;
    info.Format(L"내 주소: %s\n\n같은 네트워크의 사람들은 [공유 참가]의 방 목록에서 바로 찾을 수 있습니다.",
                addresses.empty() ? L"(알 수 없음)" : addresses.c_str());
    SetDlgItemText(IDC_SHARE_ADDRESSES, info);
    return TRUE;
}

void CShareStartDlg::OnOK()
{
    std::wstring name;
    uint16_t port = 0;
    if (!ReadName(*this, IDC_SHARE_NAME, name) || !ReadPort(*this, IDC_SHARE_PORT, port))
    {
        return;
    }
    std::wstring room = Share::CleanText(GetText(*this, IDC_SHARE_ROOM), Share::kMaxRoomLength);
    if (room.empty())
    {
        room = name + L"의 그림";
    }
    m_name = name;
    m_room = room;
    m_port = port;
    CDialog::OnOK();
}

// ===========================================================================
// 공유 참가
// ===========================================================================

BEGIN_MESSAGE_MAP(CShareJoinDlg, CDialog)
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_BN_CLICKED(IDC_JOIN_REFRESH, &CShareJoinDlg::OnRefresh)
    ON_NOTIFY(LVN_ITEMCHANGED, IDC_JOIN_LIST, &CShareJoinDlg::OnListItemChanged)
    ON_NOTIFY(NM_DBLCLK, IDC_JOIN_LIST, &CShareJoinDlg::OnListDoubleClick)
END_MESSAGE_MAP()

CShareJoinDlg::CShareJoinDlg(CWnd* parent) : CDialog(IDD_SHARE_JOIN, parent)
{
}

CShareJoinDlg::~CShareJoinDlg()
{
    if (m_socket != INVALID_SOCKET)
    {
        ::closesocket(m_socket);
    }
}

BOOL CShareJoinDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    if (m_name.empty())
    {
        m_name = DefaultShareName();
    }
    SetDlgItemText(IDC_JOIN_NAME, m_name.c_str());
    static_cast<CEdit*>(GetDlgItem(IDC_JOIN_NAME))->SetLimitText(Share::kMaxNameLength);
    SetDlgItemText(IDC_JOIN_ADDRESS, m_address.c_str());
    SetDlgItemInt(IDC_JOIN_PORT, m_port, FALSE);

    m_list.SubclassDlgItem(IDC_JOIN_LIST, this);
    m_list.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    CRect r;
    m_list.GetClientRect(&r);
    const int w = r.Width();
    m_list.InsertColumn(0, L"방 이름", LVCFMT_LEFT, w * 40 / 100);
    m_list.InsertColumn(1, L"방장", LVCFMT_LEFT, w * 22 / 100);
    m_list.InsertColumn(2, L"인원", LVCFMT_LEFT, w * 12 / 100);
    m_list.InsertColumn(3, L"주소", LVCFMT_LEFT, w - w * 74 / 100);

    // 방 찾기용 UDP 소켓 (방송 허용, 비동기)
    m_socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket != INVALID_SOCKET)
    {
        BOOL broadcast = TRUE;
        ::setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast),
                     sizeof(broadcast));
        u_long nonBlocking = 1;
        ::ioctlsocket(m_socket, FIONBIO, &nonBlocking);
    }
    StartSearch();
    SetTimer(kPollTimer, kPollIntervalMs, nullptr);
    return TRUE;
}

void CShareJoinDlg::OnDestroy()
{
    KillTimer(kPollTimer);
    CDialog::OnDestroy();
}

void CShareJoinDlg::StartSearch()
{
    m_rooms.clear();
    m_list.DeleteAllItems();
    m_searchStarted = ::GetTickCount();
    if (m_socket != INVALID_SOCKET)
    {
        const std::vector<uint8_t> query = Share::MakeDiscoveryQuery();
        sockaddr_in to{};
        to.sin_family = AF_INET;
        to.sin_port = htons(Share::kDiscoveryPort);
        // 같은 네트워크 전체와, 같은 PC(시연용)에 보냅니다.
        const ULONG targets[] = {static_cast<ULONG>(INADDR_BROADCAST), static_cast<ULONG>(INADDR_LOOPBACK)};
        for (const ULONG address : targets)
        {
            to.sin_addr.s_addr = htonl(address);
            ::sendto(m_socket, reinterpret_cast<const char*>(query.data()), static_cast<int>(query.size()), 0,
                     reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        }
    }
    UpdateStatus();
}

void CShareJoinDlg::ReceiveReplies()
{
    if (m_socket == INVALID_SOCKET)
    {
        return;
    }
    uint8_t buffer[1024];
    while (true)
    {
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int received = ::recvfrom(m_socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                                        reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (received == SOCKET_ERROR)
        {
            break;
        }
        Share::RoomInfo info;
        if (!Share::ParseDiscoveryReply(buffer, static_cast<size_t>(received), info))
        {
            continue;
        }
        wchar_t text[64] = {};
        ::InetNtopW(AF_INET, &from.sin_addr, text, std::size(text));
        info.address = text;

        // 같은 PC의 방은 방송과 127.0.0.1로 두 번 응답할 수 있으므로 하나만 남깁니다.
        bool duplicate = false;
        for (const Share::RoomInfo& room : m_rooms)
        {
            duplicate = duplicate || (room.room == info.room && room.host == info.host && room.port == info.port);
        }
        if (duplicate)
        {
            continue;
        }
        m_rooms.push_back(info);
        const int row = m_list.InsertItem(m_list.GetItemCount(), info.room.c_str());
        m_list.SetItemText(row, 1, info.host.c_str());
        CString count;
        count.Format(L"%d/%d", info.viewers, info.maxViewers);
        m_list.SetItemText(row, 2, count);
        CString address;
        address.Format(L"%s:%u", info.address.c_str(), info.port);
        m_list.SetItemText(row, 3, address);
        m_list.SetItemData(row, m_rooms.size() - 1);
        if (m_rooms.size() == 1)
        {
            m_list.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
    }
}

void CShareJoinDlg::UpdateStatus()
{
    CString status;
    if (::GetTickCount() - m_searchStarted < kSearchWindowMs)
    {
        status = L"방을 찾는 중...";
    }
    else if (m_rooms.empty())
    {
        status = L"찾은 방이 없습니다. 주소를 직접 입력하세요.";
    }
    else
    {
        status.Format(L"방 %d개를 찾았습니다.", static_cast<int>(m_rooms.size()));
    }
    CString current;
    GetDlgItemText(IDC_JOIN_STATUS, current);
    if (current != status)
    {
        SetDlgItemText(IDC_JOIN_STATUS, status);
    }
}

void CShareJoinDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == kPollTimer)
    {
        ReceiveReplies();
        UpdateStatus();
        return;
    }
    CDialog::OnTimer(nIDEvent);
}

void CShareJoinDlg::OnRefresh()
{
    StartSearch();
}

void CShareJoinDlg::OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    const auto* info = reinterpret_cast<const NMLISTVIEW*>(pNMHDR);
    if ((info->uChanged & LVIF_STATE) == 0 || (info->uNewState & LVIS_SELECTED) == 0)
    {
        return;
    }
    // 목록에서 고른 방의 주소를 직접 입력 칸에 채워, 참가는 항상 입력 칸 값으로 합니다.
    const size_t index = m_list.GetItemData(info->iItem);
    if (index < m_rooms.size())
    {
        SetDlgItemText(IDC_JOIN_ADDRESS, m_rooms[index].address.c_str());
        SetDlgItemInt(IDC_JOIN_PORT, m_rooms[index].port, FALSE);
    }
}

void CShareJoinDlg::OnListDoubleClick(NMHDR* /*pNMHDR*/, LRESULT* pResult)
{
    *pResult = 0;
    if (m_list.GetSelectedCount() > 0)
    {
        OnOK();
    }
}

void CShareJoinDlg::OnOK()
{
    std::wstring name;
    uint16_t port = 0;
    if (!ReadName(*this, IDC_JOIN_NAME, name))
    {
        return;
    }
    const std::wstring address = Share::CleanText(GetText(*this, IDC_JOIN_ADDRESS), 255);
    if (address.empty())
    {
        AfxMessageBox(L"참가할 방을 목록에서 고르거나 주소를 입력하세요.", MB_ICONWARNING);
        GetDlgItem(IDC_JOIN_ADDRESS)->SetFocus();
        return;
    }
    if (!ReadPort(*this, IDC_JOIN_PORT, port))
    {
        return;
    }
    m_name = name;
    m_address = address;
    m_port = port;
    CDialog::OnOK();
}
