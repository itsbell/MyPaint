#pragma once

/**
 * @file Tool.h
 * @brief 그리기 도구 인터페이스와 공용 설정
 */

#include <array>

#include "Core/ShapeCommand.h"

class Command;
class Document;

/** @brief 도구 종류 */
enum class ToolId
{
    Pencil,
    Fill,
    Eraser,
    Picker,
    Magnifier,
    Brush,
    Shape,
    Count,
};

/** @brief 누른 마우스 버튼 */
enum class MouseButton
{
    Left,  ///< 색 1로 그림
    Right, ///< 색 2로 그림
};

/** @brief 두께 단계를 픽셀로 바꿀 때 쓰는 도구 분류 */
enum class WidthKind
{
    Pencil, ///< 1, 2, 3, 4
    Brush,  ///< 1, 3, 5, 8 (도형도 같음)
    Eraser, ///< 4, 6, 8, 10
};

/** @brief 두께 단계 수 */
constexpr int kSizeLevels = 4;

/** @brief 사용자 지정 색 칸 수 */
constexpr int kCustomColorCount = 10;

/**
 * @brief 모든 도구가 함께 쓰는 설정. 주 대화 상자가 소유하고 도구 패널이 바꿉니다.
 */
struct ToolSettings
{
    COLORREF color1 = RGB(0, 0, 0);       ///< 색 1 (왼쪽 버튼)
    COLORREF color2 = RGB(255, 255, 255); ///< 색 2 (오른쪽 버튼)
    int activeColorSlot = 0;              ///< 팔레트를 눌렀을 때 바뀌는 칸 (0: 색 1, 1: 색 2)
    int sizeLevel = 0;                    ///< 두께 단계 (0 ~ kSizeLevels-1)
    ShapeKind shapeKind = ShapeKind::Rectangle;
    bool shapeOutline = true; ///< 도형 윤곽선
    bool shapeFill = false;   ///< 도형 채우기

    /** @brief 사용자 지정 색. 아직 정하지 않은 칸은 CLR_INVALID */
    std::array<COLORREF, kCustomColorCount> customColors = MakeEmptyCustomColors();

    COLORREF ColorFor(MouseButton button) const { return button == MouseButton::Left ? color1 : color2; }
    COLORREF OtherColor(MouseButton button) const { return button == MouseButton::Left ? color2 : color1; }

    /** @brief 현재 두께 단계의 픽셀 값 */
    int WidthFor(WidthKind kind) const { return WidthAt(kind, sizeLevel); }

    /** @brief 단계별 픽셀 값 */
    static int WidthAt(WidthKind kind, int level)
    {
        static constexpr int kPencil[kSizeLevels] = {1, 2, 3, 4};
        static constexpr int kBrush[kSizeLevels] = {1, 3, 5, 8};
        static constexpr int kEraser[kSizeLevels] = {4, 6, 8, 10};
        const int i = level < 0 ? 0 : (level >= kSizeLevels ? kSizeLevels - 1 : level);
        switch (kind)
        {
        case WidthKind::Pencil: return kPencil[i];
        case WidthKind::Eraser: return kEraser[i];
        default: return kBrush[i];
        }
    }

private:
    static std::array<COLORREF, kCustomColorCount> MakeEmptyCustomColors()
    {
        std::array<COLORREF, kCustomColorCount> colors{};
        colors.fill(CLR_INVALID);
        return colors;
    }
};

/**
 * @brief 도구가 문서 밖의 상태를 바꿀 때 부르는 쪽 (주 대화 상자)
 */
class IToolHost
{
public:
    virtual ~IToolHost() = default;

    /** @brief 색 1 또는 색 2를 바꿉니다. (스포이트) */
    virtual void SetToolColor(MouseButton button, COLORREF color) = 0;

    /** @brief 바로 전에 쓰던 도구로 돌아갑니다. (스포이트) */
    virtual void RestorePreviousTool() = 0;

    /** @brief 문서 좌표 docPoint를 기준으로 한 단계 확대/축소합니다. (돋보기) */
    virtual void ZoomAt(CPoint docPoint, bool zoomIn) = 0;
};

/**
 * @brief 도구가 동작할 때 필요한 것들
 */
struct ToolContext
{
    Document& document;
    const ToolSettings& settings;
    IToolHost& host;
    bool shift = false; ///< Shift를 누르고 있는지 (도형 비율 고정 등)
};

/**
 * @brief 그리기 도구
 *
 * 캔버스가 마우스 입력을 문서 좌표로 바꿔 넘겨 줍니다.
 * 도구는 드래그하는 동안 미리보기 명령을 보여 주고, 끝나면 명령을 만들어 Document::Apply()에 넘깁니다.
 */
class Tool
{
public:
    virtual ~Tool() = default;

    virtual void OnDown(ToolContext& context, CPoint point, MouseButton button) = 0;
    virtual void OnMove(ToolContext& context, CPoint point) = 0;
    virtual void OnUp(ToolContext& context, CPoint point) = 0;

    /** @brief 진행 중인 작업을 버립니다. (Esc, 마우스 캡처 상실 등) */
    virtual void Cancel() {}

    /** @brief 드래그 등 진행 중인 작업이 있는지 */
    virtual bool IsActive() const { return false; }

    /** @brief 진행 중인 작업의 미리보기. 캔버스가 문서 사본 위에 실행해서 보여 줍니다. */
    virtual const Command* PreviewCommand() const { return nullptr; }

    /** @brief 지난번 호출 뒤 미리보기가 바뀐 영역(문서 좌표)을 돌려주고 비웁니다. */
    virtual CRect TakePreviewDirty() { return CRect(); }

    /** @brief 캔버스 위에서 보일 커서 */
    virtual HCURSOR Cursor() const { return ::LoadCursor(nullptr, IDC_CROSS); }
};
