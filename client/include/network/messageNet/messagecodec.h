#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "message.h"
#include "netheader.h"
#include <QByteArray>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief 业务 Message 与网络帧之间的编解码,解析网络帧中的业务 Message
 */
class MessageCodec {
public:
    /**
     * @brief 将业务 Message 编码为网络帧
     * @param msg 业务 Message
     * @param local_ip 本地 IP 地址
     * @return 网络帧
    */
    static QByteArray encode_wire_frame(const Message &msg, std::uint32_t local_ip);

    /**
     * @brief 解析网络帧,封装成业务 Message
     * @param frame 网络帧
     * @param n_body 网络帧长度
     * @param msgtype 网络帧类型
     * @return 业务 Message
    */
    static MessagePtr decode_wire_packet(const std::uint8_t *frame,
                                         std::uint32_t n_body,
                                         MSG_TYPE msgtype);

    /** @brief 流式解帧器 */
    class WireStreamParser {
    public:
        /** @brief 重置流式解帧器,清空缓冲区 */
        void reset();
        /** @brief 喂入数据,将数据添加到缓冲区 */
        std::vector<MessagePtr> feed(const std::uint8_t *data, std::size_t len);

    private:
        std::vector<MessagePtr> extract_all(); ///< 提取所有消息

        QByteArray buffer_; ///< 缓冲区
        static constexpr std::size_t k_max_buffer = 4 * 1024 * 1024; ///< 最大缓冲区大小 4mb
    };

private:
    static MSG_TYPE to_wire_type(MessageKind kind); ///< 将消息类型MessageKind转换为网络帧类型MSG_TYPE
    static MessageKind from_wire_type(MSG_TYPE type); ///< 将网络帧类型MSG_TYPE转换为消息类型MessageKind
};

#endif // MESSAGECODEC_H
