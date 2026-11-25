#pragma once

/**
 * @file Protocol.h
 * @brief 실시간 공유 메시지 형식
 *
 * 프레임 내용의 첫 바이트가 메시지 종류이고, 나머지는 ByteWriter로 쓴 값입니다.
 * 그림 편집은 Command 직렬화(WriteCommand)를 그대로 실어 보냅니다.
 */

#include <cstdint>
#include <string>
#include <vector>

#include "Core/ByteStream.h"
#include "Core/Surface.h"

namespace Share
{
constexpr uint32_t kMagic = 0x4853504D; ///< "MPSH"
constexpr uint16_t kVersion = 1;
constexpr uint16_t kDefaultPort = 47800;
constexpr uint16_t kDiscoveryPort = 47801;
constexpr int kMaxViewers = 8;
constexpr size_t kMaxNameLength = 20;
constexpr size_t kMaxRoomLength = 40;
constexpr size_t kMaxChatLength = 300;

/** @brief 메시지 종류. 값을 바꾸지 마세요. */
enum class MsgType : uint8_t
{
    Hello = 1,        ///< 참가자 → 방장: 매직, 버전, 닉네임
    Welcome = 2,      ///< 방장 → 참가자: 정해진 닉네임, 방 이름, 방장 닉네임
    Reject = 3,       ///< 방장 → 참가자: 거절 이유
    Snapshot = 4,     ///< 방장 → 참가자: 그림 전체 (PNG)
    Command = 5,      ///< 방장 → 참가자: 편집 명령 (WriteCommand)
    Preview = 6,      ///< 방장 → 참가자: 그리는 중인 미리보기
    Cursor = 7,       ///< 방장 → 참가자: 방장 커서 위치
    ChatSend = 8,     ///< 참가자 → 방장: 채팅 글
    Chat = 9,         ///< 방장 → 참가자: 채팅 (보낸 사람, 글, 알림 여부)
    Participants = 10, ///< 방장 → 참가자: 참가자 목록 (첫 번째가 방장)
    Bye = 11,         ///< 방장 → 참가자: 공유를 끝냄
};

/** @brief 미리보기 메시지 안의 종류 */
enum class PreviewKind : uint8_t
{
    None = 0,         ///< 미리보기 없음
    Command = 1,      ///< 명령 하나 (WriteCommand)
    MovedPatch = 2,   ///< 바로 전에 보낸 픽셀 조각을 다른 위치에 (선택 영역 이동)
};

/** @brief 메시지 종류 바이트로 시작하는 writer */
ByteWriter Begin(MsgType type);

void WriteString(ByteWriter& writer, const std::wstring& text);
std::wstring ReadString(ByteReader& reader, size_t maxLength);
void WriteRect(ByteWriter& writer, const CRect& rect);
CRect ReadRect(ByteReader& reader);

/** @brief 제어 문자를 빼고 앞뒤 공백을 지운 뒤 maxLength로 자릅니다. */
std::wstring CleanText(const std::wstring& text, size_t maxLength);

/** @brief 그림 전체를 PNG로 압축합니다. */
bool EncodeSnapshot(const Surface& surface, std::vector<uint8_t>& png);

/** @brief PNG를 표면으로 풉니다. */
bool DecodeSnapshot(const uint8_t* data, size_t size, Surface& surface);

/** @brief 같은 네트워크의 방 정보 (방 찾기 응답) */
struct RoomInfo
{
    std::wstring room;
    std::wstring host;
    uint16_t port = kDefaultPort;
    int viewers = 0;
    int maxViewers = kMaxViewers;
    std::wstring address; ///< 응답을 보낸 주소 (받는 쪽이 채움)
};

std::vector<uint8_t> MakeDiscoveryQuery();
std::vector<uint8_t> MakeDiscoveryReply(const RoomInfo& info);
bool ParseDiscoveryReply(const uint8_t* data, size_t size, RoomInfo& info);

/** @brief 이 PC의 IPv4 주소들 (공유 시작 대화 상자에 보여 줄 용도) */
std::vector<std::wstring> LocalAddresses();
} // namespace Share
