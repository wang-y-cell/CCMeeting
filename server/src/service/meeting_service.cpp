#include "service/meeting_service.h"

#include "protocol/codec.h"

#include <spdlog/spdlog.h>

#include <arpa/inet.h>
#include <cstring>

namespace service {

MeetingService::MeetingService(config::ServerConfig config)
    : _config(std::move(config)), _room_manager(_config) {}

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
            /*找到了房间, 处理媒体消息*/
            room_it->second->handle_packet(conn, packet);
        } else {
            /*没有找到房间, 处理大厅消息*/
            handle_lobby_packet(conn, packet);
        }
    }
}

void MeetingService::on_connection_close(std::shared_ptr<network::Connection> conn) {
    auto room_it = _connection_room.find(conn->id());
    if (room_it == _connection_room.end()) {
        spdlog::warn("connection {} close, but not in any room", conn->id());
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
                                           const protocol::Packet& /*packet*/) {
    const auto room_id = _room_manager.create_room(conn);

    protocol::Packet response;
    response.type = protocol::MessageType::CreateMeetingResponse;

    uint32_t room_no = 0;
    if (room_id) {
        auto room = _room_manager.get_room(*room_id);
        room->set_close_callback([this](uint32_t id) { cleanup_room(id); });
        _connection_room.emplace(conn->id(), room);
        room_no = htonl(*room_id);
        spdlog::info("meeting created: room id = {} owner connection id = {}", *room_id,
                     conn->id());
    } else {
        spdlog::warn("create meeting failed for connection {}", conn->id());
    }

    response.payload.resize(sizeof(room_no));
    std::memcpy(response.payload.data(), &room_no, sizeof(room_no));
    conn->send(protocol::Codec::encode(response));
}

void MeetingService::handle_join_meeting(std::shared_ptr<network::Connection> conn,
                                         const protocol::Packet& packet) {
    if (packet.payload.size() < sizeof(uint32_t)) {
        spdlog::warn("join meeting payload too short from connection {}", conn->id());
        conn->close();
        return;
    }

    uint32_t room_no_net = 0;
    /*将房间号复制到room_no_net中(网络字节序)*/
    std::memcpy(&room_no_net, packet.payload.data(), sizeof(room_no_net));
    /*将网络字节序转换为主机字节序*/
    const uint32_t room_id = ntohl(room_no_net);
    spdlog::info("connection {} join meeting, room_id: {}", conn->id(), room_id);
    const auto result = _room_manager.join_room(room_id, conn);

    protocol::Packet response;
    response.type = protocol::MessageType::JoinMeetingResponse;
    response.payload.resize(sizeof(uint32_t));

    uint32_t response_value = 0;
    if (result == meeting::RoomManager::JoinResult::Ok) {
        auto room = _room_manager.get_room(room_id);
        _connection_room.emplace(conn->id(), room);

        response_value = htonl(room_id);
        std::memcpy(response.payload.data(), &response_value, sizeof(response_value));
        conn->send(protocol::Codec::encode(response));
        notify_partner_join(room, conn);
        spdlog::info("connection {} joined room {}", conn->id(), room_id);
        return;
    }

    if (result == meeting::RoomManager::JoinResult::Full) {
        response_value = htonl(static_cast<uint32_t>(-1));
        spdlog::info("room {} is full, reject connection {}", room_id, conn->id());
    } else {
        spdlog::info("room {} not available, reject connection {}", room_id,
                     conn->id());
    }

    std::memcpy(response.payload.data(), &response_value, sizeof(response_value));
    conn->send(protocol::Codec::encode(response));
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
