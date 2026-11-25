/**
 * @file ImageFile.cpp
 * @brief 그림 파일 읽기/쓰기 구현
 */
#include "pch.h"
#include "UI/ImageFile.h"

#include <atlimage.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

namespace ImageFile
{
const wchar_t* const kOpenFilter = L"모든 그림 파일|*.png;*.jpg;*.jpeg;*.bmp;*.gif|"
                                   L"PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|"
                                   L"BMP (*.bmp)|*.bmp|GIF (*.gif)|*.gif|모든 파일 (*.*)|*.*||";

const wchar_t* const kSaveFilter = L"PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg)|*.jpg;*.jpeg|"
                                   L"BMP (*.bmp)|*.bmp|GIF (*.gif)|*.gif||";

const wchar_t* ExtensionForSaveFilter(int filterIndex)
{
    switch (filterIndex)
    {
    case 2: return L".jpg";
    case 3: return L".bmp";
    case 4: return L".gif";
    default: return L".png";
    }
}

bool IsSupportedExtension(const CString& path)
{
    CString ext = ::PathFindExtensionW(path);
    ext.MakeLower();
    return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" || ext == L".gif";
}

bool Load(const CString& path, Surface& surface, CString& error)
{
    CImage image;
    if (FAILED(image.Load(path)))
    {
        error.Format(L"그림 파일을 열 수 없습니다.\n%s", path.GetString());
        return false;
    }
    Surface loaded;
    if (!loaded.Create(image.GetWidth(), image.GetHeight(), RGB(255, 255, 255)))
    {
        error.Format(L"그림이 너무 커서 열 수 없습니다. (%d \u00D7 %d)", image.GetWidth(), image.GetHeight());
        return false;
    }
    // 알파 채널이 있는 그림은 Draw가 AlphaBlend로 흰 바탕 위에 합성합니다.
    image.Draw(loaded.DC(), 0, 0);

    Pixel* bits = loaded.Bits();
    const size_t count = static_cast<size_t>(loaded.Width()) * loaded.Height();
    for (size_t i = 0; i < count; ++i)
    {
        bits[i] &= 0x00FFFFFF; // 알파 바이트는 쓰지 않으므로 0으로 맞춥니다.
    }
    surface = std::move(loaded);
    return true;
}

bool Save(const CString& path, const Surface& surface, CString& error)
{
    if (!IsSupportedExtension(path))
    {
        error = L"지원하지 않는 파일 형식입니다. (.png, .jpg, .bmp, .gif)";
        return false;
    }
    CImage image;
    if (!image.Create(surface.Width(), surface.Height(), 24))
    {
        error = L"저장할 그림을 만들 수 없습니다.";
        return false;
    }
    HDC dc = image.GetDC();
    ::BitBlt(dc, 0, 0, surface.Width(), surface.Height(), surface.DC(), 0, 0, SRCCOPY);
    image.ReleaseDC();

    // 형식은 확장자로 정해집니다.
    if (FAILED(image.Save(path)))
    {
        error.Format(L"파일을 저장할 수 없습니다.\n%s", path.GetString());
        return false;
    }
    return true;
}
} // namespace ImageFile
