/**
 * @file NetEngine.cpp
 * @brief NetEngine 구현
 */
#include "pch.h"
#include "Net/NetEngine.h"

#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace
{
/** @brief select 한 번에 기다리는 시간. 보낼 프레임이 새로 생겨도 이만큼 안에 처리됩니다. */
constexpr long kSelectTimeoutUs = 10 * 1000;

/** @brief 연결을 시도하는 최대 시간 */
constexpr DWORD kConnectTimeoutMs = 5000;

/** @brief 방 찾기 요청 패킷 최대 크기 */
constexpr int kMaxDatagram = 1024;

void SetNonBlocking(SOCKET s)
{
    u_long on = 1;
    ::ioctlsocket(s, FIONBIO, &on);
}

void SetNoDelay(SOCKET s)
{
    // 커서, 미리보기처럼 작은 프레임이 지연 없이 가도록 Nagle 알고리즘을 끕니다.
    BOOL on = TRUE;
    ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&on), sizeof(on));
}

void CloseSocket(SOCKET& s)
{
    if (s != INVALID_SOCKET)
    {
        ::closesocket(s);
        s = INVALID_SOCKET;
    }
}

std::wstring SocketErrorText(const wchar_t* what, int code)
{
    wchar_t* message = nullptr;
    ::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, static_cast<DWORD>(code), 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    std::wstring text = what;
    if (message != nullptr)
    {
        text += L"\n";
        text += message;
        ::LocalFree(message);
    }
    return text;
}
} // namespace

NetEngine::~NetEngine()
{
    Stop();
}

NetFrame NetEngine::MakeFrame(const std::vector<uint8_t>& payload)
{
    auto frame = std::make_shared<std::vector<uint8_t>>();
    frame->resize(sizeof(uint32_t) + payload.size());
    const auto length = static_cast<uint32_t>(payload.size());
    std::memcpy(frame->data(), &length, sizeof(length));
    if (!payload.empty())
    {
        std::memcpy(frame->data() + sizeof(length), payload.data(), payload.size());
    }
    return frame;
}

bool NetEngine::StartServer(uint16_t port, uint16_t discoveryPort, HWND notify, UINT message, std::wstring& error)
{
    Stop();
    m_isServer = true;

    m_listen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET)
    {
        error = SocketErrorText(L"소켓을 만들 수 없습니다.", ::WSAGetLastError());
        return false;
    }
    // 같은 포트를 다른 프로그램이 쓰고 있으면 실패하도록 독점 사용을 요청합니다.
    BOOL exclusive = TRUE;
    ::setsockopt(m_listen, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
                 sizeof(exclusive));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (::bind(m_listen, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        ::listen(m_listen, SOMAXCONN) == SOCKET_ERROR)
    {
        error = SocketErrorText(L"포트를 열 수 없습니다. 다른 프로그램이 쓰고 있거나 방화벽이 막고 있을 수 있습니다.",
                                ::WSAGetLastError());
        CloseSocket(m_listen);
        return false;
    }
    SetNonBlocking(m_listen);

    // 방 찾기: 같은 PC에서 여러 MyPaint가 함께 받을 수 있도록 주소 재사용을 허용합니다.
    // 실패해도 IP 직접 입력으로는 참가할 수 있으므로 공유는 계속합니다.
    m_udp = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_udp != INVALID_SOCKET)
    {
        BOOL reuse = TRUE;
        ::setsockopt(m_udp, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in udpAddress{};
        udpAddress.sin_family = AF_INET;
        udpAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        udpAddress.sin_port = htons(discoveryPort);
        if (::bind(m_udp, reinterpret_cast<const sockaddr*>(&udpAddress), sizeof(udpAddress)) == SOCKET_ERROR)
        {
            CloseSocket(m_udp);
        }
        else
        {
            SetNonBlocking(m_udp);
        }
    }

    Launch(notify, message);
    return true;
}

void NetEngine::StartClient(const std::wstring& host, uint16_t port, HWND notify, UINT message)
{
    Stop();
    m_isServer = false;
    m_host = host;
    m_port = port;
    Launch(notify, message);
}

void NetEngine::Launch(HWND notify, UINT message)
{
    m_notify = notify;
    m_message = message;
    m_stop = false;
    m_flushDeadline = 0;
    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_events.clear();
        m_notified = false;
    }
    m_thread = std::thread([this] { Run(); });
}

void NetEngine::Stop(DWORD flushMs)
{
    if (m_thread.joinable())
    {
        m_flushDeadline = ::GetTickCount() + flushMs;
        m_stop = true;
        m_thread.join();
    }
    Cleanup();
}

void NetEngine::Cleanup()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : m_connections)
    {
        CloseSocket(entry.second.socket);
    }
    m_connections.clear();
    CloseSocket(m_listen);
    CloseSocket(m_udp);
}

void NetEngine::Send(uint32_t connection, const NetFrame& frame)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_connections.find(connection);
    if (it == m_connections.end() || it->second.closing)
    {
        return;
    }
    Connection& c = it->second;
    c.out.push_back(frame);
    c.queuedBytes += frame->size();
    if (c.queuedBytes > kMaxQueuedBytes)
    {
        CloseLocked(connection, true); // 받는 쪽이 따라오지 못하면 연결을 끊습니다.
    }
}

void NetEngine::Close(uint32_t connection)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_connections.find(connection);
    if (it != m_connections.end())
    {
        it->second.closing = true;
    }
}

void NetEngine::SetDiscoveryReply(std::vector<uint8_t> reply)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_discoveryReply = std::move(reply);
}

std::vector<NetEvent> NetEngine::TakeEvents()
{
    std::lock_guard<std::mutex> lock(m_eventMutex);
    std::vector<NetEvent> events;
    events.swap(m_events);
    m_notified = false;
    return events;
}

void NetEngine::PushEvent(NetEvent&& event)
{
    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(std::move(event));
    // 알림 메시지가 쌓이지 않도록, UI가 아직 꺼내 가지 않았으면 다시 보내지 않습니다.
    if (!m_notified && m_notify != nullptr)
    {
        m_notified = true;
        ::PostMessage(m_notify, m_message, 0, 0);
    }
}

void NetEngine::CloseLocked(uint32_t id, bool notify)
{
    const auto it = m_connections.find(id);
    if (it == m_connections.end())
    {
        return;
    }
    CloseSocket(it->second.socket);
    m_connections.erase(it);
    if (notify)
    {
        NetEvent event;
        event.kind = NetEvent::Kind::Closed;
        event.connection = id;
        PushEvent(std::move(event));
    }
}

// ---------------------------------------------------------------------------
// 스레드
// ---------------------------------------------------------------------------

void NetEngine::Run()
{
    if (!m_isServer && !ConnectClient())
    {
        return;
    }
    Loop();
}

bool NetEngine::ConnectClient()
{
    auto fail = [this] {
        NetEvent event;
        event.kind = NetEvent::Kind::ConnectFailed;
        event.connection = 1;
        PushEvent(std::move(event));
        return false;
    };

    addrinfoW hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfoW* result = nullptr;
    const std::wstring service = std::to_wstring(m_port);
    if (::GetAddrInfoW(m_host.c_str(), service.c_str(), &hints, &result) != 0 || result == nullptr)
    {
        return fail();
    }
    SOCKET s = ::socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (s == INVALID_SOCKET)
    {
        ::FreeAddrInfoW(result);
        return fail();
    }
    SetNonBlocking(s);
    const int rc = ::connect(s, result->ai_addr, static_cast<int>(result->ai_addrlen));
    ::FreeAddrInfoW(result);
    if (rc == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK)
    {
        ::closesocket(s);
        return fail();
    }

    // 연결이 끝나기를 조금씩 기다리면서 Stop() 요청도 확인합니다.
    const DWORD start = ::GetTickCount();
    bool connected = false;
    while (!m_stop && ::GetTickCount() - start < kConnectTimeoutMs)
    {
        fd_set writable, failed;
        FD_ZERO(&writable);
        FD_ZERO(&failed);
        FD_SET(s, &writable);
        FD_SET(s, &failed);
        timeval timeout{0, 100 * 1000};
        const int ready = ::select(0, nullptr, &writable, &failed, &timeout);
        if (ready == SOCKET_ERROR || FD_ISSET(s, &failed))
        {
            break;
        }
        if (FD_ISSET(s, &writable))
        {
            connected = true;
            break;
        }
    }
    if (!connected)
    {
        ::closesocket(s);
        return m_stop ? false : fail();
    }
    SetNoDelay(s);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        Connection c;
        c.socket = s;
        m_connections.emplace(1, std::move(c));
    }
    NetEvent event;
    event.kind = NetEvent::Kind::Connected;
    event.connection = 1;
    PushEvent(std::move(event));
    return true;
}

void NetEngine::Loop()
{
    while (true)
    {
        if (m_stop)
        {
            // 멈출 때는 남은 프레임(끝 인사 등)을 정해진 시간까지만 보내 봅니다.
            std::lock_guard<std::mutex> lock(m_mutex);
            bool pending = false;
            for (const auto& entry : m_connections)
            {
                pending = pending || !entry.second.out.empty();
            }
            if (!pending || static_cast<LONG>(::GetTickCount() - m_flushDeadline.load()) >= 0)
            {
                break;
            }
        }

        fd_set readable, writable;
        FD_ZERO(&readable);
        FD_ZERO(&writable);
        int count = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_listen != INVALID_SOCKET && count < FD_SETSIZE)
            {
                FD_SET(m_listen, &readable);
                ++count;
            }
            if (m_udp != INVALID_SOCKET && count < FD_SETSIZE)
            {
                FD_SET(m_udp, &readable);
                ++count;
            }
            for (const auto& entry : m_connections)
            {
                if (count >= FD_SETSIZE)
                {
                    break;
                }
                FD_SET(entry.second.socket, &readable);
                if (!entry.second.out.empty())
                {
                    FD_SET(entry.second.socket, &writable);
                }
                ++count;
            }
        }
        if (count == 0)
        {
            ::Sleep(10);
            continue;
        }

        timeval timeout{0, kSelectTimeoutUs};
        const int ready = ::select(0, &readable, &writable, nullptr, &timeout);
        if (ready == SOCKET_ERROR)
        {
            ::Sleep(10);
            continue;
        }

        if (m_listen != INVALID_SOCKET && FD_ISSET(m_listen, &readable))
        {
            AcceptPending();
        }
        if (m_udp != INVALID_SOCKET && FD_ISSET(m_udp, &readable))
        {
            AnswerDiscovery();
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<uint32_t> broken;
        for (auto& entry : m_connections)
        {
            Connection& c = entry.second;
            if (ready > 0 && FD_ISSET(c.socket, &readable) && !ReceiveFrom(entry.first, c))
            {
                broken.push_back(entry.first);
                continue;
            }
            if (!c.out.empty() && FD_ISSET(c.socket, &writable) && !SendPending(c))
            {
                broken.push_back(entry.first);
                continue;
            }
            if (c.closing && c.out.empty())
            {
                ::shutdown(c.socket, SD_SEND);
                broken.push_back(entry.first);
            }
        }
        for (const uint32_t id : broken)
        {
            CloseLocked(id, true);
        }
    }
}

void NetEngine::AcceptPending()
{
    while (true)
    {
        SOCKET s = ::accept(m_listen, nullptr, nullptr);
        if (s == INVALID_SOCKET)
        {
            return;
        }
        SetNonBlocking(s);
        SetNoDelay(s);
        uint32_t id = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            id = m_nextId++;
            Connection c;
            c.socket = s;
            m_connections.emplace(id, std::move(c));
        }
        NetEvent event;
        event.kind = NetEvent::Kind::Accepted;
        event.connection = id;
        PushEvent(std::move(event));
    }
}

void NetEngine::AnswerDiscovery()
{
    char buffer[kMaxDatagram];
    sockaddr_in from{};
    int fromLength = sizeof(from);
    while (true)
    {
        const int received =
            ::recvfrom(m_udp, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (received == SOCKET_ERROR)
        {
            return;
        }
        // 요청 형식 확인은 프로토콜 층의 일이지만, 여기서는 앞 4바이트(매직 번호)만 확인해 아무 패킷에나 답하지 않게 합니다.
        std::vector<uint8_t> reply;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            reply = m_discoveryReply;
        }
        if (received >= 4 && !reply.empty() && std::memcmp(buffer, reply.data(), 4) == 0)
        {
            ::sendto(m_udp, reinterpret_cast<const char*>(reply.data()), static_cast<int>(reply.size()), 0,
                     reinterpret_cast<const sockaddr*>(&from), fromLength);
        }
        fromLength = sizeof(from);
    }
}

bool NetEngine::ReceiveFrom(uint32_t id, Connection& c)
{
    char buffer[64 * 1024];
    bool open = true;
    while (true)
    {
        const int received = ::recv(c.socket, buffer, sizeof(buffer), 0);
        if (received == 0)
        {
            open = false; // 상대가 연결을 닫음. 그 전에 받은 프레임(거절 이유 등)은 아래에서 먼저 넘깁니다.
            break;
        }
        if (received == SOCKET_ERROR)
        {
            if (::WSAGetLastError() == WSAEWOULDBLOCK)
            {
                break;
            }
            open = false;
            break;
        }
        c.in.insert(c.in.end(), buffer, buffer + received);
    }

    // 완성된 프레임을 잘라 이벤트로 넘깁니다.
    size_t pos = 0;
    while (c.in.size() - pos >= sizeof(uint32_t))
    {
        uint32_t length = 0;
        std::memcpy(&length, c.in.data() + pos, sizeof(length));
        if (length > kMaxFrameBytes)
        {
            return false; // 손상되었거나 다른 프로그램의 데이터
        }
        if (c.in.size() - pos - sizeof(length) < length)
        {
            break;
        }
        NetEvent event;
        event.kind = NetEvent::Kind::Received;
        event.connection = id;
        const auto begin = c.in.begin() + static_cast<std::ptrdiff_t>(pos + sizeof(length));
        event.payload.assign(begin, begin + length);
        PushEvent(std::move(event));
        pos += sizeof(length) + length;
    }
    c.in.erase(c.in.begin(), c.in.begin() + static_cast<std::ptrdiff_t>(pos));
    return open;
}

bool NetEngine::SendPending(Connection& c)
{
    while (!c.out.empty())
    {
        const std::vector<uint8_t>& frame = *c.out.front();
        const size_t left = frame.size() - c.outOffset;
        const int chunk = static_cast<int>((std::min)(left, static_cast<size_t>(1024 * 1024)));
        const int sent = ::send(c.socket, reinterpret_cast<const char*>(frame.data() + c.outOffset), chunk, 0);
        if (sent == SOCKET_ERROR)
        {
            return ::WSAGetLastError() == WSAEWOULDBLOCK;
        }
        c.outOffset += static_cast<size_t>(sent);
        if (c.outOffset == frame.size())
        {
            c.queuedBytes -= frame.size();
            c.out.pop_front();
            c.outOffset = 0;
        }
    }
    return true;
}
