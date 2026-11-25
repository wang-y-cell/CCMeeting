#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "network/buffer.h"
#include "network/server.h"
#include "protocol/codec.h"
#include "service/meeting_service.h"

namespace {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using boost::asio::ip::tcp;
using json = nlohmann::json;

// 由 HMAC-SHA256("test-secret", "77777:4102444800") 生成，exp 远在未来
constexpr const char *kOwnerToken =
    "Nzc3Nzc6NDEwMjQ0NDgwMA.w2hqevjeivApqHx4ld8dID6bJ04Ddnf0TTWrJVJMxdk";
constexpr const char *kMemberToken =
    "Nzc3Nzg6NDEwMjQ0NDgwMA.6CohwONwUqxEdU7huou_aUL5zQsV0ol_ZjwTS6ReELM";

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

        // v2：一条 WS 消息就是一个 JSON，不需要任何长度前缀
        Buffer frame;
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

protocol::Packet make_create_packet(uint32_t max_participants,
                                    uint32_t duration_minutes,
                                    const std::string &token = std::string()) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::CreateMeeting;
    packet.auth_token = token;
    packet.data = json{{"max_participants", max_participants},
                       {"duration_minutes", duration_minutes}};
    return packet;
}

protocol::Packet make_join_packet(uint32_t room_id,
                                  const std::string &token = std::string()) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::JoinMeeting;
    packet.auth_token = token;
    packet.data = json{{"room_id", room_id}};
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

protocol::Packet make_close_camera_packet() {
    protocol::Packet packet;
    packet.type = protocol::MessageType::CloseCamera;
    return packet;
}

protocol::Packet make_profile_packet(const std::string &nickname,
                                     const std::string &avatar) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::UserProfile;
    packet.data = json{{"nickname", nickname}, {"avatar", avatar}};
    return packet;
}

protocol::Packet make_room_settings_packet(bool allow_record,
                                           bool allow_member_share) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::RoomSettings;
    packet.data = json{{"allow_record", allow_record},
                       {"allow_member_share", allow_member_share}};
    return packet;
}

uint64_t read_uint(const protocol::Packet &packet, const char *key) {
    if (!packet.data.is_object()) {
        return 0;
    }
    const auto it = packet.data.find(key);
    if (it == packet.data.end() || !it->is_number_integer()) {
        return 0;
    }
    if (it->is_number_unsigned()) {
        return it->get<uint64_t>();
    }
    const auto value = it->get<int64_t>();
    return value < 0 ? 0 : static_cast<uint64_t>(value);
}

uint32_t read_room_id(const protocol::Packet &packet) {
    return static_cast<uint32_t>(read_uint(packet, "room_id"));
}

uint64_t read_user_id(const protocol::Packet &packet) {
    return read_uint(packet, "user_id");
}

/// 起一个真实的服务端（端口 0 由系统分配），跑在后台线程
struct ServerFixture {
    static config::ServerConfig make_config(bool require_auth = false) {
        config::ServerConfig config;
        config.max_rooms = 8;
        config.max_participants_per_room = 8;
        config.require_auth = require_auth;
        if (require_auth) {
            config.token_secret = "test-secret";
        }
        return config;
    }

    ServerFixture() : ServerFixture(make_config()) {}

    explicit ServerFixture(config::ServerConfig config)
        : server(io, 0),
          service(std::move(config), io) {
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

    boost::asio::io_context io;
    network::Server server;
    service::MeetingService service;
    std::thread thread;
};

}  // namespace

TEST(MeetingServiceTest, CreateMeetingReturnsRoomId) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_create_packet(4, 60));
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::CreateMeetingResponse);
    EXPECT_EQ(response.code, 0);
    EXPECT_EQ(response.message, "ok");
    EXPECT_NE(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, SecondClientJoinReceivesPartnerJoin) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    const protocol::Packet join_response = member.read_packet();
    EXPECT_EQ(join_response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(join_response.code, 0);
    EXPECT_EQ(read_room_id(join_response), room_id);

    // 房主应收到"有人加入"，data.user_id 为服务端分配的身份
    const protocol::Packet joined = owner.read_packet();
    EXPECT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    EXPECT_NE(read_user_id(joined), 0u);
}

TEST(MeetingServiceTest, JoinMissingRoomIsRejectedWithMinusTwo) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_join_packet(999999));
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(response.code, -2);   // 房间不存在
    EXPECT_EQ(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, JoinWithoutRoomIdIsRejectedWithMinusTwo) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    protocol::Packet join;
    join.type = protocol::MessageType::JoinMeeting;
    client.send_packet(join);   // data 为空

    const protocol::Packet response = client.read_packet();
    EXPECT_EQ(response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(response.code, -2);
    EXPECT_EQ(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, JoinFullRoomIsRejectedWithMinusOne) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());
    WsClient extra(client_io, fixture.port());

    owner.send_packet(make_create_packet(2, 60));   // 上限 2 人
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);

    extra.send_packet(make_join_packet(room_id));
    const protocol::Packet rejected = extra.read_packet();
    EXPECT_EQ(rejected.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(rejected.code, -1);   // 满员
    EXPECT_EQ(read_room_id(rejected), 0u);
}

TEST(MeetingServiceTest, ExitMeetingBroadcastsPartnerExitAndAllowsRejoin) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);
    ASSERT_NE(member_user_id, 0u);

    // 新人入会后还会收到成员快照与房间开关，这里先消费掉，后续读到的才是 exit_meeting 相关消息
    ASSERT_EQ(member.read_packet().type, protocol::MessageType::PartnerJoin2);
    ASSERT_EQ(member.read_packet().type,
              protocol::MessageType::RoomSettingsNotify);

    // exit_meeting：服务端把该成员移出房间，并向其余成员广播 partner_exit
    member.send_packet(make_exit_packet());
    const protocol::Packet exit_notice = owner.read_packet();
    EXPECT_EQ(exit_notice.type, protocol::MessageType::PartnerExit);
    EXPECT_EQ(read_user_id(exit_notice), member_user_id);

    // 退出后该连接回到大厅，可以重新加入
    member.send_packet(make_join_packet(room_id));
    const protocol::Packet rejoin = member.read_packet();
    EXPECT_EQ(rejoin.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(rejoin.code, 0);
    EXPECT_EQ(read_room_id(rejoin), room_id);

    const protocol::Packet joined_again = owner.read_packet();
    EXPECT_EQ(joined_again.type, protocol::MessageType::PartnerJoin);
    EXPECT_EQ(read_user_id(joined_again), member_user_id);
}

TEST(MeetingServiceTest, MemberDisconnectNotifiesOwnerWithPartnerExit) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);

    member.close();

    const protocol::Packet exit_notice = owner.read_packet();
    EXPECT_EQ(exit_notice.type, protocol::MessageType::PartnerExit);
    EXPECT_EQ(read_user_id(exit_notice), member_user_id);
}

TEST(MeetingServiceTest, AuthRequiredRejectsCreateWithoutToken) {
    ServerFixture fixture(ServerFixture::make_config(/*require_auth=*/true));
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_create_packet(4, 60));   // 不带 token
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::CreateMeetingResponse);
    EXPECT_EQ(response.code, -3);   // 鉴权失败
    EXPECT_EQ(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, AuthRequiredRejectsJoinWithoutToken) {
    ServerFixture fixture(ServerFixture::make_config(/*require_auth=*/true));
    boost::asio::io_context client_io;
    WsClient client(client_io, fixture.port());

    client.send_packet(make_join_packet(123456));   // 不带 token
    const protocol::Packet response = client.read_packet();

    EXPECT_EQ(response.type, protocol::MessageType::JoinMeetingResponse);
    EXPECT_EQ(response.code, -3);
    EXPECT_EQ(read_room_id(response), 0u);
}

TEST(MeetingServiceTest, AuthUsesTokenUserId) {
    ServerFixture fixture(ServerFixture::make_config(/*require_auth=*/true));
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60, kOwnerToken));
    const protocol::Packet create_response = owner.read_packet();
    EXPECT_EQ(create_response.code, 0);
    const uint32_t room_id = read_room_id(create_response);
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id, kMemberToken));
    EXPECT_EQ(member.read_packet().code, 0);

    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    EXPECT_EQ(read_user_id(joined), 77778u);   // 来自 token，而不是客户端字段
}

TEST(MeetingServiceTest, TokenUserIdIsPreferredWhenAuthDisabled) {
    // 鉴权未开启：带 token 时仍优先使用 token 里的 user_id，便于客户端在联调环境
    // 与自己的账号 id 对齐；没有 token 的连接才退回连接 id。
    auto config = ServerFixture::make_config(/*require_auth=*/false);
    config.token_secret = "test-secret";
    ServerFixture fixture(std::move(config));
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60, kOwnerToken));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id, kMemberToken));
    ASSERT_EQ(member.read_packet().code, 0);

    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    EXPECT_EQ(read_user_id(joined), 77778u);
}

TEST(MeetingServiceTest, CloseCameraIsBroadcastWithUserId) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);

    // close_camera 上行是 DataEmpty，下行广播带 user_id
    member.send_packet(make_close_camera_packet());

    const protocol::Packet notify = owner.read_packet();
    EXPECT_EQ(notify.type, protocol::MessageType::CloseCamera);
    EXPECT_EQ(read_user_id(notify), member_user_id);
}

TEST(MeetingServiceTest, LargeTextPayloadIsForwardedIntact) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);

    // 256KB 文本：验证 JSON data 不被截断
    std::string text(256 * 1024, 'x');
    text.front() = 'S';
    text.back() = 'E';
    member.send_packet(make_text_packet(text));

    const protocol::Packet received = owner.read_packet();
    // 服务端把 text_send 转成 text_recv 再广播
    EXPECT_EQ(received.type, protocol::MessageType::TextRecv);
    EXPECT_EQ(read_user_id(received), member_user_id);
    EXPECT_EQ(received.data["text"].get<std::string>(), text);
}

TEST(MeetingServiceTest, UserProfileIsBroadcastWithUserId) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);

    member.send_packet(make_profile_packet("Alice", "http://example/a.png"));
    const protocol::Packet profile = owner.read_packet();

    EXPECT_EQ(profile.type, protocol::MessageType::UserProfile);
    EXPECT_EQ(read_user_id(profile), member_user_id);
    EXPECT_EQ(profile.data["nickname"].get<std::string>(), "Alice");
    EXPECT_EQ(profile.data["avatar"].get<std::string>(), "http://example/a.png");
}

TEST(MeetingServiceTest, RoomSettingsNotifyReachesAllMembers) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    ASSERT_EQ(owner.read_packet().type, protocol::MessageType::PartnerJoin);

    // 新人入会先收到成员快照 + 房间开关初始值
    const protocol::Packet snapshot = member.read_packet();
    EXPECT_EQ(snapshot.type, protocol::MessageType::PartnerJoin2);
    ASSERT_TRUE(snapshot.data["user_ids"].is_array());
    EXPECT_EQ(snapshot.data["user_ids"].size(), 1u);

    const protocol::Packet initial = member.read_packet();
    EXPECT_EQ(initial.type, protocol::MessageType::RoomSettingsNotify);
    EXPECT_TRUE(initial.data["allow_record"].get<bool>());
    EXPECT_TRUE(initial.data["allow_member_share"].get<bool>());

    // 房主改开关：广播给全员（含发起者）
    owner.send_packet(make_room_settings_packet(false, true));

    const protocol::Packet owner_notify = owner.read_packet();
    EXPECT_EQ(owner_notify.type, protocol::MessageType::RoomSettingsNotify);
    EXPECT_FALSE(owner_notify.data["allow_record"].get<bool>());
    EXPECT_TRUE(owner_notify.data["allow_member_share"].get<bool>());

    const protocol::Packet member_notify = member.read_packet();
    EXPECT_EQ(member_notify.type, protocol::MessageType::RoomSettingsNotify);
    EXPECT_FALSE(member_notify.data["allow_record"].get<bool>());
    EXPECT_TRUE(member_notify.data["allow_member_share"].get<bool>());
}

TEST(MeetingServiceTest, BurstOfMessagesArrivesInOrder) {
    ServerFixture fixture;
    boost::asio::io_context client_io;
    WsClient owner(client_io, fixture.port());
    WsClient member(client_io, fixture.port());

    owner.send_packet(make_create_packet(4, 60));
    const uint32_t room_id = read_room_id(owner.read_packet());
    ASSERT_NE(room_id, 0u);

    member.send_packet(make_join_packet(room_id));
    ASSERT_EQ(read_room_id(member.read_packet()), room_id);
    const protocol::Packet joined = owner.read_packet();
    ASSERT_EQ(joined.type, protocol::MessageType::PartnerJoin);
    const uint64_t member_user_id = read_user_id(joined);

    constexpr int kCount = 30;
    for (int i = 0; i < kCount; ++i) {
        member.send_packet(make_text_packet("msg-" + std::to_string(i)));
    }

    for (int i = 0; i < kCount; ++i) {
        const protocol::Packet received = owner.read_packet();
        ASSERT_EQ(received.type, protocol::MessageType::TextRecv);
        EXPECT_EQ(read_user_id(received), member_user_id);
        EXPECT_EQ(received.data["text"].get<std::string>(),
                  "msg-" + std::to_string(i))
            << "index " << i;
    }
}
