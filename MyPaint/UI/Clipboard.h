#pragma once

/**
 * @file Clipboard.h
 * @brief 클립보드에 그림/글자를 넣고 꺼내는 도우미
 */

#include <string>
#include <vector>

#include "Core/Surface.h"

namespace Clipboard
{
/** @brief 그림을 24비트 DIB(CF_DIB)로 넣습니다. 대부분의 프로그램이 읽을 수 있는 형식입니다. */
bool PutImage(HWND owner, CSize size, const std::vector<Pixel>& pixels);

/** @brief 클립보드에 그림이 있는지 */
bool HasImage();

/**
 * @brief 클립보드의 그림을 꺼냅니다.
 *
 * 시스템이 어떤 형식(DIB, DIBV5, BITMAP)이든 CF_BITMAP으로 바꿔 주므로 그것을 읽어 32비트로 옮깁니다.
 */
bool GetImage(HWND owner, CSize& size, std::vector<Pixel>& pixels);

/** @brief 클립보드의 글자(CF_UNICODETEXT)를 꺼냅니다. */
bool GetText(HWND owner, std::wstring& text);
} // namespace Clipboard
