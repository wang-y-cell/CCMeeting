#pragma once

/*
 * room_manager.h — 房间管理器
 *
 * 按需动态创建/销毁房间，维护 room_id → Room 映射。
 * 受 ServerConfig 中 max_rooms 与 max_participants_per_room 约束。
 */

#include "config/server_config.h"
#include "meeting/room.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

namespace meeting {

class RoomManager {
public:
    enum class JoinResult {
        Ok,
        NotFound,  // 房间不存在
        Full,      // 已达人数上限
        Closed,    // 房间已关闭
    };

    explicit RoomManager(config::ServerConfig config);

    /**
     * @brief 创建房间
     * @param owner 房主连接
     * @return 房间ID
     */
    std::optional<uint32_t> create_room(std::shared_ptr<network::Connection> owner);
    /**
     * @brief 加入房间
     * @param room_id 房间ID
     * @param conn 连接
     */
    JoinResult join_room(uint32_t room_id, std::shared_ptr<network::Connection> conn);

    /**
     * @brief 获取房间
     * @param room_id 房间ID
     * @return 房间
     */
    std::shared_ptr<Room> get_room(uint32_t room_id);

    /**
     * @brief 删除房间
     * @param room_id 房间ID
     */
    void remove_room(uint32_t room_id);

    /**
     * @brief 获取房间数量
     * @return 房间数量
     */
    std::size_t room_count() const { return _rooms.size(); }

    /**
     * @brief 获取服务器配置
     * @return 服务器配置
     */
    const config::ServerConfig& config() const { return _config; }

    /**
     * @brief 获取房间ID
     * @return 房间ID
     */
    uint32_t next_room_id() const { return _next_room_id; }

private:
    config::ServerConfig _config;/*服务器配置*/
    std::unordered_map<uint32_t, std::shared_ptr<Room>> _rooms; /*房间号和房间的指针的映射*/
    uint32_t _next_room_id = 1;  /*自增房间号，替代 cloudMeet 的 pid*/
};

}  // namespace meeting
