/**
 * @file MyPaint.cpp
 * @brief 애플리케이션 클래스 구현
 */
#include "pch.h"
#include "MyPaint.h"
#include "MyPaintDlg.h"

#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CMyPaintApp, CWinApp)
END_MESSAGE_MAP()

/** @brief 유일한 애플리케이션 개체 */
CMyPaintApp theApp;

CMyPaintApp::CMyPaintApp()
{
}

BOOL CMyPaintApp::InitInstance()
{
    // 공용 컨트롤 6.0(비주얼 스타일)을 쓰려면 InitCommonControlsEx가 필요합니다.
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    // 실시간 공유에 쓰는 Winsock을 준비합니다. 실패해도 그림판 기능은 그대로 씁니다.
    WSADATA wsa{};
    const bool winsock = ::WSAStartup(MAKEWORD(2, 2), &wsa) == 0;

    {
        CMyPaintDlg dlg;
        m_pMainWnd = &dlg;
        dlg.DoModal();
    }

    if (winsock)
    {
        ::WSACleanup();
    }

    // 대화 상자가 닫혔으므로 메시지 펌프를 시작하지 않고 종료합니다.
    return FALSE;
}
