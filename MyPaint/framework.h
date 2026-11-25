#pragma once

/**
 * @file framework.h
 * @brief MFC 공통 헤더
 */

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN // 거의 쓰지 않는 Windows 헤더 제외
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS // 일부 CString 생성자를 explicit으로

// 무시해도 되는 MFC 경고 메시지 일부를 숨기지 않음
#define _AFX_ALL_WARNINGS

#include <afxwin.h>  // MFC 핵심 및 표준 구성 요소
#include <afxext.h>  // MFC 확장

#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>  // Windows 공용 컨트롤
#endif

// 공용 컨트롤 6.0 (비주얼 스타일)
#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
