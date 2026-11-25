#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "network/buffer.h"
#include "protocol/codec.h"
#include "protocol/packet.h"

namespace {

using json = nlohmann::json;

protocol::Packet make_packet(protocol::MessageType type,
                             json data = json::object()) {
    protocol::Packet packet;
    packet.type = type;
    packet.data = std::move(data);
    return packet;
}

void append_bytes(Buffer &buffer, const std::vector<uint8_t> &bytes) {
    if (bytes.empty()) {
        return;
    }
    buffer.write(bytes.data(), bytes.size());
}

void append_text(Buffer &buffer, const std::string &text) {
    if (text.empty()) {
        return;
    }
    buffer.write(reinterpret_cast<const uint8_t *>(text.data()), text.size());
}

json encode_to_json(const protocol::Packet &packet) {
    const auto bytes = protocol::Codec::encode(packet);
    return json::parse(std::string(bytes.begin(), bytes.end()));
}

protocol::DecodeStatus decode_text(const std::string &text,
                                   protocol::Packet &packet) {
    Buffer buffer;
    append_text(buffer, text);
    return protocol::Codec::try_decode(buffer, packet);
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


TEST(CodecEncode, EnvelopeCarriesTypeCodeMessageData) {
    protocol::Packet packet =
        make_packet(protocol::MessageType::TextSend, json{{"text", "hello 世界"}});
    packet.auth_token = "tok-123";

    const json document = encode_to_json(packet);
    EXPECT_EQ(document["t"].get<std::string>(), "text_send");
    EXPECT_EQ(document["code"], 0);
    EXPECT_EQ(document["message"].get<std::string>(), "ok");
    EXPECT_EQ(document["token"].get<std::string>(), "tok-123");
    ASSERT_TRUE(document["data"].is_object());
    EXPECT_EQ(document["data"]["text"].get<std::string>(), "hello 世界");
}

TEST(CodecEncode, LegacyFieldsAreAbsent) {
    const json document = encode_to_json(
        make_packet(protocol::MessageType::TextSend, json{{"text", "x"}}));
    EXPECT_FALSE(document.contains("p"));
    EXPECT_FALSE(document.contains("u"));
    EXPECT_FALSE(document.contains("s"));
    EXPECT_FALSE(document.contains("a"));
    EXPECT_FALSE(document.contains("token"));  // 空 token 不写出
}

TEST(CodecEncode, DownstreamCarriesStatus) {
    protocol::Packet packet =
        make_packet(protocol::MessageType::JoinMeetingResponse,
                    json{{"room_id", 0}});
    packet.code = -2;
    packet.message = "room not found";

    const json document = encode_to_json(packet);
    EXPECT_EQ(document["code"], -2);
    EXPECT_EQ(document["message"].get<std::string>(), "room not found");
    EXPECT_EQ(document["data"]["room_id"], 0);
}

TEST(CodecEncode, DataObjectIsAlwaysPresent) {
    // 默认 data 为空对象，不应是 null
    const json document =
        encode_to_json(make_packet(protocol::MessageType::ExitMeeting));
    ASSERT_TRUE(document.contains("data"));
    EXPECT_TRUE(document["data"].is_object());
    EXPECT_TRUE(document["data"].empty());
}

TEST(CodecDecode, UpstreamTextMessage) {
    protocol::Packet packet;
    ASSERT_EQ(decode_text(
                  R"({"t":"text_send","token":"tk","data":{"text":"hi"}})",
                  packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::TextSend);
    EXPECT_EQ(packet.auth_token, "tk");
    ASSERT_TRUE(packet.data.is_object());
    EXPECT_EQ(packet.data["text"].get<std::string>(), "hi");
    EXPECT_EQ(packet.code, 0);
    EXPECT_EQ(packet.message, "ok");
}

TEST(CodecDecode, MissingDataBecomesEmptyObject) {
    protocol::Packet packet;
    ASSERT_EQ(decode_text(R"({"t":"exit_meeting"})", packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::ExitMeeting);
    ASSERT_TRUE(packet.data.is_object());
    EXPECT_TRUE(packet.data.empty());
}

TEST(CodecDecode, DownstreamStatusIsParsed) {
    protocol::Packet packet;
    ASSERT_EQ(decode_text(
                  R"({"t":"join_meeting_response","code":-1,"message":"room is full","data":{"room_id":0}})",
                  packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.code, -1);
    EXPECT_EQ(packet.message, "room is full");
    EXPECT_EQ(packet.data["room_id"], 0);
}

TEST(CodecDecode, LegacyFieldsAreIgnored) {
    protocol::Packet packet;
    ASSERT_EQ(decode_text(
                  R"({"t":"text_send","u":42,"s":"legacy","a":"old","p":"AA=="})",
                  packet),
              protocol::DecodeStatus::Ok);
    EXPECT_TRUE(packet.auth_token.empty());
    EXPECT_TRUE(packet.data.empty());
}

TEST(CodecRoundTrip, RoomSettingsBooleans) {
    protocol::Packet outbound = make_packet(
        protocol::MessageType::RoomSettings,
        json{{"allow_record", false}, {"allow_member_share", true}});

    Buffer buffer;
    append_bytes(buffer, protocol::Codec::encode(outbound));

    protocol::Packet inbound;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, inbound),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(inbound.type, protocol::MessageType::RoomSettings);
    EXPECT_EQ(inbound.data["allow_record"], false);
    EXPECT_EQ(inbound.data["allow_member_share"], true);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecRoundTrip, UserIdsAndTextSurvive) {
    protocol::Packet outbound = make_packet(
        protocol::MessageType::TextRecv,
        json{{"user_id", 5002}, {"text", "hi there"}});
    outbound.code = 0;

    Buffer buffer;
    append_bytes(buffer, protocol::Codec::encode(outbound));

    protocol::Packet inbound;
    ASSERT_EQ(protocol::Codec::try_decode(buffer, inbound),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(inbound.type, protocol::MessageType::TextRecv);
    EXPECT_EQ(inbound.data["user_id"], 5002);
    EXPECT_EQ(inbound.data["text"].get<std::string>(), "hi there");
}

TEST(CodecDecode, NeedMoreForEmptyBuffer) {
    Buffer buffer;
    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::NeedMore);
}

TEST(CodecDecode, MalformedJsonIsInvalidAndConsumed) {
    Buffer buffer;
    append_text(buffer, "{not valid json");

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
    EXPECT_EQ(buffer.readable_size(), 0u);   // 坏消息被丢弃，避免卡死
}

TEST(CodecDecode, NonObjectJsonIsInvalid) {
    Buffer buffer;
    append_text(buffer, "[1,2,3]");

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecDecode, UnknownTypeIsInvalid) {
    Buffer buffer;
    append_text(buffer, R"({"t":"no_such_type","data":{}})");

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecDecode, NonStringTypeIsInvalid) {
    Buffer buffer;
    append_text(buffer, R"({"t":5})");

    protocol::Packet packet;
    EXPECT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Invalid);
}

TEST(CodecDecode, ReusedPacketDoesNotLeakPreviousFields) {
    protocol::Packet packet;
    Buffer buffer;

    append_bytes(buffer, protocol::Codec::encode(make_packet(
                             protocol::MessageType::TextSend,
                             json{{"text", "first"}})));
    packet.auth_token = "stale-token";
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.data["text"].get<std::string>(), "first");

    // 第二条消息不带 token / data：复用同一个 Packet 也不应残留旧值
    append_text(buffer, R"({"t":"exit_meeting"})");
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.type, protocol::MessageType::ExitMeeting);
    EXPECT_TRUE(packet.auth_token.empty());
    ASSERT_TRUE(packet.data.is_object());
    EXPECT_TRUE(packet.data.empty());
    EXPECT_EQ(buffer.readable_size(), 0u);
}

TEST(CodecDecode, ConsecutiveMessagesDecodeOneAtATime) {
    Buffer buffer;
    protocol::Packet packet;

    append_bytes(buffer, protocol::Codec::encode(make_packet(
                             protocol::MessageType::TextSend,
                             json{{"text", "a"}})));
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.data["text"].get<std::string>(), "a");

    append_bytes(buffer, protocol::Codec::encode(make_packet(
                             protocol::MessageType::TextSend,
                             json{{"text", "b"}})));
    ASSERT_EQ(protocol::Codec::try_decode(buffer, packet),
              protocol::DecodeStatus::Ok);
    EXPECT_EQ(packet.data["text"].get<std::string>(), "b");
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
    buffer.write(big.data(), big.size());
    EXPECT_EQ(buffer.readable_size(), 64u);
    EXPECT_EQ(buffer.read_begin()[63], 0x5A);
}
