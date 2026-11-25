#include "protocol/codec.h"

#include <nlohmann/json.hpp>

#include <exception>

namespace protocol {

namespace {

using json = nlohmann::json;

bool read_string(const json& object, const char* key, std::string& out) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) {
        return false;
    }
    out = it->get<std::string>();
    return true;
}

int read_int(const json& object, const char* key, int fallback) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) {
        return fallback;
    }
    try {
        return it->get<int>();
    } catch (const std::exception&) {
        return fallback;
    }
}

}  // namespace

const char* Codec::type_name(MessageType type) {
    switch (type) {
        case MessageType::TextSend:
            return "text_send";
        case MessageType::TextRecv:
            return "text_recv";
        case MessageType::CreateMeeting:
            return "create_meeting";
        case MessageType::ExitMeeting:
            return "exit_meeting";
        case MessageType::JoinMeeting:
            return "join_meeting";
        case MessageType::CloseCamera:
            return "close_camera";
        case MessageType::RoomSettings:
            return "room_settings";
        case MessageType::CreateMeetingResponse:
            return "create_meeting_response";
        case MessageType::PartnerExit:
            return "partner_exit";
        case MessageType::PartnerJoin:
            return "partner_join";
        case MessageType::JoinMeetingResponse:
            return "join_meeting_response";
        case MessageType::PartnerJoin2:
            return "partner_join2";
        case MessageType::UserProfile:
            return "user_profile";
        case MessageType::RoomSettingsNotify:
            return "room_settings_notify";
    }
    return "unknown";
}

std::optional<MessageType> Codec::type_from_name(const std::string& name) {
    static const MessageType kAll[] = {
        MessageType::TextSend,       MessageType::TextRecv,
        MessageType::CreateMeeting,  MessageType::ExitMeeting,
        MessageType::JoinMeeting,    MessageType::CloseCamera,
        MessageType::RoomSettings,   MessageType::CreateMeetingResponse,
        MessageType::PartnerExit,    MessageType::PartnerJoin,
        MessageType::JoinMeetingResponse, MessageType::PartnerJoin2,
        MessageType::UserProfile,    MessageType::RoomSettingsNotify,
    };
    for (const MessageType type : kAll) {
        if (name == type_name(type)) {
            return type;
        }
    }
    return std::nullopt;
}

DecodeStatus Codec::try_decode(Buffer& buffer, Packet& packet) {
    // 一条 WS 消息 = 一个完整 JSON：整个可读区就是一条消息
    if (buffer.readable_size() == 0) {
        return DecodeStatus::NeedMore;
    }

    const auto* raw = buffer.read_begin();
    const char* json_begin = reinterpret_cast<const char*>(raw);
    const char* json_end = json_begin + buffer.readable_size();

    json document;
    try {
        document = json::parse(json_begin, json_end);
    } catch (const std::exception&) {
        // 坏消息直接丢弃，避免连接卡死
        buffer.move_read_pos(buffer.readable_size());
        return DecodeStatus::Invalid;
    }
    // 无论后面的语义是否合法，这一条消息都已消费
    buffer.move_read_pos(buffer.readable_size());

    if (!document.is_object()) {
        return DecodeStatus::Invalid;
    }

    std::string type_text;
    if (!read_string(document, "t", type_text) || type_text.empty()) {
        return DecodeStatus::Invalid;
    }
    const std::optional<MessageType> type = type_from_name(type_text);
    if (!type.has_value()) {
        return DecodeStatus::Invalid;
    }

    // 解码阶段只做形状转换；上行身份由服务层决定（token / 连接状态）
    packet.type = *type;
    packet.auth_token.clear();
    packet.code = 0;
    packet.message = "ok";
    packet.data = json::object();

    read_string(document, "token", packet.auth_token);
    packet.code = read_int(document, "code", 0);
    read_string(document, "message", packet.message);
    if (const auto it = document.find("data"); it != document.end()) {
        packet.data = *it;
    }

    return DecodeStatus::Ok;
}

std::vector<uint8_t> Codec::encode(const Packet& packet) {
    json document;
    document["t"] = type_name(packet.type);
    document["code"] = packet.code;
    document["message"] = packet.message;
    document["data"] = packet.data.is_null() ? json::object() : packet.data;
    if (!packet.auth_token.empty()) {
        document["token"] = packet.auth_token;
    }

    const std::string text = document.dump();
    return std::vector<uint8_t>(text.begin(), text.end());
}

}  // namespace protocol
