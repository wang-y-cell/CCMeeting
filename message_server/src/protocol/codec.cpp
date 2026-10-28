#include "protocol/codec.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstring>

namespace protocol {

namespace {

using json = nlohmann::json;

uint32_t read_uint32_be(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

void write_uint32_be(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(value & 0xFF);
}

/// 标准 Base64（服务端不再依赖 Qt）
std::string base64_encode(const std::vector<uint8_t>& bytes) {
    static const char* kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    size_t i = 0;
    while (i + 2 < bytes.size()) {
        const uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16) |
                                (static_cast<uint32_t>(bytes[i + 1]) << 8) |
                                static_cast<uint32_t>(bytes[i + 2]);
        out.push_back(kTable[(triple >> 18) & 0x3F]);
        out.push_back(kTable[(triple >> 12) & 0x3F]);
        out.push_back(kTable[(triple >> 6) & 0x3F]);
        out.push_back(kTable[triple & 0x3F]);
        i += 3;
    }
    if (i + 1 == bytes.size()) {
        const uint32_t triple = static_cast<uint32_t>(bytes[i]) << 16;
        out.push_back(kTable[(triple >> 18) & 0x3F]);
        out.push_back(kTable[(triple >> 12) & 0x3F]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == bytes.size()) {
        const uint32_t triple = (static_cast<uint32_t>(bytes[i]) << 16) |
                                (static_cast<uint32_t>(bytes[i + 1]) << 8);
        out.push_back(kTable[(triple >> 18) & 0x3F]);
        out.push_back(kTable[(triple >> 12) & 0x3F]);
        out.push_back(kTable[(triple >> 6) & 0x3F]);
        out.push_back('=');
    }
    return out;
}

std::vector<uint8_t> base64_decode(const std::string& text) {
    std::array<int, 256> table{};
    table.fill(-1);
    const char* kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (int i = 0; i < 64; ++i) {
        table[static_cast<unsigned char>(kTable[i])] = i;
    }
    std::vector<uint8_t> out;
    out.reserve(text.size() / 4 * 3);
    uint32_t accumulator = 0;
    int bits = 0;
    for (const char ch : text) {
        if (ch == '=') {
            break;
        }
        const int value = table[static_cast<unsigned char>(ch)];
        if (value < 0) {
            continue;  // 忽略换行等空白
        }
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((accumulator >> bits) & 0xFF));
        }
    }
    return out;
}

}  // namespace

bool Codec::is_text_type(MessageType type) {
    return type == MessageType::TextSend || type == MessageType::TextRecv;
}

const char* Codec::type_name(MessageType type) {
    switch (type) {
        case MessageType::ImgSend:
            return "img_send";
        case MessageType::ImgRecv:
            return "img_recv";
        case MessageType::AudioSend:
            return "audio_send";
        case MessageType::AudioRecv:
            return "audio_recv";
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
        MessageType::ImgSend,        MessageType::ImgRecv,
        MessageType::AudioSend,      MessageType::AudioRecv,
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
    // Connection 内部约定：[uint32 大端长度][JSON]
    if (buffer.readable_size() < kLengthPrefixSize) {
        return DecodeStatus::NeedMore;
    }
    const uint8_t* head = buffer.read_begin();
    const uint32_t json_size = read_uint32_be(head);
    if (json_size == 0) {
        return DecodeStatus::Invalid;
    }
    if (buffer.readable_size() < kLengthPrefixSize + json_size) {
        return DecodeStatus::NeedMore;
    }

    const char* json_begin =
        reinterpret_cast<const char*>(head + kLengthPrefixSize);
    json document;
    try {
        document = json::parse(json_begin, json_begin + json_size);
    } catch (const std::exception&) {
        buffer.move_read_pos(kLengthPrefixSize + json_size);
        return DecodeStatus::Invalid;
    }

    const std::string type_text =
        document.value("t", std::string("unknown"));
    const std::optional<MessageType> type = type_from_name(type_text);
    if (!type.has_value()) {
        buffer.move_read_pos(kLengthPrefixSize + json_size);
        return DecodeStatus::Invalid;
    }

    packet.type = *type;
    packet.user_id = static_cast<uint64_t>(document.value("u", 0.0));

    if (document.contains("s")) {
        // 文本类：明文 UTF-8
        const std::string text = document.value("s", std::string());
        packet.payload.assign(text.begin(), text.end());
    } else if (document.contains("p")) {
        packet.payload =
            base64_decode(document.value("p", std::string()));
    } else {
        packet.payload.clear();
    }

    buffer.move_read_pos(kLengthPrefixSize + json_size);
    return DecodeStatus::Ok;
}

std::vector<uint8_t> Codec::encode(const Packet& packet) {
    json document;
    document["t"] = type_name(packet.type);
    document["u"] = static_cast<double>(packet.user_id);

    if (is_text_type(packet.type)) {
        // 文本消息走明文 s 字段，WS 上直接可读
        document["s"] =
            std::string(packet.payload.begin(), packet.payload.end());
    } else if (!packet.payload.empty()) {
        document["p"] = base64_encode(packet.payload);
    }

    const std::string text = document.dump();
    return std::vector<uint8_t>(text.begin(), text.end());
}

}  // namespace protocol
