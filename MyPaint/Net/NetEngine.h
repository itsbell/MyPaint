#pragma once

/**
 * @file NetEngine.h
 * @brief Winsock 소켓 입출력을 별도 스레드에서 처리하는 엔진
 *
 * 프로토콜 내용은 모르고 "길이 + 내용" 프레임만 주고받습니다.
 * UI 스레드는 Send()로 보낼 프레임을 넣고, 알림 메시지를 받으면 TakeEvents()로 받은 일을 꺼내 처리합니다.
 */

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/** @brief 보낼 프레임 (앞에 길이 4바이트가 붙은 상태). 여러 연결이 같은 버퍼를 함께 씁니다. */
using NetFrame = std::shared_ptr<const std::vector<uint8_t>>;

/** @brief 네트워크 스레드가 UI 스레드에 넘기는 일 */
struct NetEvent
{
    enum class Kind
    {
        Connected,     ///< (참가) 방장에게 연결됨
        ConnectFailed, ///< (참가) 연결 실패
        Accepted,      ///< (방장) 새 연결이 들어옴
        Received,      ///< 프레임 하나를 받음 (payload)
        Closed,        ///< 연결이 끊김
    };

    Kind kind = Kind::Closed;
    uint32_t connection = 0;
    std::vector<uint8_t> payload;
};

/**
 * @brief 소켓 스레드
 *
 * - 방장: TCP 접속 대기 + 여러 연결 + UDP 방 찾기 응답
 * - 참가자: TCP 연결 하나
 */
class NetEngine
{
public:
    /** @brief 프레임 하나의 최대 크기 (손상된 길이로 큰 메모리를 잡지 않도록) */
    static constexpr uint32_t kMaxFrameBytes = 256u * 1024 * 1024;

    /** @brief 한 연결에 쌓아 둘 수 있는 보낼 데이터 최대량. 넘으면 너무 느린 연결로 보고 끊습니다. */
    static constexpr size_t kMaxQueuedBytes = 256u * 1024 * 1024;

    NetEngine() = default;
    ~NetEngine();
    NetEngine(const NetEngine&) = delete;
    NetEngine& operator=(const NetEngine&) = delete;

    /**
     * @brief 방장으로 시작합니다. 포트를 여는 데 실패하면 error에 이유를 넣고 false를 돌려줍니다.
     * @param notify 받은 일이 생기면 message를 보낼 창
     */
    bool StartServer(uint16_t port, uint16_t discoveryPort, HWND notify, UINT message, std::wstring& error);

    /** @brief 참가자로 시작합니다. 연결 결과는 Connected/ConnectFailed 이벤트로 옵니다. */
    void StartClient(const std::wstring& host, uint16_t port, HWND notify, UINT message);

    /**
     * @brief 스레드를 멈추고 소켓을 닫습니다.
     * @param flushMs 남은 프레임을 보내기 위해 기다릴 최대 시간 (밀리초)
     */
    void Stop(DWORD flushMs = 0);

    bool IsRunning() const { return m_thread.joinable(); }

    /** @brief 프레임을 보냅니다. (참가자는 connection = 1) */
    void Send(uint32_t connection, const NetFrame& frame);

    /** @brief 남은 프레임을 다 보낸 뒤 연결을 닫습니다. */
    void Close(uint32_t connection);

    /** @brief 방 찾기 요청에 돌려줄 응답 (방장) */
    void SetDiscoveryReply(std::vector<uint8_t> reply);

    /** @brief 쌓인 이벤트를 꺼냅니다. */
    std::vector<NetEvent> TakeEvents();

    /** @brief 내용 앞에 길이를 붙여 프레임을 만듭니다. */
    static NetFrame MakeFrame(const std::vector<uint8_t>& payload);

private:
    struct Connection
    {
        SOCKET socket = INVALID_SOCKET;
        std::vector<uint8_t> in;  ///< 받았지만 아직 프레임으로 자르지 못한 바이트
        std::deque<NetFrame> out; ///< 보낼 프레임
        size_t outOffset = 0;     ///< out.front()에서 이미 보낸 바이트
        size_t queuedBytes = 0;
        bool closing = false;     ///< 다 보내면 닫기
    };

    void Launch(HWND notify, UINT message);
    void Run();
    bool ConnectClient();
    void Loop();
    void Cleanup();

    /** @brief 읽을 수 있는 연결에서 받고 프레임을 이벤트로 만듭니다. 끊기면 false */
    bool ReceiveFrom(uint32_t id, Connection& connection);

    /** @brief 보낼 수 있는 만큼 보냅니다. 오류면 false */
    bool SendPending(Connection& connection);

    void AcceptPending();
    void AnswerDiscovery();
    void CloseLocked(uint32_t id, bool notify);
    void PushEvent(NetEvent&& event);

    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<DWORD> m_flushDeadline{0};

    bool m_isServer = false;
    std::wstring m_host;
    uint16_t m_port = 0;
    SOCKET m_listen = INVALID_SOCKET;
    SOCKET m_udp = INVALID_SOCKET;

    std::mutex m_mutex; ///< m_connections, m_discoveryReply 보호
    std::map<uint32_t, Connection> m_connections;
    std::vector<uint8_t> m_discoveryReply;
    uint32_t m_nextId = 1;

    std::mutex m_eventMutex; ///< m_events, m_notified 보호
    std::vector<NetEvent> m_events;
    bool m_notified = false;
    HWND m_notify = nullptr;
    UINT m_message = 0;
};
