#pragma once

/**
 * @file ShareDialogs.h
 * @brief 실시간 공유 시작/참가 대화 상자
 */

#include <winsock2.h>

#include <string>
#include <vector>

#include "Net/Protocol.h"

/**
 * @brief 공유 시작: 닉네임, 방 이름, 포트
 */
class CShareStartDlg : public CDialog
{
public:
    explicit CShareStartDlg(CWnd* parent = nullptr);

    std::wstring m_name;
    std::wstring m_room;
    uint16_t m_port = Share::kDefaultPort;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
};

/**
 * @brief 공유 참가: 닉네임, 같은 네트워크의 방 목록(UDP 방송으로 찾기), 주소 직접 입력
 */
class CShareJoinDlg : public CDialog
{
public:
    explicit CShareJoinDlg(CWnd* parent = nullptr);
    ~CShareJoinDlg() override;

    std::wstring m_name;
    std::wstring m_address;
    uint16_t m_port = Share::kDefaultPort;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnRefresh();
    afx_msg void OnDestroy();
    afx_msg void OnListItemChanged(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnListDoubleClick(NMHDR* pNMHDR, LRESULT* pResult);
    DECLARE_MESSAGE_MAP()

private:
    /** @brief 방 찾기 요청을 보내고 목록을 비웁니다. */
    void StartSearch();

    /** @brief 도착한 응답을 목록에 더합니다. */
    void ReceiveReplies();

    void UpdateStatus();

    SOCKET m_socket = INVALID_SOCKET;
    std::vector<Share::RoomInfo> m_rooms;
    DWORD m_searchStarted = 0;
    CListCtrl m_list;
};

/** @brief 기본 닉네임 (Windows 사용자 이름) */
std::wstring DefaultShareName();
