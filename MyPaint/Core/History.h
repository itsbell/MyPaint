#pragma once

/**
 * @file History.h
 * @brief 실행 취소/다시 실행 기록
 */

#include <cstdint>
#include <memory>
#include <vector>

#include "Core/Command.h"
#include "Core/Surface.h"

/**
 * @brief 기록 한 칸: 실행한 명령과, 실행 직전 영향 영역의 픽셀
 */
struct HistoryEntry
{
    uint64_t id = 0;                  ///< 기록마다 고유한 번호 (저장 시점 비교용)
    std::unique_ptr<Command> command; ///< 다시 실행할 때 쓰는 명령
    PixelBlock before;                ///< 실행 취소할 때 되돌릴 픽셀

    size_t Bytes() const { return before.Bytes(); }
};

/**
 * @brief 선형 기록. 앞쪽 [0, position)이 적용된 상태, 뒤쪽은 다시 실행 가능한 기록입니다.
 *
 * 개수나 메모리 한도를 넘으면 가장 오래된 기록부터 버립니다.
 */
class History
{
public:
    /** @brief 새 기록을 추가합니다. 다시 실행 기록은 버립니다. */
    void Push(HistoryEntry&& entry);

    bool CanUndo() const { return m_position > 0; }
    bool CanRedo() const { return m_position < m_entries.size(); }

    /** @brief 위치를 한 칸 뒤로 옮기고, 되돌릴 기록을 돌려줍니다. CanUndo()일 때만 호출하세요. */
    const HistoryEntry& StepBack();

    /** @brief 위치를 한 칸 앞으로 옮기고, 다시 적용할 기록을 돌려줍니다. CanRedo()일 때만 호출하세요. */
    const HistoryEntry& StepForward();

    /** @brief 현재 상태를 만든 마지막 기록의 id (없으면 0) */
    uint64_t CurrentId() const { return m_position > 0 ? m_entries[m_position - 1].id : 0; }

    /** @brief 모두 지웁니다. */
    void Clear();

private:
    void TrimToLimits();

    std::vector<HistoryEntry> m_entries;
    size_t m_position = 0;
    size_t m_totalBytes = 0;
};
