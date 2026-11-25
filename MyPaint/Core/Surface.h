#pragma once

/**
 * @file Surface.h
 * @brief 픽셀에 직접 접근할 수 있는 32bpp 비트맵
 */

#include <cstdint>
#include <vector>

/** @brief 픽셀 값. 메모리 순서 B, G, R, X (정수로는 0x00RRGGBB) */
using Pixel = uint32_t;

/** @brief COLORREF → Pixel */
inline Pixel ToPixel(COLORREF color)
{
    return (static_cast<Pixel>(GetRValue(color)) << 16) | (static_cast<Pixel>(GetGValue(color)) << 8) |
           static_cast<Pixel>(GetBValue(color));
}

/** @brief Pixel → COLORREF */
inline COLORREF ToColorRef(Pixel pixel)
{
    return RGB((pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF);
}

/**
 * @brief 사각 영역의 픽셀 사본 (실행 취소용 백업 등)
 */
struct PixelBlock
{
    CRect rect;                ///< 원본 표면에서의 위치
    std::vector<Pixel> pixels; ///< rect.Width() * rect.Height() 개, 위에서 아래로

    bool IsEmpty() const { return rect.IsRectEmpty() != FALSE; }
    size_t Bytes() const { return pixels.size() * sizeof(Pixel); }
};

/**
 * @brief 32bpp top-down DIB Section과 이를 선택한 메모리 DC
 *
 * GDI로 그릴 때는 DC()를, 픽셀을 직접 다룰 때는 Bits()를 씁니다.
 * Bits()와 블록 함수는 GdiFlush()를 먼저 호출하므로 GDI 결과와 섞어 써도 안전합니다.
 */
class Surface
{
public:
    Surface() = default;
    ~Surface();

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    Surface(Surface&& other) noexcept;
    Surface& operator=(Surface&& other) noexcept;

    /**
     * @brief 표면을 만들고 한 색으로 채웁니다. 기존 내용은 사라집니다.
     * @return 성공 여부 (크기가 0 이하이거나 너무 크면 실패)
     */
    bool Create(int width, int height, COLORREF fill);

    /** @brief 자원 해제 */
    void Destroy();

    bool IsValid() const { return m_bitmap != nullptr; }
    int Width() const { return m_width; }
    int Height() const { return m_height; }
    CSize Size() const { return CSize(m_width, m_height); }
    CRect Bounds() const { return CRect(0, 0, m_width, m_height); }

    /** @brief 이 표면이 선택된 메모리 DC */
    HDC DC() const { return m_dc; }

    /** @brief 픽셀 배열 (행 간격 = Width()) */
    Pixel* Bits();
    const Pixel* Bits() const;

    /** @brief 영역 밖이면 0을 돌려줍니다. */
    Pixel GetPixel(int x, int y) const;

    /** @brief 표면 전체를 한 색으로 채웁니다. */
    void Fill(COLORREF color);

    /** @brief rect(표면 범위로 잘림)의 픽셀을 복사해 돌려줍니다. */
    PixelBlock ReadBlock(const CRect& rect) const;

    /** @brief ReadBlock으로 얻은 블록을 같은 위치에 되씁니다. */
    void WriteBlock(const PixelBlock& block);

    /** @brief 같은 크기의 다른 표면에서 rect 영역을 복사해 옵니다. */
    void CopyFrom(const Surface& source, const CRect& rect);

    /** @brief rect(표면 범위로 잘림)를 한 색으로 채웁니다. */
    void FillRect(const CRect& rect, COLORREF color);

    /**
     * @brief width × height 픽셀 배열을 dest 위치에 씁니다. 표면 밖으로 나가는 부분은 버립니다.
     */
    void WritePixels(CPoint dest, int width, int height, const Pixel* pixels);

private:
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_oldBitmap = nullptr;
    Pixel* m_bits = nullptr;
    int m_width = 0;
    int m_height = 0;
};
