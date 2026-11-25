/**
 * @file Document.cpp
 * @brief Document 구현
 */
#include "pch.h"
#include "Core/Document.h"

#include <algorithm>

bool Document::New(int width, int height, COLORREF background)
{
    Surface surface;
    if (!surface.Create(width, height, background))
    {
        return false;
    }
    m_surface = std::move(surface);
    m_history.Clear();
    m_savedId = m_history.CurrentId();
    NotifyReset();
    return true;
}

void Document::Apply(std::unique_ptr<Command> command)
{
    if (!command || !m_surface.IsValid())
    {
        return;
    }

    HistoryEntry entry;
    entry.before = m_surface.ReadBlock(command->ComputeBounds(m_surface));
    if (entry.before.IsEmpty())
    {
        return; // 캔버스 밖에만 그린 명령은 기록하지 않습니다.
    }

    command->Execute(m_surface);
    const CRect dirty = entry.before.rect;
    const Command& applied = *command;

    entry.id = m_nextId++;
    entry.command = std::move(command);
    m_history.Push(std::move(entry));

    NotifyChanged(dirty);
    for (IDocumentListener* listener : m_listeners)
    {
        listener->OnCommandApplied(applied);
    }
}

bool Document::Undo()
{
    if (!m_history.CanUndo())
    {
        return false;
    }
    const HistoryEntry& entry = m_history.StepBack();
    m_surface.WriteBlock(entry.before);
    NotifyChanged(entry.before.rect);
    return true;
}

bool Document::Redo()
{
    if (!m_history.CanRedo())
    {
        return false;
    }
    const HistoryEntry& entry = m_history.StepForward();
    entry.command->Execute(m_surface);
    NotifyChanged(entry.before.rect);
    return true;
}

void Document::AddListener(IDocumentListener* listener)
{
    if (listener != nullptr && std::find(m_listeners.begin(), m_listeners.end(), listener) == m_listeners.end())
    {
        m_listeners.push_back(listener);
    }
}

void Document::RemoveListener(IDocumentListener* listener)
{
    m_listeners.erase(std::remove(m_listeners.begin(), m_listeners.end(), listener), m_listeners.end());
}

void Document::NotifyChanged(const CRect& dirty)
{
    for (IDocumentListener* listener : m_listeners)
    {
        listener->OnDocumentChanged(dirty);
    }
}

void Document::NotifyReset()
{
    for (IDocumentListener* listener : m_listeners)
    {
        listener->OnDocumentReset();
    }
}
