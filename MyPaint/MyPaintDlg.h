#pragma once

/**
 * @file MyPaintDlg.h
 * @brief 주 대화 상자 클래스 선언
 */

/**
 * @brief 주 대화 상자 (과제 템플릿). 화면 구성과 동작은 지원자가 설계합니다.
 */
class CMyPaintDlg : public CDialog
{
public:
    /**
     * @brief 생성자
     * @param pParent 부모 창 (없으면 nullptr)
     */
    explicit CMyPaintDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MYPAINT_DIALOG };
#endif

protected:
    /**
     * @brief 컨트롤과 멤버 변수 사이의 데이터 교환
     * @param pDX 데이터 교환 개체
     */
    void DoDataExchange(CDataExchange* pDX) override;

    /**
     * @brief 대화 상자 초기화
     * @return 포커스를 컨트롤에 직접 지정하지 않으면 TRUE
     */
    BOOL OnInitDialog() override;

    DECLARE_MESSAGE_MAP()

private:
    HICON m_hIcon = nullptr; ///< 창 아이콘
};
