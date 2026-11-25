#pragma once

/**
 * @file ShareSession.h
 * @brief 실시간 공유: 방장(ShareHost)과 보는 사람(ShareViewer)
 *
 * 둘 다 UI 스레드에서만 쓰입니다. 소켓 입출력은 NetEngine 스레드가 하고,
 * 알림 메시지를 받으면 OnNetMessage()가 받은 프레임을 처리합니다.
 *
 * 방장은 문서에 적용된 명령을 그대로 보내고, 실행 취소/다시 실행처럼 명령 없이 바뀐 부분은
 * 바뀐 픽셀(PatchCommand)이나 그림 전체(PNG)로 보냅니다. 보는 쪽은 받은 명령을 기록 없이 적용합니다.
 */

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Core/Document.h"
#include "Core/ImageCommands.h"
#include "Net/NetEngine.h"
#include "Net/Protocol.h"

class Tool;

/** @brief 공유가 끝난 이유 */
enum class ShareEndReason
{
    Stopped,        ///< 내가 끝냄
    HostEnded,      ///< (보는 쪽) 방장이 공유를 끝냄
    ConnectionLost, ///< (보는 쪽) 연결이 끊김
    ConnectFailed,  ///< (보는 쪽) 연결하지 못함
    Rejected,       ///< (보는 쪽) 방장이 거절함 (방이 가득 참 등)
};

/**
 * @brief 공유 상태 변화를 받는 쪽 (주 대화 상자)
 */
class IShareObserver
{
public:
    virtual ~IShareObserver() = default;

    /** @brief 참가자 목록이 바뀌었습니다. 첫 번째가 방장입니다. */
    virtual void OnShareParticipants(const std::vector<std::wstring>& names) = 0;

    /** @brief 채팅 글. notice면 "OO 님이 들어왔습니다" 같은 알림입니다. */
    virtual void OnShareChat(const std::wstring& sender, const std::wstring& text, bool notice) = 0;

    /** @brief (보는 쪽) 방에 들어갔습니다. */
    virtual void OnViewerJoined(const std::wstring& /*room*/, const std::wstring& /*host*/,
                                const std::wstring& /*myName*/)
    {
    }

    /** @brief (보는 쪽) 방장의 미리보기가 바뀌었습니다. preview가 nullptr이면 없음 */
    virtual void OnViewerPreview(std::unique_ptr<Command> /*preview*/, const CRect& /*overlay*/) {}

    /** @brief (보는 쪽) 방장 커서 */
    virtual void OnViewerCursor(bool /*visible*/, CPoint /*docPoint*/) {}

    /** @brief (보는 쪽) 공유가 끝났습니다. detail은 거절 이유 등 */
    virtual void OnShareEnded(ShareEndReason /*reason*/, const std::wstring& /*detail*/) {}
};

/**
 * @brief 방장 쪽
 */
class ShareHost : public IDocumentListener
{
public:
    ShareHost(Document& document, IShareObserver& observer);
    ~ShareHost() override;
    ShareHost(const ShareHost&) = delete;
    ShareHost& operator=(const ShareHost&) = delete;

    /** @brief 공유를 시작합니다. 실패하면 error에 이유가 들어갑니다. */
    bool Start(const std::wstring& hostName, const std::wstring& room, uint16_t port, HWND notify, UINT message,
               std::wstring& error);

    /** @brief 보는 사람들에게 끝 인사를 보내고 공유를 끝냅니다. */
    void Stop();

    bool IsActive() const { return m_engine.IsRunning(); }

    /** @brief 네트워크 알림 메시지를 받았을 때 */
    void OnNetMessage();

    /** @brief 도구 미리보기가 바뀌었음 (다음 Tick에 보냄) */
    void MarkPreviewDirty() { m_previewDirty = true; }

    /** @brief 커서 위치가 바뀜 (다음 Tick에 보냄) */
    void SetCursor(bool visible, CPoint docPoint);

    /** @brief 타이머마다 불러 미리보기와 커서를 보냅니다. tool은 현재 도구 */
    void Tick(const Tool* tool);

    /** @brief 방장이 채팅을 보냅니다. */
    void SendChat(const std::wstring& text);

    int ViewerCount() const;
    uint16_t Port() const { return m_port; }
    const std::wstring& Room() const { return m_room; }
    const std::wstring& HostName() const { return m_hostName; }

    // IDocumentListener
    void OnDocumentChanged(const CRect& /*dirty*/) override {}
    void OnCommandApplied(const Command& command) override;
    void OnDocumentRestored(const CRect& dirty, bool whole) override;

private:
    struct Peer
    {
        std::wstring name;
        bool joined = false;
    };

    void HandleFrame(uint32_t id, const std::vector<uint8_t>& payload);
    void HandleHello(uint32_t id, ByteReader& reader);
    void HandleClosed(uint32_t id);

    void SendTo(uint32_t id, const std::vector<uint8_t>& payload);
    void SendToViewers(const std::vector<uint8_t>& payload);
    void BroadcastChat(const std::wstring& sender, const std::wstring& text, bool notice);
    void BroadcastParticipants();
    std::vector<uint8_t> MakeSnapshotMessage() const;
    std::wstring UniqueName(const std::wstring& wanted) const;
    void UpdateDiscovery();
    std::vector<std::wstring> Names() const;

    Document& m_document;
    IShareObserver& m_observer;
    NetEngine m_engine;
    std::map<uint32_t, Peer> m_peers;
    std::wstring m_hostName;
    std::wstring m_room;
    uint16_t m_port = Share::kDefaultPort;

    bool m_previewDirty = false;
    std::vector<uint8_t> m_lastPreview;        ///< 마지막으로 보낸 미리보기 (같으면 다시 보내지 않음)
    PatchCommand::PixelData m_lastPatchPixels; ///< 마지막으로 보낸 미리보기 픽셀 (선택 영역 이동 때 재사용)

    bool m_cursorDirty = false;
    bool m_cursorVisible = false;
    CPoint m_cursor;
};

/**
 * @brief 보는 쪽
 */
class ShareViewer
{
public:
    ShareViewer(Document& document, IShareObserver& observer);
    ShareViewer(const ShareViewer&) = delete;
    ShareViewer& operator=(const ShareViewer&) = delete;

    /** @brief 방장에게 연결합니다. 결과는 관찰자로 옵니다. */
    void Start(const std::wstring& address, uint16_t port, const std::wstring& name, HWND notify, UINT message);

    /** @brief 연결을 끊습니다. (관찰자에게 알리지 않음) */
    void Stop();

    bool IsActive() const { return m_engine.IsRunning(); }

    /** @brief 방에 들어가 있는지 (환영 메시지를 받은 뒤) */
    bool IsJoined() const { return m_joined; }

    void OnNetMessage();
    void SendChat(const std::wstring& text);

    const std::wstring& Room() const { return m_room; }
    const std::wstring& HostName() const { return m_hostName; }

private:
    void HandleFrame(const std::vector<uint8_t>& payload);
    void HandlePreview(ByteReader& reader);
    void End(ShareEndReason reason, const std::wstring& detail);

    Document& m_document;
    IShareObserver& m_observer;
    NetEngine m_engine;
    std::wstring m_name;
    std::wstring m_room;
    std::wstring m_hostName;
    bool m_joined = false;
    bool m_ended = false;
    PatchCommand::PixelData m_lastPatchPixels; ///< 마지막으로 받은 미리보기 픽셀
};
