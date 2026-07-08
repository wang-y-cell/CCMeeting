#pragma once

/*
 * meeting_service.h — 会议业务服务
 *
 * 编排各模块：绑定 Server 新连接、驱动协议解码、路由消息。
 * - 未入房连接：仅处理 CREATE_MEETING / JOIN_MEETING（Lobby）
 * - 已入房连接：将媒体消息交给 Room 转发
 */

#include "config/server_config.h"
#include "meeting/room_manager.h"
#include "network/server.h"
#include "protocol/packet.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace service {

class MeetingService {
public:
    explicit MeetingService(config::ServerConfig config);

    /**
     * @brief 绑定服务器
     * @param server 服务器
     */
    void bind_server(network::Server& server);

private:
    /**
     * @brief 新连接建立时调用
     * @param conn 连接
     */
    void on_new_connection(std::shared_ptr<network::Connection> conn);
    /**
     * @brief 连接读取时调用
     * @param conn 连接
     */
    void on_connection_read(std::shared_ptr<network::Connection> conn);
    /**
     * @brief 连接关闭时调用
     * @param conn 连接
     */
    void on_connection_close(std::shared_ptr<network::Connection> conn);

    /**
     * @brief 处理大厅消息
     * @param conn 连接
     * @param packet 消息包
     */
    void handle_lobby_packet(std::shared_ptr<network::Connection> conn,
                             const protocol::Packet& packet);
    /**
     * @brief 处理创建会议消息
     * @param conn 连接
     * @param packet 消息包
     */
    void handle_create_meeting(std::shared_ptr<network::Connection> conn,
                               const protocol::Packet& packet);
    /**
     * @brief 处理加入会议消息
     * @param conn 连接
     * @param packet 消息包
     */
    void handle_join_meeting(std::shared_ptr<network::Connection> conn,
                             const protocol::Packet& packet);
    /**
     * @brief 通知伙伴加入
     * @param room 房间
     * @param newcomer 新加入者
     */
    void notify_partner_join(std::shared_ptr<meeting::Room> room,
                             std::shared_ptr<network::Connection> newcomer);
    /**
     * @brief 清理房间
     * @param room_id 房间ID
     */
    void cleanup_room(uint32_t room_id);

    config::ServerConfig _config;
    meeting::RoomManager _room_manager;
    std::unordered_map<network::Connection::Id, std::shared_ptr<meeting::Room>>
        _connection_room;  // 连接 → 所在房间
};

}  // namespace service
