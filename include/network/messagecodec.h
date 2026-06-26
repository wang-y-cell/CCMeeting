#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "netheader.h"
#include <QByteArray>
#include <QImage>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

/** 业务数据与 MESG 协议包之间的编解码，不涉及 socket 读写。 */
class MessageCodec
{
public:
    enum class PacketQueue { Recv, Audio };

    struct ParsedPacket {
        MESG *msg = nullptr;
        PacketQueue queue = PacketQueue::Recv;
    };

    /**编码控制消息*/
    static MESG *encodeControl(MSG_TYPE type);
    /**编码加入会议消息*/
    static MESG *encodeJoinMeeting(std::uint32_t roomNo);
    /**编码文本消息*/
    static MESG *encodeText(const std::string &text);
    /**编码图片消息*/
    static MESG *encodeImage(const QImage &image);
    /**编码网络错误消息*/
    static MESG *encodeNetworkError(MSG_TYPE type);

    /** 将 MESG 编码为线上帧：$ + type + ip + [len] + body + # */
    static QByteArray encodeWireFrame(const MESG *send, std::uint32_t localIp);

    /*解码图片数据,base64解码后，再解压缩*/
    static QByteArray decodeImageWirePayload(const QByteArray &wireBody);
    /*解码文本数据,直接解压缩*/
    static QByteArray decodeTextWirePayload(const QByteArray &wireBody);
    /*解码音频数据,base64解码后，再解压缩*/
    static QByteArray decodeAudioWirePayload(const QByteArray &wireBody);

    /*解码图片消息,直接解压缩*/
    static QImage decodeImageMessage(const MESG *msg);
    /*解码文本消息,直接解压缩*/
    static std::string decodeTextMessage(const MESG *msg);

    /** 
    *@brief 解析完整线上帧（不含 '$' 与 '#' 外的流式缓冲），返回 nullptr 表示忽略该包
    *@param frame 线上帧数据
    *@param nBody 线上帧数据长度
    *@param msgtype 线上帧类型
    *@return 解析出的MESG
    */
    static std::optional<ParsedPacket> decodeWirePacket(const std::uint8_t *frame,
                                                        std::uint32_t nBody,
                                                        MSG_TYPE msgtype);

    /** 流式解帧：feed 追加字节，返回本次解析出的全部 MESG。 */
    class WireStreamParser {
    public:
        /*重置流式缓冲*/
        void reset();
        /** 
        *@brief 追加字节,并解析m_buffer中所有的MESG
        *@param data 字节数据
        *@param len 字节长度
        *@return 解析出的MESG
        */
        std::vector<ParsedPacket> feed(const std::uint8_t *data, std::size_t len);

    private:
        /**
        *@brief 提取m_buffer中所有的MESG
        *@return 解析出的MESG
        */
        std::vector<ParsedPacket> extractAll();

        QByteArray m_buffer; /*流式缓冲*/
        static constexpr std::size_t kMaxBuffer = 4 * 1024 * 1024; /*最大缓冲区大小*/
    };
};

#endif // MESSAGECODEC_H
