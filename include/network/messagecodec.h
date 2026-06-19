#ifndef MESSAGECODEC_H
#define MESSAGECODEC_H

#include "network/netheader.h"
#include <QImage>
#include <QString>
#include <QByteArray>

/** 业务数据与 MESG 协议包之间的编解码，不涉及 socket 读写。 */
class MessageCodec
{
public:
    static MESG *encodeControl(MSG_TYPE type);
    static MESG *encodeJoinMeeting(quint32 roomNo);
    static MESG *encodeText(const QString &text);
    static MESG *encodeImage(const QImage &image);

    static QByteArray decodeImageWirePayload(const QByteArray &wireBody);
    static QByteArray decodeTextWirePayload(const QByteArray &wireBody);
    static QByteArray decodeAudioWirePayload(const QByteArray &wireBody);

    static QImage decodeImageMessage(const MESG *msg);
    static QString decodeTextMessage(const MESG *msg);
};

#endif // MESSAGECODEC_H
