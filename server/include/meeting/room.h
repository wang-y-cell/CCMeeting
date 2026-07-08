#pragma once

/*
 * room.h — 单个会议房间
 *
 * 管理房间成员、媒体消息广播与成员进出通知。
 * 房主断开时关闭整个房间；普通成员离开则广播 PARTNER_EXIT。
 */

#include "config/server_config.h"
#include "meeting/participant.h"
#include "protocol/packet.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace meeting {

class Room : public std::enable_shared_from_this<Room> {
public:
    using CloseCallback = std::function<void(uint32_t room_id)>;

    Room(uint32_t room_id,
         std::shared_ptr<network::Connection> owner,
         const config::ServerConfig& config);

    /**
     * @brief 获取房间ID
     * @return 房间ID
     */
    uint32_t id() const { return _room_id; }
    /**
     * @brief 获取房间是否关闭
     * @return 房间是否关闭
     */
    bool is_closed() const { return _closed; }
    /**
     * @brief 获取房间参与者数量
     * @return 房间参与者数量
     */
    std::size_t participant_count() const { return _participants.size(); }
    /**
     * @brief 添加参与者
     * @param conn 连接
     * @param is_owner 是否为房主
     * @return 是否成功
     */

    bool add_participant(std::shared_ptr<network::Connection> conn, bool is_owner);
    /**
     * @brief 删除参与者
     * @param conn_id 连接ID
     */
    void remove_participant(network::Connection::Id conn_id);

    /**
     * @brief 处理房间内媒体消息（IMG/AUDIO/TEXT/CLOSE_CAMERA），转发给其它成员
     * @param from 发送者
     * @param packet 媒体消息
     */
    void handle_packet(std::shared_ptr<network::Connection> from,
                       const protocol::Packet& packet);

    /**
     * @brief 新人加入后：广播 PARTNER_JOIN，并向新人发送 PARTNER_JOIN2（在场 IP 列表）
     * @param newcomer 新人
     */
    void notify_user_joined(std::shared_ptr<network::Connection> newcomer);

    /**
     * @brief 设置关闭回调
     * @param callback 关闭回调
     */
    void set_close_callback(CloseCallback callback);

private:
    /**
     * @brief 广播媒体消息
     * @param packet 媒体消息
     * @param exclude 排除的连接ID
     */
    void broadcast(const protocol::Packet& packet,
                   network::Connection::Id exclude = 0);
    /**
     * @brief 发送媒体消息到指定连接
     * @param target 目标连接
     * @param packet 媒体消息
     */
    void send_to(std::shared_ptr<network::Connection> target,
                 const protocol::Packet& packet);
    /**
     * @brief 关闭房间
     */
    void close_room();

    uint32_t _room_id;/*房间ID*/
    config::ServerConfig _config;/*服务器配置*/
    bool _closed = false;/*房间是否关闭*/
    std::unordered_map<network::Connection::Id, Participant> _participants;/*连接ID和参与者的映射*/
    CloseCallback _close_callback;/*关闭回调*/
};
}  // namespace meeting
