#pragma once

/**
 * @file Command.h
 * @brief 문서를 바꾸는 모든 편집 작업의 기반 클래스
 */

#include <cstdint>
#include <memory>

class Surface;
class ByteWriter;
class ByteReader;

/** @brief 직렬화할 때 앞에 붙는 명령 종류 번호. 값을 바꾸지 마세요. */
enum class CommandType : uint16_t
{
    Stroke = 1,
    Fill = 2,
    Shape = 3,
    Patch = 4,
    Invert = 5,
    Transform = 6,
    ResizeSkew = 7,
    CanvasSize = 8,
};

/**
 * @brief 편집 명령
 *
 * 같은 표면에 같은 명령을 실행하면 항상 같은 결과가 나와야 합니다.
 * 이 성질 덕분에 다시 실행, 리플레이, 원격 공유가 모두 같은 명령으로 동작합니다.
 */
class Command
{
public:
    virtual ~Command() = default;

    /** @brief 명령 종류 */
    virtual CommandType Type() const = 0;

    /** @brief 이 명령이 바꿀 수 있는 영역 (문서 좌표, 표면 범위로 자르기 전) */
    virtual CRect Bounds() const = 0;

    /**
     * @brief 실제 표면을 보고 영향 영역을 계산합니다. (실행 취소 백업 범위)
     *
     * 채우기처럼 표면 내용에 따라 영역이 정해지는 명령만 재정의합니다.
     */
    virtual CRect ComputeBounds(const Surface& /*surface*/) const { return Bounds(); }

    /**
     * @brief 캔버스 크기를 바꿀 수 있는 명령인지 (회전, 크기 조정, 자르기 등)
     *
     * true이면 Document가 실행 전에 이미지 전체를 백업하고, Execute는 표면을 새로 만들어 바꿔 끼웁니다.
     */
    virtual bool ChangesSize() const { return false; }

    /** @brief 표면에 명령을 적용합니다. DC에 클리핑이 걸려 있으면 그 안에만 그려도 됩니다. */
    virtual void Execute(Surface& surface) const = 0;

    /** @brief 종류 번호를 뺀 본문을 씁니다. */
    virtual void Write(ByteWriter& writer) const = 0;
};

/** @brief 종류 번호와 본문을 함께 씁니다. */
void WriteCommand(ByteWriter& writer, const Command& command);

/**
 * @brief WriteCommand로 쓴 명령을 읽습니다.
 * @throw std::runtime_error 알 수 없는 종류이거나 데이터가 손상된 경우
 */
std::unique_ptr<Command> ReadCommand(ByteReader& reader);
