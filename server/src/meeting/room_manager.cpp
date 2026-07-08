#include "meeting/room_manager.h"

#include <spdlog/spdlog.h>

namespace meeting {

RoomManager::RoomManager(config::ServerConfig config) : _config(std::move(config)) {}

std::optional<uint32_t> RoomManager::create_room(
    std::shared_ptr<network::Connection> owner) {
    if (!owner || !owner->is_open()) {
        spdlog::warn("cannot create room: owner is not open");
        return std::nullopt;
    }
    if (_rooms.size() >= _config.max_rooms) {
        spdlog::warn("cannot create room: reached max rooms {}", _config.max_rooms);
        return std::nullopt;
    }

    const uint32_t room_id = _next_room_id++;
    auto room = std::make_shared<Room>(room_id, owner, _config);
    _rooms.emplace(room_id, room);
    spdlog::info("room {} created", room_id);
    return room_id;
}

RoomManager::JoinResult RoomManager::join_room(
    uint32_t room_id, std::shared_ptr<network::Connection> conn) {
    auto room = get_room(room_id);
    if (!room) {
        spdlog::warn("room {} not found", room_id);
        return JoinResult::NotFound;
    }
    if (room->is_closed()) {
        spdlog::warn("room {} is closed", room_id);
        return JoinResult::Closed;
    }
    if (room->participant_count() >= _config.max_participants_per_room) {
        spdlog::warn("room {} is full", room_id);
        return JoinResult::Full;
    }
    if (!room->add_participant(conn, false)) {
        spdlog::warn("room {} add participant failed", room_id);
        return JoinResult::Full;
    }
    return JoinResult::Ok;
}

std::shared_ptr<Room> RoomManager::get_room(uint32_t room_id) {
    auto it = _rooms.find(room_id);
    if (it == _rooms.end()) {
        return nullptr;
    }
    return it->second;
}

void RoomManager::remove_room(uint32_t room_id) {
    _rooms.erase(room_id);
    spdlog::info("room {} removed", room_id);
}

}  // namespace meeting
