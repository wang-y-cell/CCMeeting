#include "meeting/room_manager.h"

#include <spdlog/spdlog.h>

namespace meeting {

RoomManager::RoomManager(config::ServerConfig config,
                         boost::asio::io_context& io_ctx)
    : _config(std::move(config)), _io_ctx(io_ctx) {}

std::optional<uint32_t> RoomManager::create_room(
    std::shared_ptr<network::Connection> owner, RoomOptions options,
    uint64_t owner_user_id) {
    if (!owner || !owner->is_open()) {
        spdlog::warn("cannot create room: owner is not open");
        return std::nullopt;
    }
    if (_rooms.size() >= _config.max_rooms) {
        spdlog::warn("cannot create room: reached max rooms {}", _config.max_rooms);
        return std::nullopt;
    }

    const uint32_t room_id = _next_room_id++;
    auto room = std::make_shared<Room>(room_id, owner, _config, _io_ctx, options,
                                       owner_user_id);
    room->start_expire_timer();
    _rooms.emplace(room_id, room);
    spdlog::info("room {} created, max_participants={}, duration_minutes={}",
                 room_id, room->max_participants(), room->duration_minutes());
    return room_id;
}

RoomManager::JoinResult RoomManager::join_room(
    uint32_t room_id, std::shared_ptr<network::Connection> conn,
    uint64_t user_id) {
    auto room = get_room(room_id);
    if (!room) {
        spdlog::warn("room {} not found", room_id);
        return JoinResult::NotFound;
    }
    if (room->is_closed()) {
        spdlog::warn("room {} is closed", room_id);
        return JoinResult::Closed;
    }
    if (room->participant_count() >= room->max_participants()) {
        spdlog::warn("room {} is full", room_id);
        return JoinResult::Full;
    }
    if (!room->add_participant(conn, false, user_id)) {
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
