#pragma once

/**
 * @file ImageFile.h
 * @brief 그림 파일 읽기/쓰기 (BMP, PNG, JPEG, GIF)
 *
 * MFC/ATL의 CImage(내부적으로 Windows 기본 GDI+)를 쓰므로 별도 라이브러리가 필요 없습니다.
 */

#include "Core/Surface.h"

namespace ImageFile
{
/**
 * @brief 그림 파일을 읽습니다. 투명한 부분은 흰 바탕 위에 합성합니다.
 * @param error 실패했을 때 사용자에게 보여 줄 설명
 */
bool Load(const CString& path, Surface& surface, CString& error);

/**
 * @brief 확장자에 맞는 형식으로 저장합니다. (.png .jpg .jpeg .bmp .gif)
 */
bool Save(const CString& path, const Surface& surface, CString& error);

/** @brief 지원하는 확장자인지 */
bool IsSupportedExtension(const CString& path);

/** @brief 열기 대화 상자용 필터 */
extern const wchar_t* const kOpenFilter;

/** @brief 저장 대화 상자용 필터 (순서: PNG, JPEG, BMP, GIF) */
extern const wchar_t* const kSaveFilter;

/** @brief 저장 필터 번호(1부터)에 해당하는 기본 확장자 */
const wchar_t* ExtensionForSaveFilter(int filterIndex);
} // namespace ImageFile
