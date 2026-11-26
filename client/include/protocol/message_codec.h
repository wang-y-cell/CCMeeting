#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "protocol/message.h"
#include "config/global_config.h"
#include "protocol/net_header.h"

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief 业务 Message 与 WebSocket 帧之间的编解码（统一 JSON 规范 v2）
 *
 * 事实来源：docs/schema/api.json（x-ws / x-messages）。一条 WS 文本帧 = 一个 JSON 对象。
 *
 * 上行（客户端 → 服务端）：
 * @code
 *   {"t":"text_send", "token":"<可选>", "data":{"text":"hello"}}
 *   {"t":"create_meeting", "token":"...", "data":{"max_participants":8,"duration_minutes":60}}
 *   {"t":"close_camera", "token":"...", "data":{}}
 * @endcode
 *
 * 下行（服务端 → 客户端）：
 * @code
 *   {"t":"text_recv", "code":0, "message":"ok", "data":{"user_id":123,"text":"hello"}}
 *   {"t":"join_meeting_response", "code":-1, "message":"房间已满", "data":{"room_id":0}}
 * @endcode
 *
 * 约定：
 * - `t` 消息类型名（见 type_name()）；
 * - `token` 只在上行、且本地已登录时出现（取自 UserSession::instance().token()）；
 * - `code`/`message` 只在下行出现：0=成功，非 0=失败（HTTP 下与状态码同值）；
 * - 所有业务字段（含 user_id）一律在 `data` 里，不再有 `p`(base64 payload)、
 *   `u`(用户 id)、`s`(文本)、`a`(token) 这些 v1 顶层字段，也不再有二进制大端布局。
 *
 * 为了不改动上层一堆调用点，Message 子类的公开访问器保持原样（room_no_u32()、
 * text()、display_name()、avatar_url()、partner_user_ids() 等）；房间开关在
 * 内部与线上都是两个独立布尔（allow_record / allow_member_share），不再做位掩码转换。
 */
class MessageCodec {
public:
    /**
     * @brief 将业务 Message 编码为 JSON 文本帧
     * @param msg 业务 Message
     * @param local_user_id 本地用户 ID。上行帧不再携带 user_id（身份由 token 决定），
     *        该参数仅在下行消息往返自测时作为 user_id 缺失的兜底值
     * @return UTF-8 JSON 字节
     */
    static QByteArray encode_json_frame(const Message &msg, qint64 local_user_id);

    /**
     * @brief 解析单条 JSON 文本帧为业务 Message
     * @param json_frame JSON 字节
     * @return 业务 Message（无法识别返回 nullptr）
     */
    static MessagePtr decode_json_frame(const QByteArray &json_frame);

    /** @brief 消息类型名（线上 `t` 字段），如 text_send */
    static const char *type_name(MSG_TYPE type);
    /** @brief 由线上类型名反查枚举，未知返回 std::nullopt */
    static std::optional<MSG_TYPE> type_from_name(const QString &name);

    /**
     * @brief 流式解帧器
     *
     * WebSocket 按消息投递，正常每次 feed 就是一条完整 JSON；
     * 仍保留缓冲以防御半包/粘包。非 JSON 帧（旧协议残留/异常对端）会被丢弃并记日志。
     */
    class WireStreamParser {
    public:
        /** @brief 清空缓冲 */
        void reset();
        /** @brief 喂入一段字节，返回本次解出的全部消息 */
        std::vector<MessagePtr> feed(const char *data, std::size_t len);

    private:
        QByteArray buffer_;
        static constexpr std::size_t k_max_buffer = config::protocol::max_message_buffer;
    };

private:
    static MSG_TYPE to_wire_type(MessageKind kind);
};

#endif // MESSAGECODEC_H
