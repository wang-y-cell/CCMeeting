#pragma once

/*
 * message_type.h — 协议消息类型枚举
 *
 * 与客户端 wire 协议保持一致，数值不可随意修改。
 * 0~6 为客户端上行，20~26 为服务器下行；线上通过 JSON `t` 字段传类型名，
 * 枚举数值仅用于本进程内部分派。
 */

#include <cstdint>
#include <string>

namespace protocol {

enum class MessageType : uint16_t {
    // 客户端 → 服务器
    TextSend = 0,
    TextRecv,
    CreateMeeting,
    ExitMeeting,
    JoinMeeting,
    CloseCamera,
    RoomSettings,  // 房主修改房间开关（data: allow_record / allow_member_share）

    // 服务器 → 客户端
    CreateMeetingResponse = 20,
    PartnerExit = 21,
    PartnerJoin = 22,
    JoinMeetingResponse = 23,
    PartnerJoin2 = 24,
    UserProfile = 25,
    RoomSettingsNotify = 26,  // 房间开关当前值（服务器判定房主后广播/补发）
};

std::string get_type_name(MessageType type);

}  // namespace protocol
