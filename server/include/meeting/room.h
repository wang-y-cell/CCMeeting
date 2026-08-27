#pragma once

/*
 * room.h — 单个会议房间
 *
 * 管理房间成员、媒体消息广播与成员进出通知。
 * 房主断开或会议时长到期时关闭整个房间；普通成员离开则广播 PARTNER_EXIT。
 */

#include "config/server_config.h"
#include "meeting/participant.h"
#include "protocol/packet.h"

#include <boost/asio.hpp>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace meeting {

/** @brief 创建房间时的可选参数 */
struct RoomOptions {
    std::size_t max_participants = 1024;  ///< 本房间人数上限
    uint32_t duration_minutes = 60;       ///< 会议时长（分钟），0 表示不限时
};

class Room : public std::enable_shared_from_this<Room> {
public:
    using CloseCallback = std::function<void(uint32_t room_id)>;

    Room(uint32_t room_id,
         std::shared_ptr<network::Connection> owner,
         const config::ServerConfig& config,
         boost::asio::io_context& io_ctx,
         RoomOptions options,
         uint64_t owner_user_id);

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
     * @brief 获取本房间人数上限
     */
    std::size_t max_participants() const { return _options.max_participants; }
    /**
     * @brief 获取会议时长（分钟）
     */
    uint32_t duration_minutes() const { return _options.duration_minutes; }

    /**
     * @brief 启动到期关房定时器（须在 shared_ptr 管理下调用）
     */
    void start_expire_timer();

    /**
     * @brief 添加参与者
     * @param conn 连接
     * @param is_owner 是否为房主
     * @return 是否成功
     */
    bool add_participant(std::shared_ptr<network::Connection> conn, bool is_owner,
                         uint64_t user_id);
    /**
     * @brief 删除参与者
     * @param conn_id 连接ID
     */
    void remove_participant(network::Connection::Id conn_id);

    /**
     * @brief 处理房间内控制/文本消息（TEXT/CLOSE_CAMERA/USER_PROFILE 等），转发给其它成员
     * @param from 发送者
     * @param packet 消息
     */
    void handle_packet(std::shared_ptr<network::Connection> from,
                       const protocol::Packet& packet);

    /**
     * @brief 新人加入后：广播 PARTNER_JOIN，并向新人发送 PARTNER_JOIN2（在场用户 ID 列表）
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
    RoomOptions _options;/*本房间参数*/
    boost::asio::io_context& _io_ctx;/*IO上下文，用于到期定时器*/
    std::unique_ptr<boost::asio::steady_timer> _expire_timer;/*到期关房定时器*/
    bool _closed = false;/*房间是否关闭*/
    std::unordered_map<network::Connection::Id, Participant> _participants;/*连接ID和参与者的映射*/
    CloseCallback _close_callback;/*关闭回调*/
};
}  // namespace meeting
