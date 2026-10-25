#include "meeting/room_manager.h"

#include <spdlog/spdlog.h>

#include <random>

namespace meeting {

namespace {

/// 随机房间号范围：6 位数字，便于口头/短信分享，且远离 0（0 表示无房间）
constexpr uint32_t kRoomIdMin = 100000;
constexpr uint32_t kRoomIdMax = 999999;
/// 冲突重试次数；用尽后回退到自增兜底，保证一定能建出房间
constexpr int kRoomIdAttempts = 32;

uint32_t random_room_id() {
    // random_device 负责播种；mt19937 保证同一进程内分布均匀
    static std::mt19937 engine{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> dist(kRoomIdMin, kRoomIdMax);
    return dist(engine);
}

}  // namespace

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

    // 随机房间号：避免每次都是 1、2、3…；与现有房间冲突则重试
    uint32_t room_id = 0;
    for (int attempt = 0; attempt < kRoomIdAttempts; ++attempt) {
        const uint32_t candidate = random_room_id();
        if (_rooms.find(candidate) == _rooms.end()) {
            room_id = candidate;
            break;
        }
        spdlog::debug("room id {} already in use, retrying", candidate);
    }

    if (room_id == 0) {
        // 极端兜底：随机段被占满时退回自增，保证创建不会失败
        room_id = _next_room_id++;
        while (_rooms.find(room_id) != _rooms.end()) {
            room_id = _next_room_id++;
        }
        spdlog::warn("random room id exhausted, fallback to sequential {}",
                     room_id);
    }

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
