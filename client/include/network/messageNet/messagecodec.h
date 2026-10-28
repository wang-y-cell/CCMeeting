#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "message.h"
#include "netheader.h"

#include <QByteArray>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief 业务 Message 与 WebSocket 帧之间的编解码
 *
 * 传输层已从自定义 TCP 字节流改为 WebSocket，因此不再有
 * `$ | type | uid | len | payload | #` 这种二进制帧头。
 *
 * 线上格式（UTF-8 JSON，一条 WS 消息 = 一条业务消息）：
 * @code
 *   {"t":"text_send", "u":123, "s":"hello"}            // 文本类：明文 s 字段
 *   {"t":"create_meeting", "u":123, "p":"AAAAAgAA..."}  // 其它：p 为 payload 的 Base64
 * @endcode
 *
 * - `t` 消息类型名（见 type_name()）
 * - `u` 发送者 user_id（下行成员事件靠它区分是谁）
 * - `s` 文本内容（仅 TEXT_SEND / TEXT_RECV），不再压缩
 * - `p` 二进制 payload 的 Base64（旗帜位、房间号、资料结构等保持原二进制布局，
 *      因此服务端 room/service 的业务代码无需改动）
 */
class MessageCodec {
public:
    /**
     * @brief 将业务 Message 编码为 JSON 文本帧
     * @param msg 业务 Message
     * @param local_user_id 本地用户 ID
     * @return UTF-8 JSON 字节
     */
    static QByteArray encode_wire_frame(const Message &msg, qint64 local_user_id);

    /**
     * @brief 解析单条 JSON 文本帧为业务 Message
     * @param json_frame JSON 字节
     * @return 业务 Message（无法识别返回 nullptr）
     */
    static MessagePtr decode_json_frame(const QByteArray &json_frame);

    /**
     * @brief 解析已解出的 payload（保留此入口供自检使用）
     * @param payload payload 字节（文本类为明文 UTF-8）
     * @param user_id 发送者 user_id
     * @param msgtype 消息类型
     */
    static MessagePtr decode_payload(const QByteArray &payload, qint64 user_id,
                                     MSG_TYPE msgtype);

    /** @brief 消息类型名（线上 `t` 字段），如 text_send */
    static const char *type_name(MSG_TYPE type);
    /** @brief 由线上类型名反查枚举，未知返回 std::nullopt */
    static std::optional<MSG_TYPE> type_from_name(const QString &name);

    /**
     * @brief 流式解帧器
     *
     * WebSocket 按消息投递，正常每次 feed 就是一条完整 JSON；
     * 仍保留缓冲以防御半包/粘包。
     */
    class WireStreamParser {
    public:
        /** @brief 清空缓冲 */
        void reset();
        /** @brief 喂入一段字节，返回本次解出的全部消息 */
        std::vector<MessagePtr> feed(const char *data, std::size_t len);

    private:
        QByteArray buffer_;
        static constexpr std::size_t k_max_buffer = 4 * 1024 * 1024;
    };

private:
    static MSG_TYPE to_wire_type(MessageKind kind);
    static MessageKind from_wire_type(MSG_TYPE type);
};

#endif // MESSAGECODEC_H
