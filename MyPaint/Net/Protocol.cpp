/**
 * @file Protocol.cpp
 * @brief 실시간 공유 메시지 형식 구현
 */
#include "pch.h"
#include "Net/Protocol.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <atlimage.h>
#include <cwctype>

#include "Core/ImageCommands.h" // kMaxCanvasSide

namespace Share
{
namespace
{
constexpr uint32_t kDiscoveryMagic = 0x4453504D; ///< "MPSD"
constexpr uint8_t kDiscoveryQuery = 'Q';
constexpr uint8_t kDiscoveryReply = 'R';
} // namespace

ByteWriter Begin(MsgType type)
{
    ByteWriter writer;
    writer.Write(static_cast<uint8_t>(type));
    return writer;
}

void WriteString(ByteWriter& writer, const std::wstring& text)
{
    writer.WriteVector(std::vector<wchar_t>(text.begin(), text.end()));
}

std::wstring ReadString(ByteReader& reader, size_t maxLength)
{
    const auto chars = reader.ReadVector<wchar_t>(static_cast<uint32_t>(maxLength));
    return std::wstring(chars.begin(), chars.end());
}

void WriteRect(ByteWriter& writer, const CRect& rect)
{
    writer.Write(static_cast<int32_t>(rect.left));
    writer.Write(static_cast<int32_t>(rect.top));
    writer.Write(static_cast<int32_t>(rect.right));
    writer.Write(static_cast<int32_t>(rect.bottom));
}

CRect ReadRect(ByteReader& reader)
{
    const auto l = reader.Read<int32_t>();
    const auto t = reader.Read<int32_t>();
    const auto r = reader.Read<int32_t>();
    const auto b = reader.Read<int32_t>();
    return CRect(l, t, r, b);
}

std::wstring CleanText(const std::wstring& text, size_t maxLength)
{
    std::wstring result;
    for (const wchar_t ch : text)
    {
        result += std::iswcntrl(ch) ? L' ' : ch;
    }
    const size_t first = result.find_first_not_of(L' ');
    if (first == std::wstring::npos)
    {
        return std::wstring();
    }
    result = result.substr(first, result.find_last_not_of(L' ') - first + 1);
    if (result.size() > maxLength)
    {
        result.resize(maxLength);
    }
    return result;
}

bool EncodeSnapshot(const Surface& surface, std::vector<uint8_t>& png)
{
    CImage image;
    if (!surface.IsValid() || !image.Create(surface.Width(), surface.Height(), 24))
    {
        return false;
    }
    HDC dc = image.GetDC();
    ::BitBlt(dc, 0, 0, surface.Width(), surface.Height(), surface.DC(), 0, 0, SRCCOPY);
    image.ReleaseDC();

    IStream* stream = nullptr;
    if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
    {
        return false;
    }
    bool ok = SUCCEEDED(image.Save(stream, Gdiplus::ImageFormatPNG));
    if (ok)
    {
        HGLOBAL memory = nullptr;
        ::GetHGlobalFromStream(stream, &memory);
        STATSTG stat{};
        stream->Stat(&stat, STATFLAG_NONAME);
        const auto size = static_cast<size_t>(stat.cbSize.QuadPart);
        const void* data = ::GlobalLock(memory);
        ok = data != nullptr;
        if (ok)
        {
            png.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
            ::GlobalUnlock(memory);
        }
    }
    stream->Release();
    return ok;
}

bool DecodeSnapshot(const uint8_t* data, size_t size, Surface& surface)
{
    if (size == 0)
    {
        return false;
    }
    HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, size);
    if (memory == nullptr)
    {
        return false;
    }
    void* target = ::GlobalLock(memory);
    std::memcpy(target, data, size);
    ::GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(::CreateStreamOnHGlobal(memory, TRUE, &stream)))
    {
        ::GlobalFree(memory);
        return false;
    }
    CImage image;
    const bool loaded = SUCCEEDED(image.Load(stream));
    stream->Release();
    if (!loaded || image.GetWidth() <= 0 || image.GetHeight() <= 0 || image.GetWidth() > kMaxCanvasSide ||
        image.GetHeight() > kMaxCanvasSide)
    {
        return false;
    }
    Surface decoded;
    if (!decoded.Create(image.GetWidth(), image.GetHeight(), RGB(255, 255, 255)))
    {
        return false;
    }
    image.BitBlt(decoded.DC(), 0, 0, SRCCOPY);
    Pixel* bits = decoded.Bits();
    const size_t count = static_cast<size_t>(decoded.Width()) * decoded.Height();
    for (size_t i = 0; i < count; ++i)
    {
        bits[i] &= 0x00FFFFFF;
    }
    surface = std::move(decoded);
    return true;
}

std::vector<uint8_t> MakeDiscoveryQuery()
{
    ByteWriter writer;
    writer.Write(kDiscoveryMagic);
    writer.Write(kDiscoveryQuery);
    writer.Write(kVersion);
    return writer.Take();
}

std::vector<uint8_t> MakeDiscoveryReply(const RoomInfo& info)
{
    ByteWriter writer;
    writer.Write(kDiscoveryMagic);
    writer.Write(kDiscoveryReply);
    writer.Write(kVersion);
    writer.Write(info.port);
    writer.Write(static_cast<uint8_t>(info.viewers));
    writer.Write(static_cast<uint8_t>(info.maxViewers));
    WriteString(writer, info.room);
    WriteString(writer, info.host);
    return writer.Take();
}

bool ParseDiscoveryReply(const uint8_t* data, size_t size, RoomInfo& info)
{
    try
    {
        ByteReader reader(data, size);
        if (reader.Read<uint32_t>() != kDiscoveryMagic || reader.Read<uint8_t>() != kDiscoveryReply ||
            reader.Read<uint16_t>() != kVersion)
        {
            return false;
        }
        info.port = reader.Read<uint16_t>();
        info.viewers = reader.Read<uint8_t>();
        info.maxViewers = reader.Read<uint8_t>();
        info.room = ReadString(reader, kMaxRoomLength);
        info.host = ReadString(reader, kMaxNameLength);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

std::vector<std::wstring> LocalAddresses()
{
    std::vector<std::wstring> addresses;
    wchar_t name[256] = {};
    if (::GetHostNameW(name, static_cast<int>(std::size(name))) != 0)
    {
        return addresses;
    }
    addrinfoW hints{};
    hints.ai_family = AF_INET;
    addrinfoW* result = nullptr;
    if (::GetAddrInfoW(name, nullptr, &hints, &result) != 0)
    {
        return addresses;
    }
    for (const addrinfoW* p = result; p != nullptr; p = p->ai_next)
    {
        wchar_t text[64] = {};
        const auto* address = reinterpret_cast<const sockaddr_in*>(p->ai_addr);
        if (::InetNtopW(AF_INET, &address->sin_addr, text, std::size(text)) != nullptr)
        {
            addresses.emplace_back(text);
        }
    }
    ::FreeAddrInfoW(result);
    return addresses;
}
} // namespace Share
