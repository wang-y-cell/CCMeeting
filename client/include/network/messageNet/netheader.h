#ifndef NETHEADER_H
#define NETHEADER_H
#include "room_settings_flags.h"

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
 * 传输已改为 WebSocket，线上用 JSON 的 `t` 字段表示类型
 * （见 MessageCodec::type_name），这里的枚举值只用于程序内部。
 * 0–3 为旧 TCP JPEG/PCM 媒体槽位，保留但不再使用；媒体面走 WebRTC。
 */
enum MSG_TYPE : std::uint8_t {
    IMG_SEND = 0,  // reserved legacy
    IMG_RECV,      // reserved legacy
    AUDIO_SEND,    // reserved legacy
    AUDIO_RECV,    // reserved legacy
    TEXT_SEND,
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
