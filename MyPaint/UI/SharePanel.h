#pragma once

/**
 * @file SharePanel.h
 * @brief 오른쪽 공유 창: 참가자 목록과 채팅
 */

#include <functional>
#include <string>
#include <vector>

/**
 * @brief 참가자 목록 + 채팅 기록 + 입력 칸
 */
class CSharePanel : public CWnd
{
public:
    /** @brief 채팅 보내기를 눌렀을 때 불릴 함수 */
    using SendHandler = std::function<void(const std::wstring&)>;

    BOOL Create(CWnd* parent, UINT id);

    void SetSendHandler(SendHandler handler) { m_onSend = std::move(handler); }

    /** @brief 참가자 목록 (첫 번째가 방장). myName은 "(나)"를 붙일 이름 */
    void SetParticipants(const std::vector<std::wstring>& names, const std::wstring& myName);

    /** @brief 채팅 기록에 한 줄을 더합니다. */
    void AppendChat(const std::wstring& sender, const std::wstring& text, bool notice);

    /** @brief 채팅 기록과 참가자 목록을 비웁니다. */
    void Clear();

    /** @brief 입력 칸과 보내기 버튼을 쓸 수 있는지 */
    void SetInputEnabled(bool enabled);

    /** @brief 키 입력을 처리했으면 true (입력 칸에서 Enter) */
    bool HandleKey(const MSG* msg);

    /** @brief 포커스가 이 창의 입력 칸이나 기록에 있는지 (단축키를 가로채지 않도록) */
    bool HasFocusInside() const;

    /** @brief 입력 칸에 포커스를 줍니다. */
    void FocusInput();

    /** @brief 권장 폭 (현재 DPI 기준 픽셀) */
    int PreferredWidth() const;

protected:
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSend();
    DECLARE_MESSAGE_MAP()

private:
    void Layout();
    void SendCurrent();
    int Scale(int value) const;

    CStatic m_participantsLabel;
    CListBox m_participants;
    CStatic m_chatLabel;
    CEdit m_log;
    CEdit m_input;
    CButton m_send;
    CFont m_font;
    SendHandler m_onSend;
};
