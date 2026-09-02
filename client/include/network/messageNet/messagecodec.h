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
     * @param local_user_id 本地用户 ID
     * @return 网络帧
    */
    static QByteArray encode_wire_frame(const Message &msg, qint64 local_user_id);

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
        /** @brief 喂入数据,将数据添加到缓冲区,并尽可能解析里面完整的数据包 */
        std::vector<MessagePtr> feed(const std::uint8_t *data, std::size_t len);

    private:
        /// @brief 尽可能解析缓冲区中的完整的包,如果缓冲区没有完整的包也没事,留着下次解析
        /// 连头部都不够 break，返回空 vector，半数据留在 buffer_
        /// 头够了但整包不够 break，返回空（或前面已解出的包），半包留下
        /// 正好一包或多包 解出来放进返回值，已消费部分从 buffer_ 删掉
        /// 多包 + 后面半包 完整的都解出，半包留到下次
        std::vector<MessagePtr> extract_all(); ///< 提取所有消息

        QByteArray buffer_; ///< 缓冲区
        static constexpr std::size_t k_max_buffer = 4 * 1024 * 1024; ///< 最大缓冲区大小 4mb
    };

private:
    static MSG_TYPE to_wire_type(MessageKind kind); ///< 将消息类型MessageKind转换为网络帧类型MSG_TYPE
    static MessageKind from_wire_type(MSG_TYPE type); ///< 将网络帧类型MSG_TYPE转换为消息类型MessageKind
};

#endif // MESSAGECODEC_H
