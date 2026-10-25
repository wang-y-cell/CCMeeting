#pragma once

/*
 * message_type.h — 协议消息类型枚举
 *
 * 与 cloudMeet 客户端 wire 协议保持一致，数值不可随意修改。
 * 0~15 为客户端上行，20~24 为服务器下行。
 */

#include <cstdint>
#include <string>

namespace protocol {

enum class MessageType : uint16_t {
    // 客户端 → 服务器
    ImgSend = 0,   // reserved legacy
    ImgRecv,       // reserved legacy
    AudioSend,     // reserved legacy
    AudioRecv,     // reserved legacy
    TextSend,
    TextRecv,
    CreateMeeting,
    ExitMeeting,
    JoinMeeting,
    CloseCamera,
    RoomSettings,  // 房主修改房间开关（payload: 4 字节大端 flags）

    // 服务器 → 客户端
    CreateMeetingResponse = 20,
    PartnerExit = 21,
    PartnerJoin = 22,
    JoinMeetingResponse = 23,
PartnerJoin2 = 24,
    UserProfile = 25,
    RoomSettingsNotify = 26,  // 房间开关当前值（服务器判定房主后广播/补发）
};

// ---- 房间开关位（与客户端 client/include/network/messageNet/netheader.h 一致）----
constexpr uint32_t kRoomSettingsAllowRecord = 1u << 0;       ///< 允许录制会议
constexpr uint32_t kRoomSettingsAllowMemberShare = 1u << 1;  ///< 允许普通成员共享屏幕
constexpr uint32_t kRoomSettingsDefault =
    kRoomSettingsAllowRecord | kRoomSettingsAllowMemberShare;

std::string get_type_name(MessageType type);

}  // namespace protocol
