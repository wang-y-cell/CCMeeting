#ifndef NETHEADER_H
#define NETHEADER_H

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
 * 传输为 WebSocket 文本帧，一条消息 = 一个 JSON 对象（规范见 docs/schema/api.json）：
 *
 *   上行：{"t":"<类型>", "token":"<可选>", "data":{...}}
 *   下行：{"t":"<类型>", "code":0, "message":"ok", "data":{...}}
 *
 * - `t` 用 JSON 类型名表示（见 MessageCodec::type_name），本枚举只用于程序内部；
 * - `code`：0=成功，非 0=失败（join_meeting_response 失败时 -1/-2/-3）；
 * - `message`：成功固定 "ok"，失败为可读原因；
 * - `data`：全部业务字段都在这里；v1 的 `p`/`u`/`s`/`a` 与二进制大端布局已废弃。
 */
enum MSG_TYPE : std::uint8_t {
    TEXT_SEND = 0,
    TEXT_RECV,
    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    CLOSE_CAMERA,   // 上行 data 为 {}；下行 data 为 {"user_id":N}
    ROOM_SETTINGS,  // 客户端 -> 服务器：房主修改房间开关
                    // data: {"allow_record":bool,"allow_member_share":bool}

    CREATE_MEETING_RESPONSE = 20,  // data: {"room_id":N}，失败时 code!=0 且 room_id=0
    PARTNER_EXIT,                  // data: {"user_id":N}
    PARTNER_JOIN,                  // data: {"user_id":N}
    JOIN_MEETING_RESPONSE,         // data: {"room_id":N}；失败时 code=-1/-2/-3
    PARTNER_JOIN2,                 // data: {"user_ids":[N,...]}

    USER_PROFILE = 25,              // 上行 data: {"nickname","avatar"}
                                    // 下行 data: {"user_id","nickname","avatar"}
    ROOM_SETTINGS_NOTIFY = 26,      // 服务器 -> 客户端：房间开关当前值
                                    // data: {"allow_record":bool,"allow_member_share":bool}

    RemoteHostClosedError = 40,     // 本地网络错误，不走线上
    OtherNetError
};


#endif // NETHEADER_H
