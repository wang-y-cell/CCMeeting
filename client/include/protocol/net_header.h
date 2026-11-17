#ifndef NETHEADER_H
#define NETHEADER_H
#include "protocol/room_settings_flags.h"

#include <QMetaType>
#include <cstdint>



#ifndef OPENVIDEO
#define OPENVIDEO "打开视频"
#endif

#ifndef CLOSEVIDEO
#define CLOSEVIDEO "关闭视频"
#endif

#ifndef OPENAUDIO
#define OPENAUDIO "打开音频"
#endif

#ifndef CLOSEAUDIO
#define CLOSEAUDIO "关闭音频"
#endif

/**
 * @brief 线上消息类型
 *
 * 传输为 WebSocket，线上用 JSON 的 `t` 字段表示类型（见 MessageCodec::type_name），
 * 这里的枚举值只用于程序内部。
 */
enum MSG_TYPE : std::uint8_t {
    TEXT_SEND = 0,
    TEXT_RECV,
    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    CLOSE_CAMERA,
    ROOM_SETTINGS,  // 客户端 -> 服务器：房主修改房间开关（4 字节 BE flags）

    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT,
    PARTNER_JOIN,
    JOIN_MEETING_RESPONSE,
    PARTNER_JOIN2,

    USER_PROFILE = 25,
    ROOM_SETTINGS_NOTIFY = 26,  // 服务器 -> 客户端：房间开关当前值

    RemoteHostClosedError = 40,
    OtherNetError
};
Q_DECLARE_METATYPE(MSG_TYPE);


#endif // NETHEADER_H
