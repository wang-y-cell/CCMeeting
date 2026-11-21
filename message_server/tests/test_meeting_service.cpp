#include <gtest/gtest.h>

#ifdef _WIN32
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "network/buffer.h"
#include "network/server.h"
#include "protocol/codec.h"
#include "service/meeting_service.h"

namespace {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using boost::asio::ip::tcp;

/// 同步 WebSocket 客户端（测试用）：连接 → 握手 → 发一帧 → 收一帧
class WsClient {
public:
    WsClient(boost::asio::io_context &io, uint16_t port) : ws_(io) {
        tcp::resolver resolver(io);
        const auto results =
            resolver.resolve(std::string("127.0.0.1"), std::to_string(port));
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));
        beast::get_lowest_layer(ws_).connect(results);
        ws_.handshake("127.0.0.1:" + std::to_string(port), "/");
    }

    void send_packet(const protocol::Packet &packet) {
        const auto encoded = protocol::Codec::encode(packet);
        const std::string text(encoded.begin(), encoded.end());
        ws_.write(boost::asio::buffer(text));
    }

    /// @return 服务端发来的一条 JSON 文本；超时/断开抛异常
    protocol::Packet read_packet() {
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));
        beast::flat_buffer buffer;
        ws_.read(buffer);
        const std::string text = beast::buffers_to_string(buffer.data());

        // 按 Codec 约定补上 [uint32 BE 长度][JSON] 再解码
        Buffer frame;
        const uint32_t size = static_cast<uint32_t>(text.size());
        uint8_t head[4] = {static_cast<uint8_t>((size >> 24) & 0xFF),
                           static_cast<uint8_t>((size >> 16) & 0xFF),
                           static_cast<uint8_t>((size >> 8) & 0xFF),
                           static_cast<uint8_t>(size & 0xFF)};
        frame.write(head, sizeof(head));
        frame.write(reinterpret_cast<uint8_t *>(const_cast<char *>(text.data())),
                    text.size());

        protocol::Packet packet;
        EXPECT_EQ(protocol::Codec::try_decode(frame, packet),
                  protocol::DecodeStatus::Ok);
        return packet;
    }

    void close() {
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
    }

private:
    websocket::stream<beast::tcp_stream> ws_;
};

protocol::Packet make_create_packet(uint64_t user_id, uint32_t max_participants,
                                    uint32_t duration_minutes) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::CreateMeeting;
    packet.user_id = user_id;
    packet.payload.resize(sizeof(uint32_t) * 2);
    const uint32_t max_net = htonl(max_participants);
    const uint32_t duration_net = htonl(duration_minutes);
    std::memcpy(packet.payload.data(), &max_net, sizeof(max_net));
    std::memcpy(packet.payload.data() + sizeof(max_net), &duration_net,
                sizeof(duration_net));
    return packet;
}

protocol::Packet make_join_packet(uint64_t user_id, uint32_t room_id) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::JoinMeeting;
    packet.user_id = user_id;
    const uint32_t room_net = htonl(room_id);
    packet.payload.resize(sizeof(room_net));
    std::memcpy(packet.payload.data(), &room_net, sizeof(room_net));
    return packet;
}

uint32_t read_room_id(const protocol::Packet &packet) {
    if (packet.payload.size() < sizeof(uint32_t)) {
        return 0;
    }
    uint32_t room_net = 0;
    std::memcpy(&room_net, packet.payload.data(), sizeof(room_net));
    return ntohl(room_net);
}

/// 起一个真实的服务端（端口 0 由系统分配），跑在后台线程
struct ServerFixture {
    boost::asio::io_context io;
    network::Server server;
    service::MeetingService service;
    std::thread thread;

    ServerFixture()
        : server(io, 0),
          service(make_config(), io) {
        service.bind_server(server);
        server.start();
        thread = std::thread([this]() { io.run(); });
    }

    ~ServerFixture() {
        io.stop();
        if (thread.joinable()) {
            thread.join();
        }
    }

    uint16_t port() const { return server.local_port(); }

    static config::ServerConfig make_config() {
        config::ServerConfig config;
        config.max_rooms = 8;
        config.max_participants_per_room = 8;
        return config;
    }
};

}  // namespace

TEST(MeetingServiceTest, CreateMeetingReturnsRoomId) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_create_packet(1001, 4, 60));
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::CreateMeetingResponse);
    EXPECT_NE(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, SecondClientJoinReceivesPartnerJoin) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(2001, 4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(2002, room_id));
    const protocol::Packet join_response = member.read_packet();
    EXPECT_EQ(join_response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(read_room_id(join_response), room_id);

    // 房主应收到"有人加入"
    const protocol::Packet joined = owner.read_packet();
    EXPECT_EQ(joined.type, protocol::MessageType::PartnerJoin);
}

TEST(MeetingServiceTest, JoinMissingRoomIsRejected) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_join_packet(3001, 999999));
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(read_room_id(response), 0u);   // 0 = 房间不存在
}

TEST(MeetingServiceTest, JoinFullRoomIsRejectedWithMinusOne) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());
    WsClient extra(client_io, fixture.port());

    owner.send_packet(make_create_packet(4001, 2, 60));   // 上限 2 人
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(4002, room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);

    extra.send_packet(make_join_packet(4003, room_id));
    const protocol::Packet rejected = extra.read_packet();
    EXPECT_EQ(rejected.type, protocol::MessageType::JoinMeetingResponse);
    uint32_t value_net = 0;
    ASSERT_GE(rejected.payload.size(), sizeof(value_net));
    std::memcpy(&value_net, rejected.payload.data(), sizeof(value_net));
    EXPECT_EQ(ntohl(value_net), 0xFFFFFFFFu);   // -1 = 满员
}

TEST(MeetingServiceTest, MemberDisconnectNotifiesOwnerWithPartnerExit) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(5001, 4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(5002, room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    ASSERT_EQ(owner.read_packet().type, protocol::MessageType::PartnerJoin);

    member.close();

    const protocol::Packet exit = owner.read_packet();
    EXPECT_EQ(exit.type, protocol::MessageType::PartnerExit);
    EXPECT_EQ(exit.user_id, 5002u);
}

TEST(MeetingServiceTest, UserIdZeroIsRejectedAndConnectionClosed) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_create_packet(0, 4, 60));

    bool closed = false;
    try {
        client.read_packet();
    } catch (const std::exception &) {
        closed = true;
    }
    EXPECT_TRUE(closed);
}
TEST(MeetingServiceTest, LargeTextPayloadIsForwardedIntact) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(6001, 4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(6002, room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    ASSERT_EQ(owner.read_packet().type, protocol::MessageType::PartnerJoin);

    // 256KB 文本：验证跨 WS 帧/Codec 的二进制 payload 不被截断
    protocol::Packet text;
    text.type = protocol::MessageType::TextSend;
    text.user_id = 6002;
    text.payload.assign(256 * 1024, 'x');
    text.payload[0] = 'S';
    text.payload[text.payload.size() - 1] = 'E';
    member.send_packet(text);

    const protocol::Packet received = owner.read_packet();
    // 服务端把 TextSend 转成 TextRecv 再广播（客户端视角）
    EXPECT_EQ(received.type, protocol::MessageType::TextRecv);
    EXPECT_EQ(received.user_id, 6002u);
    EXPECT_EQ(received.payload.size(), text.payload.size());
    EXPECT_EQ(received.payload, text.payload);
}

TEST(MeetingServiceTest, BurstOfMessagesArrivesInOrder) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(7001, 4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(7002, room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    ASSERT_EQ(owner.read_packet().type, protocol::MessageType::PartnerJoin);

    constexpr int kCount = 30;
    for (int i = 0; i < kCount; ++i) {
        protocol::Packet text;
        text.type = protocol::MessageType::TextSend;
        text.user_id = 7002;
        const std::string body = "msg-" + std::to_string(i);
        text.payload.assign(body.begin(), body.end());
        member.send_packet(text);
    }

    for (int i = 0; i < kCount; ++i) {
        const protocol::Packet received = owner.read_packet();
        ASSERT_EQ(received.type, protocol::MessageType::TextRecv);
        const std::string body(received.payload.begin(), received.payload.end());
        EXPECT_EQ(body, "msg-" + std::to_string(i)) << "index " << i;
    }
}