/**
 * @file ShareSession.cpp
 * @brief ShareHost, ShareViewer 구현
 */
#include "pch.h"
#include "Net/ShareSession.h"

#include <set>
#include <stdexcept>

#include "Tools/Tool.h"

using namespace Share;

namespace
{
/** @brief 끝낼 때 끝 인사를 보내려고 기다리는 최대 시간 */
constexpr DWORD kByeFlushMs = 500;

/** @brief 보는 사람이 닉네임을 비워 두었을 때 */
constexpr wchar_t kDefaultViewerName[] = L"손님";

std::vector<uint8_t> MakeChatMessage(const std::wstring& sender, const std::wstring& text, bool notice)
{
    ByteWriter writer = Begin(MsgType::Chat);
    WriteString(writer, sender);
    WriteString(writer, text);
    writer.Write(static_cast<uint8_t>(notice ? 1 : 0));
    return writer.Take();
}
} // namespace

// ===========================================================================
// ShareHost
// ===========================================================================

ShareHost::ShareHost(Document& document, IShareObserver& observer) : m_document(document), m_observer(observer)
{
}

ShareHost::~ShareHost()
{
    Stop();
}

bool ShareHost::Start(const std::wstring& hostName, const std::wstring& room, uint16_t port, HWND notify,
                      UINT message, std::wstring& error)
{
    Stop();
    m_hostName = hostName;
    m_room = room;
    m_port = port;
    m_peers.clear();
    if (!m_engine.StartServer(port, kDiscoveryPort, notify, message, error))
    {
        return false;
    }
    m_document.AddListener(this);
    UpdateDiscovery();
    m_observer.OnShareParticipants(Names());
    return true;
}

void ShareHost::Stop()
{
    if (!m_engine.IsRunning())
    {
        return;
    }
    m_document.RemoveListener(this);
    SendToViewers(Begin(MsgType::Bye).Take());
    m_engine.Stop(kByeFlushMs);
    m_peers.clear();
    m_lastPreview.clear();
    m_lastPatchPixels.reset();
}

int ShareHost::ViewerCount() const
{
    int count = 0;
    for (const auto& entry : m_peers)
    {
        count += entry.second.joined ? 1 : 0;
    }
    return count;
}

std::vector<std::wstring> ShareHost::Names() const
{
    std::vector<std::wstring> names{m_hostName};
    for (const auto& entry : m_peers)
    {
        if (entry.second.joined)
        {
            names.push_back(entry.second.name);
        }
    }
    return names;
}

void ShareHost::UpdateDiscovery()
{
    RoomInfo info;
    info.room = m_room;
    info.host = m_hostName;
    info.port = m_port;
    info.viewers = ViewerCount();
    info.maxViewers = kMaxViewers;
    m_engine.SetDiscoveryReply(MakeDiscoveryReply(info));
}

void ShareHost::SendTo(uint32_t id, const std::vector<uint8_t>& payload)
{
    m_engine.Send(id, NetEngine::MakeFrame(payload));
}

void ShareHost::SendToViewers(const std::vector<uint8_t>& payload)
{
    // 프레임 하나를 만들어 모든 연결이 함께 씁니다.
    NetFrame frame;
    for (const auto& entry : m_peers)
    {
        if (entry.second.joined)
        {
            if (!frame)
            {
                frame = NetEngine::MakeFrame(payload);
            }
            m_engine.Send(entry.first, frame);
        }
    }
}

void ShareHost::BroadcastChat(const std::wstring& sender, const std::wstring& text, bool notice)
{
    SendToViewers(MakeChatMessage(sender, text, notice));
    m_observer.OnShareChat(sender, text, notice);
}

void ShareHost::BroadcastParticipants()
{
    const std::vector<std::wstring> names = Names();
    ByteWriter writer = Begin(MsgType::Participants);
    writer.Write(static_cast<uint32_t>(names.size()));
    for (const std::wstring& name : names)
    {
        WriteString(writer, name);
    }
    SendToViewers(writer.Take());
    m_observer.OnShareParticipants(names);
    UpdateDiscovery();
}

std::wstring ShareHost::UniqueName(const std::wstring& wanted) const
{
    std::set<std::wstring> taken{m_hostName};
    for (const auto& entry : m_peers)
    {
        if (entry.second.joined)
        {
            taken.insert(entry.second.name);
        }
    }
    if (taken.count(wanted) == 0)
    {
        return wanted;
    }
    for (int n = 2;; ++n)
    {
        const std::wstring candidate = wanted + L" (" + std::to_wstring(n) + L")";
        if (taken.count(candidate) == 0)
        {
            return candidate;
        }
    }
}

std::vector<uint8_t> ShareHost::MakeSnapshotMessage() const
{
    std::vector<uint8_t> png;
    if (!EncodeSnapshot(m_document.GetSurface(), png))
    {
        return {};
    }
    ByteWriter writer = Begin(MsgType::Snapshot);
    writer.Write(static_cast<uint32_t>(png.size()));
    writer.WriteBytes(png.data(), png.size());
    return writer.Take();
}

void ShareHost::OnNetMessage()
{
    for (NetEvent& event : m_engine.TakeEvents())
    {
        switch (event.kind)
        {
        case NetEvent::Kind::Accepted:
            m_peers[event.connection] = Peer{}; // 인사(Hello)를 받기 전까지는 참가자가 아닙니다.
            break;
        case NetEvent::Kind::Received:
            HandleFrame(event.connection, event.payload);
            break;
        case NetEvent::Kind::Closed:
            HandleClosed(event.connection);
            break;
        default:
            break;
        }
    }
}

void ShareHost::HandleFrame(uint32_t id, const std::vector<uint8_t>& payload)
{
    const auto it = m_peers.find(id);
    if (it == m_peers.end() || payload.empty())
    {
        return;
    }
    try
    {
        ByteReader reader(payload.data(), payload.size());
        const auto type = static_cast<MsgType>(reader.Read<uint8_t>());
        if (!it->second.joined)
        {
            if (type != MsgType::Hello)
            {
                throw std::runtime_error("인사 전에 다른 메시지를 받았습니다.");
            }
            HandleHello(id, reader);
            return;
        }
        if (type == MsgType::ChatSend)
        {
            const std::wstring text = CleanText(ReadString(reader, kMaxChatLength), kMaxChatLength);
            if (!text.empty())
            {
                BroadcastChat(it->second.name, text, false);
            }
        }
        // 보는 사람은 그림을 바꿀 수 없으므로 다른 메시지는 무시합니다.
    }
    catch (const std::exception&)
    {
        m_engine.Close(id); // 형식이 맞지 않는 상대는 끊습니다.
    }
}

void ShareHost::HandleHello(uint32_t id, ByteReader& reader)
{
    const auto magic = reader.Read<uint32_t>();
    const auto version = reader.Read<uint16_t>();
    std::wstring name = CleanText(ReadString(reader, kMaxNameLength * 2), kMaxNameLength);

    auto reject = [&](const wchar_t* reason) {
        ByteWriter writer = Begin(MsgType::Reject);
        WriteString(writer, reason);
        SendTo(id, writer.Take());
        m_engine.Close(id);
    };
    if (magic != kMagic || version != kVersion)
    {
        reject(L"MyPaint 버전이 달라서 참가할 수 없습니다.");
        return;
    }
    if (ViewerCount() >= kMaxViewers)
    {
        reject(L"방이 가득 찼습니다. (최대 8명)");
        return;
    }
    if (name.empty())
    {
        name = kDefaultViewerName;
    }

    Peer& peer = m_peers[id];
    peer.name = UniqueName(name);
    peer.joined = true;

    ByteWriter welcome = Begin(MsgType::Welcome);
    WriteString(welcome, peer.name);
    WriteString(welcome, m_room);
    WriteString(welcome, m_hostName);
    SendTo(id, welcome.Take());
    SendTo(id, MakeSnapshotMessage());

    // 새로 온 사람도 지금 그리는 중인 모습과 커서를 받도록, 다음 Tick에 처음부터 다시 보냅니다.
    m_lastPreview.clear();
    m_lastPatchPixels.reset();
    m_previewDirty = true;
    m_cursorDirty = true;

    BroadcastParticipants();
    BroadcastChat(std::wstring(), peer.name + L" 님이 들어왔습니다.", true);
}

void ShareHost::HandleClosed(uint32_t id)
{
    const auto it = m_peers.find(id);
    if (it == m_peers.end())
    {
        return;
    }
    const bool joined = it->second.joined;
    const std::wstring name = it->second.name;
    m_peers.erase(it);
    if (joined)
    {
        BroadcastParticipants();
        BroadcastChat(std::wstring(), name + L" 님이 나갔습니다.", true);
    }
}

void ShareHost::SendChat(const std::wstring& text)
{
    const std::wstring clean = CleanText(text, kMaxChatLength);
    if (!clean.empty())
    {
        BroadcastChat(m_hostName, clean, false);
    }
}

void ShareHost::OnCommandApplied(const Command& command)
{
    if (ViewerCount() == 0)
    {
        return;
    }
    ByteWriter writer = Begin(MsgType::Command);
    WriteCommand(writer, command);
    SendToViewers(writer.Take());
}

void ShareHost::OnDocumentRestored(const CRect& dirty, bool whole)
{
    if (ViewerCount() == 0)
    {
        return;
    }
    if (whole)
    {
        SendToViewers(MakeSnapshotMessage());
        return;
    }
    // 실행 취소/다시 실행으로 바뀐 영역의 결과 픽셀을 보냅니다.
    CRect area = dirty;
    area &= m_document.GetSurface().Bounds();
    if (area.IsRectEmpty())
    {
        return;
    }
    PixelBlock block = m_document.GetSurface().ReadBlock(area);
    const PatchCommand patch(CRect(), 0, area.TopLeft(), area.Size(),
                             std::make_shared<const std::vector<Pixel>>(std::move(block.pixels)));
    ByteWriter writer = Begin(MsgType::Command);
    WriteCommand(writer, patch);
    SendToViewers(writer.Take());
}

void ShareHost::SetCursor(bool visible, CPoint docPoint)
{
    if (visible != m_cursorVisible || (visible && docPoint != m_cursor))
    {
        m_cursorVisible = visible;
        m_cursor = docPoint;
        m_cursorDirty = true;
    }
}

void ShareHost::Tick(const Tool* tool)
{
    if (ViewerCount() == 0)
    {
        m_previewDirty = false;
        m_cursorDirty = false;
        return;
    }

    if (m_previewDirty)
    {
        m_previewDirty = false;
        std::unique_ptr<Command> owned = tool != nullptr ? tool->MakeSharedPreview(m_document.GetSurface()) : nullptr;
        const Command* preview = owned ? owned.get() : (tool != nullptr ? tool->PreviewCommand() : nullptr);

        ByteWriter writer = Begin(MsgType::Preview);
        WriteRect(writer, tool != nullptr ? tool->OverlayRect() : CRect());
        const auto* patch = dynamic_cast<const PatchCommand*>(preview);
        if (preview == nullptr)
        {
            writer.Write(static_cast<uint8_t>(PreviewKind::None));
            m_lastPatchPixels.reset();
        }
        else if (patch != nullptr && patch->Pixels() && patch->Pixels() == m_lastPatchPixels)
        {
            // 선택 영역을 옮기는 중: 픽셀은 이미 보냈으므로 위치만 보냅니다.
            writer.Write(static_cast<uint8_t>(PreviewKind::MovedPatch));
            WriteRect(writer, patch->ClearRect());
            writer.Write(static_cast<uint32_t>(patch->ClearColor()));
            writer.Write(static_cast<int32_t>(patch->Dest().x));
            writer.Write(static_cast<int32_t>(patch->Dest().y));
            writer.Write(static_cast<int32_t>(patch->PatchSize().cx));
            writer.Write(static_cast<int32_t>(patch->PatchSize().cy));
        }
        else
        {
            writer.Write(static_cast<uint8_t>(PreviewKind::Command));
            WriteCommand(writer, *preview);
            m_lastPatchPixels = patch != nullptr ? patch->Pixels() : nullptr;
        }
        std::vector<uint8_t> message = writer.Take();
        if (message != m_lastPreview)
        {
            SendToViewers(message);
            m_lastPreview = std::move(message);
        }
    }

    if (m_cursorDirty)
    {
        m_cursorDirty = false;
        ByteWriter writer = Begin(MsgType::Cursor);
        writer.Write(static_cast<uint8_t>(m_cursorVisible ? 1 : 0));
        writer.Write(static_cast<int32_t>(m_cursor.x));
        writer.Write(static_cast<int32_t>(m_cursor.y));
        SendToViewers(writer.Take());
    }
}

// ===========================================================================
// ShareViewer
// ===========================================================================

ShareViewer::ShareViewer(Document& document, IShareObserver& observer) : m_document(document), m_observer(observer)
{
}

void ShareViewer::Start(const std::wstring& address, uint16_t port, const std::wstring& name, HWND notify,
                        UINT message)
{
    Stop();
    m_name = name;
    m_room.clear();
    m_hostName.clear();
    m_joined = false;
    m_ended = false;
    m_lastPatchPixels.reset();
    m_engine.StartClient(address, port, notify, message);
}

void ShareViewer::Stop()
{
    m_ended = true;
    m_joined = false;
    m_engine.Stop();
}

void ShareViewer::End(ShareEndReason reason, const std::wstring& detail)
{
    if (m_ended)
    {
        return;
    }
    Stop();
    m_observer.OnShareEnded(reason, detail);
}

void ShareViewer::SendChat(const std::wstring& text)
{
    const std::wstring clean = CleanText(text, kMaxChatLength);
    if (!m_joined || clean.empty())
    {
        return;
    }
    ByteWriter writer = Begin(MsgType::ChatSend);
    WriteString(writer, clean);
    m_engine.Send(1, NetEngine::MakeFrame(writer.Take()));
}

void ShareViewer::OnNetMessage()
{
    for (NetEvent& event : m_engine.TakeEvents())
    {
        if (m_ended)
        {
            return;
        }
        switch (event.kind)
        {
        case NetEvent::Kind::Connected:
        {
            ByteWriter writer = Begin(MsgType::Hello);
            writer.Write(kMagic);
            writer.Write(kVersion);
            WriteString(writer, m_name);
            m_engine.Send(1, NetEngine::MakeFrame(writer.Take()));
            break;
        }
        case NetEvent::Kind::ConnectFailed:
            End(ShareEndReason::ConnectFailed, std::wstring());
            return;
        case NetEvent::Kind::Received:
            HandleFrame(event.payload);
            break;
        case NetEvent::Kind::Closed:
            End(ShareEndReason::ConnectionLost, std::wstring());
            return;
        default:
            break;
        }
    }
}

void ShareViewer::HandleFrame(const std::vector<uint8_t>& payload)
{
    if (payload.empty())
    {
        return;
    }
    try
    {
        ByteReader reader(payload.data(), payload.size());
        const auto type = static_cast<MsgType>(reader.Read<uint8_t>());
        switch (type)
        {
        case MsgType::Welcome:
        {
            const std::wstring myName = ReadString(reader, kMaxNameLength + 8);
            m_room = ReadString(reader, kMaxRoomLength);
            m_hostName = ReadString(reader, kMaxNameLength);
            m_joined = true;
            m_observer.OnViewerJoined(m_room, m_hostName, myName);
            break;
        }
        case MsgType::Reject:
            End(ShareEndReason::Rejected, ReadString(reader, 200));
            break;
        case MsgType::Snapshot:
        {
            const auto size = reader.Read<uint32_t>();
            if (size > reader.Remaining())
            {
                throw std::runtime_error("그림 데이터가 부족합니다.");
            }
            Surface surface;
            if (!DecodeSnapshot(reader.Current(), size, surface))
            {
                throw std::runtime_error("그림을 풀 수 없습니다.");
            }
            m_document.Reset(std::move(surface));
            break;
        }
        case MsgType::Command:
        {
            const std::unique_ptr<Command> command = ReadCommand(reader);
            m_document.ApplyUnrecorded(*command);
            break;
        }
        case MsgType::Preview:
            HandlePreview(reader);
            break;
        case MsgType::Cursor:
        {
            const bool visible = reader.Read<uint8_t>() != 0;
            const auto x = reader.Read<int32_t>();
            const auto y = reader.Read<int32_t>();
            m_observer.OnViewerCursor(visible, CPoint(x, y));
            break;
        }
        case MsgType::Chat:
        {
            const std::wstring sender = ReadString(reader, kMaxNameLength + 8);
            const std::wstring text = ReadString(reader, kMaxChatLength + 64);
            const bool notice = reader.Read<uint8_t>() != 0;
            m_observer.OnShareChat(sender, text, notice);
            break;
        }
        case MsgType::Participants:
        {
            const auto count = reader.Read<uint32_t>();
            if (count > static_cast<uint32_t>(kMaxViewers + 1))
            {
                throw std::runtime_error("참가자 수가 너무 많습니다.");
            }
            std::vector<std::wstring> names;
            for (uint32_t i = 0; i < count; ++i)
            {
                names.push_back(ReadString(reader, kMaxNameLength + 8));
            }
            m_observer.OnShareParticipants(names);
            break;
        }
        case MsgType::Bye:
            End(ShareEndReason::HostEnded, std::wstring());
            break;
        default:
            break; // 모르는 메시지는 무시합니다. (나중 버전과의 호환)
        }
    }
    catch (const std::exception&)
    {
        End(ShareEndReason::ConnectionLost, L"잘못된 데이터를 받았습니다.");
    }
}

void ShareViewer::HandlePreview(ByteReader& reader)
{
    const CRect overlay = ReadRect(reader);
    const auto kind = static_cast<PreviewKind>(reader.Read<uint8_t>());
    std::unique_ptr<Command> preview;
    switch (kind)
    {
    case PreviewKind::Command:
    {
        preview = ReadCommand(reader);
        const auto* patch = dynamic_cast<const PatchCommand*>(preview.get());
        m_lastPatchPixels = patch != nullptr ? patch->Pixels() : nullptr;
        break;
    }
    case PreviewKind::MovedPatch:
    {
        const CRect clearRect = ReadRect(reader);
        const auto clearColor = reader.Read<uint32_t>() & 0x00FFFFFF;
        const auto x = reader.Read<int32_t>();
        const auto y = reader.Read<int32_t>();
        const auto w = reader.Read<int32_t>();
        const auto h = reader.Read<int32_t>();
        if (m_lastPatchPixels && w > 0 && h > 0 && m_lastPatchPixels->size() == static_cast<size_t>(w) * h)
        {
            preview = std::make_unique<PatchCommand>(clearRect, clearColor, CPoint(x, y), CSize(w, h),
                                                     m_lastPatchPixels);
        }
        break;
    }
    default:
        m_lastPatchPixels.reset();
        break;
    }
    m_observer.OnViewerPreview(std::move(preview), overlay);
}
