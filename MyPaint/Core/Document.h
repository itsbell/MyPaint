#pragma once

/**
 * @file Document.h
 * @brief 그림 문서. 픽셀을 바꾸는 유일한 통로입니다.
 */

#include <memory>
#include <vector>

#include "Core/Command.h"
#include "Core/History.h"
#include "Core/Surface.h"

/**
 * @brief 문서 변경 알림을 받는 쪽 (캔버스, 메인 창, 나중에는 공유 송신부)
 */
class IDocumentListener
{
public:
    virtual ~IDocumentListener() = default;

    /** @brief 픽셀이 바뀌었습니다. dirty는 문서 좌표입니다. */
    virtual void OnDocumentChanged(const CRect& dirty) = 0;

    /** @brief 새 문서가 되었거나 캔버스 크기가 바뀌었습니다. */
    virtual void OnDocumentReset() {}

    /** @brief 새 명령이 적용되었습니다. (실행 취소/다시 실행은 포함하지 않음) */
    virtual void OnCommandApplied(const Command& /*command*/) {}
};

/**
 * @brief 그림 문서
 *
 * 모든 편집은 Apply()로 들어오고, 기록과 알림이 함께 처리됩니다.
 * 표면은 읽기 전용으로만 밖에 공개합니다.
 */
class Document
{
public:
    /** @brief 한 색으로 채운 새 문서를 만듭니다. 기록은 지워집니다. */
    bool New(int width, int height, COLORREF background = RGB(255, 255, 255));

    const Surface& GetSurface() const { return m_surface; }
    CSize Size() const { return m_surface.Size(); }

    /** @brief 명령을 적용하고 기록합니다. */
    void Apply(std::unique_ptr<Command> command);

    bool CanUndo() const { return m_history.CanUndo(); }
    bool CanRedo() const { return m_history.CanRedo(); }
    bool Undo();
    bool Redo();

    /** @brief 마지막으로 저장한 뒤 바뀌었는지 */
    bool IsModified() const { return m_history.CurrentId() != m_savedId; }

    /** @brief 지금 상태를 저장된 상태로 표시합니다. */
    void MarkSaved() { m_savedId = m_history.CurrentId(); }

    void AddListener(IDocumentListener* listener);
    void RemoveListener(IDocumentListener* listener);

private:
    void NotifyChanged(const CRect& dirty);
    void NotifyReset();

    Surface m_surface;
    History m_history;
    uint64_t m_nextId = 1;
    uint64_t m_savedId = 0;
    std::vector<IDocumentListener*> m_listeners;
};
