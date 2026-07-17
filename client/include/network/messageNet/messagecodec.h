#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "message.h"
#include "netheader.h"
#include <QByteArray>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief 业务 Message 与线上协议帧之间的编解码，不涉及 socket 读写
 */
class MessageCodec
{
public:
    /**
     * @brief 将 Message 编码为线上帧：$ + type + ip + [len] + body + #
     * @param msg 业务消息
     * @param localIp 本机 IP
     * @return 编码后的帧
     */
    static QByteArray encodeWireFrame(const Message &msg, std::uint32_t localIp);

    /**
     * @brief 解析完整线上帧，失败返回 nullopt
     * @param frame 帧数据指针
     * @param nBody 正文长度
     * @param msgtype 线上消息类型
     * @return 解析成功的 Message，失败为 nullopt
     */
    static std::optional<Message> decodeWirePacket(const std::uint8_t *frame,
                                                   std::uint32_t nBody,
                                                   MSG_TYPE msgtype);

    /**
     * @brief 流式解帧器：从字节流中提取完整消息
     */
    class WireStreamParser {
    public:
        /** @brief 重置内部缓冲区 */
        void reset();
        /**
         * @brief 喂入数据并提取所有完整消息
         * @param data 字节数据
         * @param len 字节长度
         * @return 解析出的消息列表
         */
        std::vector<Message> feed(const std::uint8_t *data, std::size_t len);

    private:
        /**
         * @brief 从缓冲区提取所有完整帧
         * @return 消息列表
         */
        std::vector<Message> extractAll();

        QByteArray m_buffer; ///< 未完成帧的缓冲
        static constexpr std::size_t kMaxBuffer = 4 * 1024 * 1024; ///< 最大缓冲字节数
    };

private:
    /**
     * @brief Message::Kind 转为线上 MSG_TYPE
     * @param kind 业务消息类型
     * @return 线上类型
     */
    static MSG_TYPE toWireType(Message::Kind kind);
    /**
     * @brief 线上 MSG_TYPE 转为 Message::Kind
     * @param type 线上类型
     * @return 业务消息类型
     */
    static Message::Kind fromWireType(MSG_TYPE type);
};

#endif // MESSAGECODEC_H
