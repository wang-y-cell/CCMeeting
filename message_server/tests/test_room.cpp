#include <gtest/gtest.h>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif
#include <boost/asio.hpp>

#include <cstdint>
#include <memory>
#include <vector>

#include "meeting/room_manager.h"
#include "network/connection.h"

namespace {

using boost::asio::ip::tcp;

std::shared_ptr<network::Connection> make_connection(
    boost::asio::io_context &io, bool open = true) {
    tcp::socket socket(io);
    if (open) {
        socket.open(tcp::v4());
    }
    return std::make_shared<network::Connection>(io, std::move(socket));
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

protocol::Packet make_room_settings_packet(uint32_t flags) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::RoomSettings;
    const uint32_t net_flags = htonl(flags);
    packet.payload.resize(sizeof(net_flags));
    std::memcpy(packet.payload.data(), &net_flags, sizeof(net_flags));
    return packet;
}

protocol::Packet make_text_packet(uint64_t user_id, const std::string &text) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::TextSend;
    packet.user_id = user_id;
    packet.payload.assign(text.begin(), text.end());
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

    const uint32_t flags = 0x3;
    room->handle_packet(owner, make_room_settings_packet(flags));
    EXPECT_EQ(room->settings(), flags);
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

    const uint32_t before = room->settings();
    room->handle_packet(member, make_room_settings_packet(0x7));
    EXPECT_EQ(room->settings(), before);
}

TEST(RoomTest, PacketFromNonParticipantIsIgnored) {
    boost::asio::io_context io;
    meeting::RoomManager manager(make_config(), io);
    auto owner = make_connection(io);
    auto outsider = make_connection(io);

    const auto room_id = manager.create_room(owner, make_options(), 1);
    ASSERT_TRUE(room_id.has_value());
    auto room = manager.get_room(*room_id);

    room->handle_packet(outsider, make_text_packet(99, "hack"));
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

    room->handle_packet(owner, make_text_packet(1, "after close"));
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
