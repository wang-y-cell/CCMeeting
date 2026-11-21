#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "network/buffer.h"
#include "protocol/codec.h"
#include "protocol/packet.h"

namespace {

void append_frame(Buffer &buffer, const std::vector<uint8_t> &json) {
    const uint32_t size = static_cast<uint32_t>(json.size());
    uint8_t head[4] = {static_cast<uint8_t>((size >> 24) & 0xFF),
                       static_cast<uint8_t>((size >> 16) & 0xFF),
                       static_cast<uint8_t>((size >> 8) & 0xFF),
                       static_cast<uint8_t>(size & 0xFF)};
    buffer.write(head, sizeof(head));
    if (!json.empty()) {
        buffer.write(const_cast<uint8_t *>(json.data()), json.size());
    }
}

protocol::Packet make_packet(protocol::MessageType type, uint64_t user_id,
                             std::vector<uint8_t> payload) {
    protocol::Packet packet;
    packet.type = type;
    packet.user_id = user_id;
    packet.payload = std::move(payload);
    return packet;
}

}  // namespace

TEST(CodecTypeNames, EveryTypeRoundTripsThroughName) {
    const protocol::MessageType all[] = {
        protocol::MessageType::TextSend,
        protocol::MessageType::TextRecv,
        protocol::MessageType::CreateMeeting,
        protocol::MessageType::ExitMeeting,
        protocol::MessageType::JoinMeeting,
        protocol::MessageType::CloseCamera,
        protocol::MessageType::RoomSettings,
        protocol::MessageType::CreateMeetingResponse,
        protocol::MessageType::PartnerExit,
        protocol::MessageType::PartnerJoin,
        protocol::MessageType::JoinMeetingResponse,
        protocol::MessageType::PartnerJoin2,
        protocol::MessageType::UserProfile,
        protocol::MessageType::RoomSettingsNotify,
    };
    for (const auto type : all) {
        const std::string name = protocol::Codec::type_name(type);
        EXPECT_NE(name, "unknown");
        const auto parsed = protocol::Codec::type_from_name(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_EQ(*parsed, type);
    }
}

TEST(CodecTypeNames, UnknownNameReturnsNullopt) {
    EXPECT_FALSE(protocol::Codec::type_from_name("no_such_type").has_value());
    EXPECT_FALSE(protocol::Codec::type_from_name("").has_value());
}

TEST(CodecTypeNames, TextTypesAreMarked) {
    EXPECT_TRUE(protocol::Codec::is_text_type(protocol::MessageType::TextSend));
    EXPECT_TRUE(protocol::Codec::is_text_type(protocol::MessageType::TextRecv));
    EXPECT_FALSE(protocol::Codec::is_text_type(protocol::MessageType::JoinMeeting));
}

TEST(CodecEncode, TextPayloadGoesIntoPlainTextField) {
    const std::string text = "hello 世界";
    const std::vector<uint8_t> payload(text.begin(), text.end());
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::TextSend, 42, payload));

    const std::string json_text(json.begin(), json.end());
    EXPECT_NE(json_text.find("\"t\":\"text_send\""), std::string::npos);
    EXPECT_NE(json_text.find("\"u\":42"), std::string::npos);
    EXPECT_NE(json_text.find("\"s\":\""), std::string::npos);
    EXPECT_EQ(json_text.find("\"p\""), std::string::npos);
}

TEST(CodecEncode, BinaryPayloadIsBase64Encoded) {
    const std::vector<uint8_t> payload{0x00, 0x01, 0x02, 0xFF, 0x7F};
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::CreateMeeting, 7, payload));
    const std::string json_text(json.begin(), json.end());

    EXPECT_NE(json_text.find("\"p\":\""), std::string::npos);
    EXPECT_EQ(json_text.find("\"s\""), std::string::npos);
    // {0,1,2,255,127} 的 Base64
    EXPECT_NE(json_text.find("AAEC/38="), std::string::npos);
}

TEST(CodecEncode, EmptyPayloadOmitsPayloadFields) {
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::JoinMeeting, 9, {}));
    const std::string json_text(json.begin(), json.end());
    EXPECT_EQ(json_text.find("\"p\""), std::string::npos);
    EXPECT_EQ(json_text.find("\"s\""), std::string::npos);
}

TEST(CodecDecode, TextMessageRoundTrips) {
    const std::string text = "hi there";
    const std::vector<uint8_t> payload(text.begin(), text.end());
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::TextRecv, 123, payload));

    Buffer buffer;
    append_frame(buffer, json);

    protocol::Packet packet;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::TextRecv);
    EXPECT_EQ(packet.user_id, 123u);
    EXPECT_EQ(std::string(packet.payload.begin(), packet.payload.end()), text);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecDecode, BinaryMessageRoundTrips) {
    const std::vector<uint8_t> payload{0xDE, 0xAD, 0xBE, 0xEF};
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::RoomSettings, 55, payload));

    Buffer buffer;
    append_frame(buffer, json);

    protocol::Packet packet;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::RoomSettings);
    EXPECT_EQ(packet.user_id, 55u);
    EXPECT_EQ(packet.payload, payload);
}

TEST(CodecDecode, EmptyPayloadMessageRoundTrips) {
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::CloseCamera, 1, {}));

    Buffer buffer;
    append_frame(buffer, json);

    protocol::Packet packet;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::CloseCamera);
    EXPECT_TRUE(packet.payload.empty());
}

TEST(CodecDecode, NeedMoreForEmptyBuffer) {
    Buffer buffer;
    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::NeedMore);
}

TEST(CodecDecode, NeedMoreForPartialLengthPrefix) {
    Buffer buffer;
    const uint8_t partial[2] = {0x00, 0x00};
    buffer.write(const_cast<uint8_t *>(partial), sizeof(partial));

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::NeedMore);
}

TEST(CodecDecode, NeedMoreForTruncatedJson) {
    const auto json = protocol::Codec::encode(
        make_packet(protocol::MessageType::JoinMeeting, 3, {1, 2, 3}));
    const uint32_t size = static_cast<uint32_t>(json.size());
    uint8_t head[4] = {static_cast<uint8_t>((size >> 24) & 0xFF),
                       static_cast<uint8_t>((size >> 16) & 0xFF),
                       static_cast<uint8_t>((size >> 8) & 0xFF),
                       static_cast<uint8_t>(size & 0xFF)};

    Buffer buffer;
    buffer.write(head, sizeof(head));
    buffer.write(const_cast<uint8_t *>(json.data()), json.size() - 1);

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::NeedMore);
    EXPECT_EQ(buffer.readable_size(), 4u + json.size() - 1);   // 不消费
}

TEST(CodecDecode, ZeroLengthJsonIsInvalid) {
    Buffer buffer;
    uint8_t head[4] = {0, 0, 0, 0};
    buffer.write(head, sizeof(head));

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
}

TEST(CodecDecode, MalformedJsonIsInvalidAndConsumed) {
    const std::string bad = "{not valid json";
    std::vector<uint8_t> json(bad.begin(), bad.end());

    Buffer buffer;
    append_frame(buffer, json);

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
    EXPECT_EQ(buffer.readable_size(), 0u);   // 坏帧被丢弃，避免卡死
}

TEST(CodecDecode, UnknownTypeIsInvalid) {
    const std::string text = R"({"t":"no_such_type","u":1})";
    std::vector<uint8_t> json(text.begin(), text.end());

    Buffer buffer;
    append_frame(buffer, json);

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecDecode, TwoFramesDecodeOneAtATime) {
    const auto first = protocol::Codec::encode(
        make_packet(protocol::MessageType::TextSend, 1, {0x41}));
    const auto second = protocol::Codec::encode(
        make_packet(protocol::MessageType::TextSend, 2, {0x42}));

    Buffer buffer;
    append_frame(buffer, first);
    append_frame(buffer, second);

    protocol::Packet packet;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.user_id, 1u);
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.user_id, 2u);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(BufferTest, ReadWritePositionsAndNormalize) {
    Buffer buffer(8);
    uint8_t data[4] = {1, 2, 3, 4};
    buffer.write(data, sizeof(data));
    EXPECT_EQ(buffer.readable_size(), 4u);

    buffer.move_read_pos(2);
    EXPECT_EQ(buffer.readable_size(), 2u);
    EXPECT_EQ(buffer.read_begin()[0], 3);

    buffer.normalize();
    EXPECT_EQ(buffer.readable_size(), 2u);
    EXPECT_EQ(buffer.read_begin()[0], 3);
}

TEST(BufferTest, WriteGrowsWhenFull) {
    Buffer buffer(4);
    const std::vector<uint8_t> big(64, 0x5A);
    buffer.write(const_cast<uint8_t *>(big.data()), big.size());
    EXPECT_EQ(buffer.readable_size(), 64u);
    EXPECT_EQ(buffer.read_begin()[63], 0x5A);
}
