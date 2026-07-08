#include "meeting/room.h"
#include "protocol/message_type.h"
#include "protocol/codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <arpa/inet.h>
#include <cstring>

namespace meeting {

Room::Room(uint32_t room_id,
           std::shared_ptr<network::Connection> owner,
           const config::ServerConfig& config)
    : _room_id(room_id), _config(config) {
    add_participant(std::move(owner), true);
}

void Room::set_close_callback(CloseCallback callback) {
    _close_callback = std::move(callback);
}

bool Room::add_participant(std::shared_ptr<network::Connection> conn, bool is_owner) {
    if (_closed || !conn || !conn->is_open()) {
        spdlog::warn("room {} add participant failed", _room_id);
        return false;
    }
    if (_participants.size() >= _config.max_participants_per_room) {
        spdlog::warn("room {} is full", _room_id);
        return false;
    }

    Participant participant;
    participant.connection = conn;
    participant.ip_network = conn->peer_ip_network();
    participant.is_owner = is_owner;
    _participants.emplace(conn->id(), std::move(participant));
    return true;
}

void Room::remove_participant(network::Connection::Id conn_id) {
    if (_closed) {
        return;
    }

    auto it = _participants.find(conn_id);
    if (it == _participants.end()) {
        return;
    }

    const bool was_owner = it->second.is_owner;
    const uint32_t ip_network = it->second.ip_network;
    _participants.erase(it);

    if (was_owner) {
        close_room();
        return;
    }

    protocol::Packet packet;
    packet.type = protocol::MessageType::PartnerExit;
    packet.ip = ip_network;
    broadcast(packet);
}

void Room::close_room() {
    if (_closed) {
        return;
    }
    _closed = true;

    auto participants = _participants;
    _participants.clear();
    for (auto& item : participants) {
        if (item.second.connection && item.second.connection->is_open()) {
            item.second.connection->close();
        }
    }

    if (_close_callback) {
        _close_callback(_room_id);
    }
}

void Room::send_to(std::shared_ptr<network::Connection> target,
                   const protocol::Packet& packet) {
    if (!target || !target->is_open()) {
        return;
    }
    target->send(protocol::Codec::encode(packet));
}

void Room::broadcast(const protocol::Packet& packet,
                       network::Connection::Id exclude) {
    spdlog::debug("room {} broadcast packet to {} participants", _room_id, _participants.size());
    const auto encoded = protocol::Codec::encode(packet);
    for (const auto& item : _participants) {
        if (item.first == exclude) {
            continue;
        }
        if (item.second.connection && item.second.connection->is_open()) {
            item.second.connection->send(encoded);
        }
    }
}

void Room::handle_packet(std::shared_ptr<network::Connection> from,
                         const protocol::Packet& packet) {
    if (_closed || !from) {
        return;
    }

    auto participant_it = _participants.find(from->id());
    if (participant_it == _participants.end()) {
        return;
    }

    const uint32_t sender_ip = participant_it->second.ip_network;
    spdlog::debug("room {} handle packet from connection {}, type: {}", _room_id, from->id(), protocol::get_type_name(packet.type));

    switch (packet.type) {
        case protocol::MessageType::ImgSend:
        case protocol::MessageType::AudioSend:
        case protocol::MessageType::TextSend: {
            protocol::Packet outbound = packet;
            if (packet.type == protocol::MessageType::ImgSend) {
                outbound.type = protocol::MessageType::ImgRecv;
            } else if (packet.type == protocol::MessageType::AudioSend) {
                outbound.type = protocol::MessageType::AudioRecv;
            } else {
                outbound.type = protocol::MessageType::TextRecv;
            }
            outbound.ip = sender_ip;
            broadcast(outbound, from->id());
            break;
        }
        case protocol::MessageType::CloseCamera: {
            protocol::Packet outbound;
            outbound.type = protocol::MessageType::CloseCamera;
            outbound.ip = sender_ip;
            broadcast(outbound, from->id());
            break;
        }
        default:
            spdlog::warn("room {} ignored message type {}", _room_id,
                         static_cast<int>(packet.type));
            break;
    }
}

void Room::notify_user_joined(std::shared_ptr<network::Connection> newcomer) {
    if (_closed || !newcomer) {
        return;
    }

    const uint32_t newcomer_ip = newcomer->peer_ip_network();

    protocol::Packet join_notice;
    join_notice.type = protocol::MessageType::PartnerJoin;
    join_notice.ip = newcomer_ip;
    broadcast(join_notice, newcomer->id());

    protocol::Packet partner_list;
    partner_list.type = protocol::MessageType::PartnerJoin2;
    for (const auto& item : _participants) {
        if (item.first == newcomer->id()) {
            continue;
        }
        const uint32_t ip_wire = htonl(item.second.ip_network);
        partner_list.payload.insert(partner_list.payload.end(),
                                    reinterpret_cast<const uint8_t*>(&ip_wire),
                                    reinterpret_cast<const uint8_t*>(&ip_wire) + sizeof(ip_wire));
    }
    send_to(newcomer, partner_list);
}

}  // namespace meeting
