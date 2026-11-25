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
    Reset(std::move(surface));
    return true;
}

void Document::Reset(Surface&& surface)
{
    m_surface = std::move(surface);
    m_history.Clear();
    m_savedId = m_history.CurrentId();
    NotifyReset();
    NotifyRestored(m_surface.Bounds(), true);
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
    const bool changesSize = command->ChangesSize();

    entry.id = m_nextId++;
    entry.command = std::move(command);
    m_history.Push(std::move(entry));

    if (changesSize)
    {
        NotifyReset();
    }
    else
    {
        NotifyChanged(dirty);
    }
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
    if (entry.command->ChangesSize())
    {
        // 크기를 바꾸는 명령은 이미지 전체를 백업해 두었으므로 원래 크기의 표면을 다시 만듭니다.
        Surface restored;
        if (!restored.Create(entry.before.rect.Width(), entry.before.rect.Height(), RGB(255, 255, 255)))
        {
            return false;
        }
        restored.WriteBlock(entry.before);
        m_surface = std::move(restored);
        NotifyReset();
        NotifyRestored(m_surface.Bounds(), true);
        return true;
    }
    m_surface.WriteBlock(entry.before);
    NotifyChanged(entry.before.rect);
    NotifyRestored(entry.before.rect, false);
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
    if (entry.command->ChangesSize())
    {
        NotifyReset();
    }
    else
    {
        NotifyChanged(entry.before.rect);
    }
    const bool whole = entry.command->ChangesSize();
    NotifyRestored(whole ? m_surface.Bounds() : entry.before.rect, whole);
    return true;
}

void Document::ApplyUnrecorded(const Command& command)
{
    if (!m_surface.IsValid())
    {
        return;
    }
    CRect dirty = command.ComputeBounds(m_surface);
    command.Execute(m_surface);
    if (command.ChangesSize())
    {
        NotifyReset();
        return;
    }
    dirty &= m_surface.Bounds();
    if (!dirty.IsRectEmpty())
    {
        NotifyChanged(dirty);
    }
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

void Document::NotifyRestored(const CRect& dirty, bool whole)
{
    for (IDocumentListener* listener : m_listeners)
    {
        listener->OnDocumentRestored(dirty, whole);
    }
}

void Document::NotifyReset()
{
    for (IDocumentListener* listener : m_listeners)
    {
        listener->OnDocumentReset();
    }
}
