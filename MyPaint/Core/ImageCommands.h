#pragma once

/**
 * @file ImageCommands.h
 * @brief 픽셀 단위 편집 명령: 붙여넣기/선택 이동, 색 반전, 회전/대칭, 크기 조정/기울이기, 캔버스 크기
 */

#include <memory>
#include <vector>

#include "Core/Command.h"
#include "Core/Surface.h"

/**
 * @brief 픽셀 조각을 붙이는 명령
 *
 * 선택 영역 이동(원래 자리를 배경색으로 지우고 새 자리에 붙임), 붙여넣기, 텍스트 확정, 선택 영역 삭제에 씁니다.
 * 결과 픽셀을 그대로 담으므로 글꼴 같은 환경 차이와 상관없이 어디서나 같은 결과가 나옵니다.
 */
class PatchCommand : public Command
{
public:
    using PixelData = std::shared_ptr<const std::vector<Pixel>>;

    /** @brief 지우기만 하는 명령 (선택 영역 삭제) */
    static std::unique_ptr<PatchCommand> MakeClear(const CRect& rect, COLORREF color);

    /**
     * @param clearRect 먼저 color로 채울 영역 (비어 있으면 지우지 않음)
     * @param dest 픽셀을 붙일 왼쪽 위 위치
     * @param size 픽셀 조각 크기
     * @param pixels size.cx * size.cy 개 (없으면 붙이지 않음)
     */
    PatchCommand(const CRect& clearRect, COLORREF clearColor, CPoint dest, CSize size, PixelData pixels);

    CommandType Type() const override { return CommandType::Patch; }
    CRect Bounds() const override;
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<PatchCommand> Read(ByteReader& reader);

private:
    CRect m_clearRect;
    COLORREF m_clearColor;
    CPoint m_dest;
    CSize m_size;
    PixelData m_pixels;
};

/**
 * @brief 영역의 색을 반전합니다. (두 번 하면 원래대로)
 */
class InvertCommand : public Command
{
public:
    explicit InvertCommand(const CRect& rect) : m_rect(rect) {}

    CommandType Type() const override { return CommandType::Invert; }
    CRect Bounds() const override { return m_rect; }
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<InvertCommand> Read(ByteReader& reader);

private:
    CRect m_rect;
};

/** @brief 이미지 전체 회전/대칭 종류. 직렬화에 쓰이므로 순서를 바꾸지 마세요. */
enum class TransformKind : uint8_t
{
    RotateRight, ///< 시계 방향 90°
    RotateLeft,  ///< 반시계 방향 90°
    Rotate180,
    FlipHorizontal, ///< 좌우 대칭
    FlipVertical,   ///< 상하 대칭
    Count,
};

/**
 * @brief 이미지 전체를 회전하거나 뒤집습니다.
 */
class TransformCommand : public Command
{
public:
    explicit TransformCommand(TransformKind kind) : m_kind(kind) {}

    CommandType Type() const override { return CommandType::Transform; }
    CRect Bounds() const override { return CRect(); }
    CRect ComputeBounds(const Surface& surface) const override { return surface.Bounds(); }
    bool ChangesSize() const override { return true; }
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<TransformCommand> Read(ByteReader& reader);

private:
    TransformKind m_kind;
};

/**
 * @brief 그림판의 [크기 조정 및 기울이기]
 *
 * 먼저 새 크기로 늘이거나 줄이고, 그다음 가로/세로 기울이기를 적용합니다.
 * 기울이면서 생기는 빈 곳은 배경색으로 채웁니다.
 */
class ResizeSkewCommand : public Command
{
public:
    ResizeSkewCommand(CSize newSize, int skewHorizontalDegrees, int skewVerticalDegrees, COLORREF background);

    /** @brief 이 명령을 적용한 뒤의 캔버스 크기 */
    CSize ResultSize() const;

    CommandType Type() const override { return CommandType::ResizeSkew; }
    CRect Bounds() const override { return CRect(); }
    CRect ComputeBounds(const Surface& surface) const override { return surface.Bounds(); }
    bool ChangesSize() const override { return true; }
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<ResizeSkewCommand> Read(ByteReader& reader);

private:
    CSize m_newSize;
    int m_skewH;
    int m_skewV;
    COLORREF m_background;
};

/**
 * @brief 캔버스 크기를 바꿉니다. 기존 그림은 offset 위치에 두고 나머지는 배경색으로 채웁니다.
 *
 * 이미지 속성의 크기 변경(offset = 0), 선택 영역으로 자르기(offset = -선택 위치),
 * 큰 그림을 붙여넣을 때 캔버스 늘리기에 씁니다.
 */
class CanvasSizeCommand : public Command
{
public:
    CanvasSizeCommand(CSize newSize, CPoint offset, COLORREF background)
        : m_newSize(newSize), m_offset(offset), m_background(background)
    {
    }

    CommandType Type() const override { return CommandType::CanvasSize; }
    CRect Bounds() const override { return CRect(); }
    CRect ComputeBounds(const Surface& surface) const override { return surface.Bounds(); }
    bool ChangesSize() const override { return true; }
    void Execute(Surface& surface) const override;
    void Write(ByteWriter& writer) const override;

    static std::unique_ptr<CanvasSizeCommand> Read(ByteReader& reader);

private:
    CSize m_newSize;
    CPoint m_offset;
    COLORREF m_background;
};

/** @brief 캔버스 한 변의 최대 크기 */
constexpr int kMaxCanvasSide = 20000;
