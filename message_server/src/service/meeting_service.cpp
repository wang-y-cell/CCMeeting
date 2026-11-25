#include "service/meeting_service.h"

#include "protocol/token.h"

#include "protocol/codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace service {

namespace {

using json = nlohmann::json;

constexpr uint32_t kMinParticipants = 2;
constexpr uint32_t kMinDurationMinutes = 1;
constexpr uint32_t kMaxDurationMinutes = 24 * 60;
constexpr uint32_t kDefaultDurationMinutes = 60;

/// 从 JSON data 读取非负 uint32；缺失/类型不符/越界时返回 fallback。
/// 注意：协议字段是 uint32，不能把 uint64/int64 直接 static_cast 截断
/// （例如 room_id=0x100000001 会被截成 1，导致误入别的房间）。
uint32_t read_u32(const json& data, const char* key, uint32_t fallback) {
    if (!data.is_object()) {
        return fallback;
    }
    const auto it = data.find(key);
    if (it == data.end() || !it->is_number_integer()) {
        return fallback;
    }
    if (it->is_number_unsigned()) {
        const auto value = it->get<uint64_t>();
        if (value > std::numeric_limits<uint32_t>::max()) {
            return fallback;
        }
        return static_cast<uint32_t>(value);
    }
    const auto value = it->get<int64_t>();
    if (value < 0 ||
        value > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
        return fallback;
    }
    return static_cast<uint32_t>(value);
}

/**
 * @brief 确定上行业务身份
 *
 * v2 已删除 `u` 字段：优先用顶层 token 解出 user_id；
 * 没有 token（或鉴权未开启且 token 无效）时退回连接状态。
 * 鉴权开启时 token 缺失/无效视为鉴权失败。
 * @param auth_failed 输出：鉴权开启时 token 校验是否失败
 */
uint64_t resolve_user_id(const config::ServerConfig& config,
                         const protocol::Packet& packet,
                         network::Connection::Id connection_id,
                         bool& auth_failed) {
    auth_failed = false;

    if (!packet.auth_token.empty() && !config.token_secret.empty()) {
        uint64_t user_id = 0;
        if (protocol::token::verify(packet.auth_token, config.token_secret,
                                    user_id)) {
            return user_id;
        }
        if (config.require_auth) {
            auth_failed = true;
            return 0;
        }
    } else if (config.require_auth) {
        auth_failed = true;
        return 0;
    }

    // 鉴权关闭且没有可用 token（开发/联调）：连接本身即身份。
    // 这是已知退化路径：user_id 会与真实账号 id 混用，生产必须配置
    // CCMEETING_TOKEN_SECRET（main 启动时会对空 secret 告警）。
    return connection_id;
}

/// 下游 room 类响应：{"code":...,"message":"...","data":{"room_id":...}}
protocol::Packet make_room_response(protocol::MessageType type, int code,
                                    const std::string& message,
                                    uint32_t room_id) {
    protocol::Packet response;
    response.type = type;
    response.code = code;
    response.message = message;
    response.data = json{{"room_id", room_id}};
    return response;
}

meeting::RoomOptions parse_create_meeting_options(
    const protocol::Packet& packet, const config::ServerConfig& config) {
    meeting::RoomOptions options;

    // 配置里是 size_t，协议里是 uint32：先夹到 uint32 且不小于协议下限，
    // 避免 size_t -> uint32 截断，以及 std::clamp 出现 lo > hi 的未定义行为。
    const std::size_t configured = std::max<std::size_t>(
        config.max_participants_per_room, kMinParticipants);
    const uint32_t hard_max = static_cast<uint32_t>(
        std::min<std::size_t>(configured, std::numeric_limits<uint32_t>::max()));

    // v2：参数在 data 里，缺失取默认，钳制逻辑与服务端配置保持一致
    options.max_participants = std::clamp(
        read_u32(packet.data, "max_participants", hard_max), kMinParticipants,
        hard_max);
    options.duration_minutes = std::clamp(
        read_u32(packet.data, "duration_minutes", kDefaultDurationMinutes),
        kMinDurationMinutes, kMaxDurationMinutes);
    return options;
}

}  // namespace

MeetingService::MeetingService(config::ServerConfig config,
                               boost::asio::io_context& io_ctx)
    : _config(std::move(config)), _room_manager(_config, io_ctx) {}

void MeetingService::bind_server(network::Server& server) {
    server.set_connection_handler(
        [this](std::shared_ptr<network::Connection> conn) { on_new_connection(conn); });
}

void MeetingService::on_new_connection(std::shared_ptr<network::Connection> conn) {
    conn->set_read_handler([this](std::shared_ptr<network::Connection> c) {
        on_connection_read(std::move(c));
    });
    conn->set_close_handler([this](std::shared_ptr<network::Connection> c) {
        on_connection_close(std::move(c));
    });
}

void MeetingService::on_connection_read(std::shared_ptr<network::Connection> conn) {
    protocol::Packet packet;
    while (conn->is_open()) {
        const auto status = protocol::Codec::try_decode(conn->read_buffer(), packet);
        if (status == protocol::DecodeStatus::NeedMore) {
            break;
        }
        if (status == protocol::DecodeStatus::Invalid) {
            spdlog::warn("invalid packet from connection {}", conn->id());
            conn->close();
            return;
        }

        auto room_it = _connection_room.find(conn->id());
        if (room_it != _connection_room.end()) {
            /*找到了房间, 处理房间内消息*/
            auto room = room_it->second;
            room->handle_packet(conn, packet);
            if (packet.type == protocol::MessageType::ExitMeeting) {
                // exit_meeting 后该连接回到大厅（房主离开时房间已关闭并清理过映射）
                _connection_room.erase(conn->id());
            }
        } else {
            /*没有找到房间, 处理大厅消息*/
            handle_lobby_packet(conn, packet);
        }
    }
}

void MeetingService::on_connection_close(std::shared_ptr<network::Connection> conn) {
    auto room_it = _connection_room.find(conn->id());
    if (room_it == _connection_room.end()) {
        // 大厅连接/握手失败也会走到这里，属正常情况，不要刷 warn
        spdlog::debug("connection {} closed before joining any room", conn->id());
        return;
    }

    auto room = room_it->second;
    _connection_room.erase(room_it);
    room->remove_participant(conn->id());
}

void MeetingService::handle_lobby_packet(std::shared_ptr<network::Connection> conn,
                                         const protocol::Packet& packet) {
    switch (packet.type) {
        case protocol::MessageType::CreateMeeting:
            handle_create_meeting(conn, packet);
            break;
        case protocol::MessageType::JoinMeeting:
            handle_join_meeting(conn, packet);
            break;
        default:
            spdlog::warn("connection {} sent unexpected lobby message {}", conn->id(),
                         static_cast<int>(packet.type));
            conn->close();
            break;
    }
}

void MeetingService::handle_create_meeting(std::shared_ptr<network::Connection> conn,
                                           const protocol::Packet& packet) {
    const auto options = parse_create_meeting_options(packet, _config);

    bool auth_failed = false;
    const uint64_t owner_user_id =
        resolve_user_id(_config, packet, conn->id(), auth_failed);
    if (auth_failed) {
        spdlog::warn("create meeting rejected: invalid token from connection {}",
                     conn->id());
        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::CreateMeetingResponse, -3,
            "invalid token", 0)));
        return;
    }
    if (owner_user_id == 0) {
        spdlog::warn("create meeting rejected: user_id=0 from connection {}",
                     conn->id());
        conn->close();
        return;
    }

    const auto room_id = _room_manager.create_room(conn, options, owner_user_id);
    if (!room_id) {
        spdlog::warn("create meeting failed for connection {}", conn->id());
        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::CreateMeetingResponse, -1,
            "create meeting failed", 0)));
        return;
    }

    auto room = _room_manager.get_room(*room_id);
    room->set_close_callback([this](uint32_t id) { cleanup_room(id); });
    _connection_room.emplace(conn->id(), room);
    spdlog::info(
        "meeting created: room id = {} owner connection id = {} "
        "max_participants = {} duration_minutes = {}",
        *room_id, conn->id(), options.max_participants,
        options.duration_minutes);

    conn->send(protocol::Codec::encode(make_room_response(
        protocol::MessageType::CreateMeetingResponse, 0, "ok", *room_id)));
}

void MeetingService::handle_join_meeting(std::shared_ptr<network::Connection> conn,
                                         const protocol::Packet& packet) {
    bool auth_failed = false;
    const uint64_t user_id =
        resolve_user_id(_config, packet, conn->id(), auth_failed);
    if (auth_failed) {
        spdlog::warn("join meeting rejected: invalid token from connection {}",
                     conn->id());
        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::JoinMeetingResponse, -3,
            "invalid token", 0)));
        return;
    }

    const uint32_t room_id = read_u32(packet.data, "room_id", 0);
    if (room_id == 0) {
        spdlog::warn("join meeting rejected: missing room_id from connection {}",
                     conn->id());
        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::JoinMeetingResponse, -2,
            "room not found", 0)));
        return;
    }
    if (user_id == 0) {
        spdlog::warn("join meeting rejected: user_id=0 from connection {}",
                     conn->id());
        conn->close();
        return;
    }

    spdlog::info("connection {} join meeting, room_id: {} user_id: {}",
                 conn->id(), room_id, user_id);
    const auto result = _room_manager.join_room(room_id, conn, user_id);

    if (result == meeting::RoomManager::JoinResult::Ok) {
        auto room = _room_manager.get_room(room_id);
        _connection_room.emplace(conn->id(), room);

        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::JoinMeetingResponse, 0, "ok", room_id)));
        notify_partner_join(room, conn);
        spdlog::info("connection {} joined room {}", conn->id(), room_id);
        return;
    }

    if (result == meeting::RoomManager::JoinResult::Full) {
        spdlog::info("room {} is full, reject connection {}", room_id, conn->id());
        conn->send(protocol::Codec::encode(make_room_response(
            protocol::MessageType::JoinMeetingResponse, -1, "room is full", 0)));
        return;
    }

    spdlog::info("room {} not available, reject connection {}", room_id,
                 conn->id());
    conn->send(protocol::Codec::encode(make_room_response(
        protocol::MessageType::JoinMeetingResponse, -2,
        "room not available", 0)));
}

void MeetingService::notify_partner_join(std::shared_ptr<meeting::Room> room,
                                         std::shared_ptr<network::Connection> newcomer) {
    if (!room || !newcomer) {
        return;
    }
    room->notify_user_joined(newcomer);
}

void MeetingService::cleanup_room(uint32_t room_id) {
    spdlog::info("cleaning up room {}", room_id);
    for (auto it = _connection_room.begin(); it != _connection_room.end();) {
        if (it->second && it->second->id() == room_id) {
            it = _connection_room.erase(it);
        } else {
            ++it;
        }
    }
    _room_manager.remove_room(room_id);
}

}  // namespace service
