#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "meeting/room_manager.h"
#include "network/connection.h"

namespace {

using boost::asio::ip::tcp;
using json = nlohmann::json;

std::shared_ptr<network::Connection> make_connection(
    boost::asio::io_context &io, bool open = true) {
    tcp::socket socket(io);
    if (open) {
        socket.open(tcp::v4());
    }
    return std::make_shared<network::Connection>(std::move(socket));
}

config::ServerConfig make_config(std::size_t max_rooms = 8,
                                 std::size_t max_per_room = 8) {
    config::ServerConfig config;
    config.max_rooms = max_rooms;
    config.max_participants_per_room = max_per_room;
    return config;
}

meeting::RoomOptions make_options(std::size_t max_participants = 4,
                                  uint32_t duration_minutes = 0) {
    meeting::RoomOptions options;
    options.max_participants = max_participants;
    options.duration_minutes = duration_minutes;
    return options;
}

protocol::Packet make_room_settings_packet(bool allow_record,
                                           bool allow_member_share) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::RoomSettings;
    packet.data = json{{"allow_record", allow_record},
                       {"allow_member_share", allow_member_share}};
    return packet;
}

protocol::Packet make_text_packet(const std::string &text) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::TextSend;
    packet.data = json{{"text", text}};
    return packet;
}

protocol::Packet make_exit_packet() {
    protocol::Packet packet;
    packet.type = protocol::MessageType::ExitMeeting;
    return packet;
}

protocol::Packet make_profile_packet(const std::string &nickname,
                                     const std::string &avatar) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::UserProfile;
    packet.data = json{{"nickname", nickname}, {"avatar", avatar}};
    return packet;
}

}  // namespace

TEST(RoomManagerTest, CreateRoomAddsOwnerAndRegistersRoom) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(8, 8), io);
    auto owner = make_connection(io);

    const auto room_id =
        manager.create_room(owner, make_options(2, 0), 1001);
    ASSERT_TRUE(room_id.has_value());

    EXPECT_EQ(manager.room_count(), 1u);
    auto room = manager.get_room(*room_id);
    ASSERT_NE(room, nullptr);
    EXPECT_EQ(room->id(), *room_id);
    EXPECT_EQ(room->participant_count(), 1u);   // 房主已加入
    EXPECT_EQ(room->max_participants(), 2u);
    EXPECT_EQ(room->duration_minutes(), 0u);
    EXPECT_FALSE(room->is_closed());
    // 默认允许录制、允许成员共享
    EXPECT_TRUE(room->allow_record());
    EXPECT_TRUE(room->allow_member_share());
}

TEST(RoomManagerTest, CreateRoomRejectsClosedOwner) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io, /*open=*/false);

    EXPECT_FALSE(manager.create_room(owner, make_options(), 1001).has_value());
    EXPECT_EQ(manager.room_count(), 0u);
}

TEST(RoomManagerTest, CreateRoomRejectsWhenMaxRoomsReached) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(/*max_rooms=*/1, 4), io);
    auto owner1 = make_connection(io);
    auto owner2 = make_connection(io);

    ASSERT_TRUE(manager.create_room(owner1, make_options(), 1).has_value());
    EXPECT_FALSE(manager.create_room(owner2, make_options(), 2).has_value());
    EXPECT_EQ(manager.room_count(), 1u);
}

TEST(RoomManagerTest, JoinExistingRoomSucceeds) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(4, 0), 1);
    ASSERT_TRUE(room_id.has_value());

    EXPECT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);
    EXPECT_EQ(manager.get_room(*room_id)->participant_count(), 2u);
}

TEST(RoomManagerTest, JoinMissingRoomReturnsNotFound) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto conn = make_connection(io);

    EXPECT_EQ(manager.join_room(123456, conn, 1),
              meeting::RoomManager::JoinResult::NotFound);
}

TEST(RoomManagerTest, JoinFullRoomReturnsFull) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);
    auto extra = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(2, 0), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);

    EXPECT_EQ(manager.join_room(*room_id, extra, 3),
              meeting::RoomManager::JoinResult::Full);
    EXPECT_EQ(manager.get_room(*room_id)->participant_count(), 2u);
}

TEST(RoomManagerTest, JoinClosedRoomReturnsClosed) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    // 房主离开 = 关闭房间（close_room 是私有实现，通过语义入口触发）
    manager.get_room(*room_id)->remove_participant(owner->id());

    EXPECT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Closed);
}

TEST(RoomTest, OwnerLeavingClosesRoomAndInvokesCallback) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(4, 0), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);

    auto room = manager.get_room(*room_id);
    uint32_t closed_id = 0;
    int close_calls = 0;
    room->set_close_callback([&](uint32_t id) {
        closed_id = id;
        ++close_calls;
    });

    room->remove_participant(owner->id());

    EXPECT_TRUE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 0u);
    EXPECT_EQ(close_calls, 1);
    EXPECT_EQ(closed_id, *room_id);
}

TEST(RoomTest, MemberLeavingKeepsRoomOpen) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(4, 0), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);

    auto room = manager.get_room(*room_id);
    room->remove_participant(member->id());

    EXPECT_FALSE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 1u);
}

TEST(RoomTest, OwnerCanUpdateRoomSettings) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    auto room = manager.get_room(*room_id);

    room->handle_packet(owner, make_room_settings_packet(false, true));
    EXPECT_FALSE(room->allow_record());
    EXPECT_TRUE(room->allow_member_share());

    // 两个开关互相独立，可再次取反
    room->handle_packet(owner, make_room_settings_packet(true, false));
    EXPECT_TRUE(room->allow_record());
    EXPECT_FALSE(room->allow_member_share());
}

TEST(RoomTest, NonOwnerCannotUpdateRoomSettings) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);
    auto room = manager.get_room(*room_id);

    room->handle_packet(owner, make_room_settings_packet(false, false));
    ASSERT_FALSE(room->allow_record());
    ASSERT_FALSE(room->allow_member_share());

    room->handle_packet(member, make_room_settings_packet(true, true));
    EXPECT_FALSE(room->allow_record());
    EXPECT_FALSE(room->allow_member_share());
}

TEST(RoomTest, MemberExitMeetingRemovesParticipantWithoutClosingRoom) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(4, 0), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);
    auto room = manager.get_room(*room_id);

    room->handle_packet(member, make_exit_packet());

    EXPECT_FALSE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 1u);

    // 重复 exit_meeting 不应再改变状态（remove_participant 幂等）
    room->handle_packet(member, make_exit_packet());
    EXPECT_FALSE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 1u);
}

TEST(RoomTest, OwnerExitMeetingClosesRoom) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(4, 0), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);
    auto room = manager.get_room(*room_id);

    int close_calls = 0;
    room->set_close_callback([&](uint32_t) { ++close_calls; });
    room->handle_packet(owner, make_exit_packet());

    EXPECT_TRUE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 0u);
    EXPECT_EQ(close_calls, 1);
}

TEST(RoomTest, UserProfileOnlyChangesDisplayState) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto member = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    ASSERT_EQ(manager.join_room(*room_id, member, 2),
              meeting::RoomManager::JoinResult::Ok);
    auto room = manager.get_room(*room_id);

    room->handle_packet(member, make_profile_packet("Alice", "http://a/b.png"));
    // user_profile 不改变成员数量/身份
    EXPECT_EQ(room->participant_count(), 2u);
    EXPECT_FALSE(room->is_closed());
}

TEST(RoomTest, PacketFromNonParticipantIsIgnored) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto outsider = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    auto room = manager.get_room(*room_id);

    room->handle_packet(outsider, make_text_packet("hack"));
    EXPECT_EQ(room->participant_count(), 1u);
    EXPECT_FALSE(room->is_closed());
}

TEST(RoomTest, ClosedRoomIgnoresPackets) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    auto room = manager.get_room(*room_id);
    room->remove_participant(owner->id());   // 房主离开 -> 关房

    room->handle_packet(owner, make_text_packet("after close"));
    EXPECT_TRUE(room->is_closed());
    EXPECT_EQ(room->participant_count(), 0u);
}

TEST(RoomManagerTest, RemoveRoomDropsRegistration) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    EXPECT_EQ(manager.room_count(), 1u);

    manager.remove_room(*room_id);
    EXPECT_EQ(manager.room_count(), 0u);
    EXPECT_EQ(manager.get_room(*room_id), nullptr);
}
