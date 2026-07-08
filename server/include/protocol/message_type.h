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
    ImgSend = 0,
    ImgRecv,
    AudioSend,
    AudioRecv,
    TextSend,
    TextRecv,
    CreateMeeting,
    ExitMeeting,
    JoinMeeting,
    CloseCamera,

    // 服务器 → 客户端
    CreateMeetingResponse = 20,
    PartnerExit = 21,
    PartnerJoin = 22,
    JoinMeetingResponse = 23,
    PartnerJoin2 = 24,
};

std::string get_type_name(MessageType type);

}  // namespace protocol
