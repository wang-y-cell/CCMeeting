#ifndef ROOM_SETTINGS_FLAGS_H
#define ROOM_SETTINGS_FLAGS_H

#include <cstdint>

/**
 * @brief 房间开关位定义
 *
 * 客户端与服务端共用同一套位含义，数值必须与
 * message_server/include/protocol/message_type.h 中的
 * kRoomSettings* 常量保持一致。
 *
 * 单独成文件是因为 netheader.h、meeting_widget.h、会议设置面板都要用，
 * 又不希望为了几个常量把整条消息协议头都拉进来。
 */
enum ROOM_SETTINGS_FLAG : std::uint32_t {
    ROOM_SETTINGS_ALLOW_RECORD = 1u << 0,        ///< 允许录制会议
    ROOM_SETTINGS_ALLOW_MEMBER_SHARE = 1u << 1,  ///< 允许普通成员共享屏幕
};

/** @brief 房间开关默认值：允许录制、允许成员共享 */
#define ROOM_SETTINGS_DEFAULT \
    (ROOM_SETTINGS_ALLOW_RECORD | ROOM_SETTINGS_ALLOW_MEMBER_SHARE)

#endif // ROOM_SETTINGS_FLAGS_H
