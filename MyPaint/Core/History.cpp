/**
 * @file History.cpp
 * @brief History 구현
 */
#include "pch.h"
#include "Core/History.h"

namespace
{
constexpr size_t kMaxEntries = 100;
constexpr size_t kMaxBytes = 512u * 1024 * 1024;
} // namespace

void History::Push(HistoryEntry&& entry)
{
    for (size_t i = m_position; i < m_entries.size(); ++i)
    {
        m_totalBytes -= m_entries[i].Bytes();
    }
    m_entries.erase(m_entries.begin() + static_cast<ptrdiff_t>(m_position), m_entries.end());

    m_totalBytes += entry.Bytes();
    m_entries.push_back(std::move(entry));
    m_position = m_entries.size();
    TrimToLimits();
}

const HistoryEntry& History::StepBack()
{
    ASSERT(CanUndo());
    return m_entries[--m_position];
}

const HistoryEntry& History::StepForward()
{
    ASSERT(CanRedo());
    return m_entries[m_position++];
}

void History::Clear()
{
    m_entries.clear();
    m_position = 0;
    m_totalBytes = 0;
}

void History::TrimToLimits()
{
    // 방금 넣은 기록 하나는 한도를 넘어도 남겨 둡니다.
    size_t drop = 0;
    size_t bytes = m_totalBytes;
    while (m_entries.size() - drop > 1 && (m_entries.size() - drop > kMaxEntries || bytes > kMaxBytes))
    {
        bytes -= m_entries[drop].Bytes();
        ++drop;
    }
    if (drop > 0)
    {
        m_entries.erase(m_entries.begin(), m_entries.begin() + static_cast<ptrdiff_t>(drop));
        m_position -= drop;
        m_totalBytes = bytes;
    }
}
