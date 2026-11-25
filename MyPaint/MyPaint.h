#pragma once

/**
 * @file MyPaint.h
 * @brief 애플리케이션 클래스 선언
 */

#ifndef __AFXWIN_H__
#error "PCH에 대해 이 파일을 포함하기 전에 'pch.h'를 포함합니다."
#endif

#include "Resource.h"

/**
 * @brief MyPaint 애플리케이션. 시작하면 주 대화 상자를 띄웁니다.
 */
class CMyPaintApp : public CWinApp
{
public:
    /**
     * @brief 생성자
     */
    CMyPaintApp();

    /**
     * @brief 애플리케이션 초기화. 주 대화 상자를 모달로 띄우고, 닫히면 종료합니다.
     * @return 메시지 펌프를 시작하지 않으므로 항상 FALSE
     */
    BOOL InitInstance() override;

    DECLARE_MESSAGE_MAP()
};

extern CMyPaintApp theApp;
