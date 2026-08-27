#ifndef NETHEADER_H
#define NETHEADER_H
#include <QMetaType>
#include <cstdint>

#ifndef MSG_HEADER
#define MSG_HEADER 15
#endif

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
 * @brief 线上协议消息类型（与对端约定的 MSG_TYPE）
 * @note 0–3 为旧 TCP JPEG/PCM 媒体槽位，数值保留勿改；媒体面已改走 WebRTC。
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

    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT,
    PARTNER_JOIN,
    JOIN_MEETING_RESPONSE,
    PARTNER_JOIN2,

    USER_PROFILE = 25,

    RemoteHostClosedError = 40,
    OtherNetError
};
Q_DECLARE_METATYPE(MSG_TYPE);

#endif // NETHEADER_H
