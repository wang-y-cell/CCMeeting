#include "network/messagecodec.h"
#include "logger/Logger.h"
#include <QBuffer>
#include <cstring>

MESG *MessageCodec::encodeControl(MSG_TYPE type)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = type;
    send->len = 0;
    send->data = nullptr;
    return send;
}

MESG *MessageCodec::encodeJoinMeeting(quint32 roomNo)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = JOIN_MEETING;
    send->len = 4;
    send->data = static_cast<uchar *>(malloc(send->len + 10));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, send->len + 10);
    memcpy(send->data, &roomNo, sizeof(roomNo));
    return send;
}

MESG *MessageCodec::encodeText(const QString &text)
{
    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = TEXT_SEND;
    const QByteArray data = qCompress(QByteArray::fromStdString(text.toStdString()));
    send->len = data.size();
    send->data = static_cast<uchar *>(malloc(send->len));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, send->len);
    memcpy(send->data, data.data(), static_cast<size_t>(data.size()));
    return send;
}

MESG *MessageCodec::encodeImage(const QImage &image)
{
    QByteArray byte;
    QBuffer buf(&byte);
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "JPEG"))
        return nullptr;

    const QByteArray compressed = qCompress(byte);
    const QByteArray payload = compressed.toBase64();

    MESG *send = static_cast<MESG *>(malloc(sizeof(MESG)));
    if (!send)
        return nullptr;
    memset(send, 0, sizeof(MESG));
    send->msg_type = IMG_SEND;
    send->len = payload.size();
    send->data = static_cast<uchar *>(malloc(send->len));
    if (!send->data) {
        free(send);
        return nullptr;
    }
    memset(send->data, 0, send->len);
    memcpy(send->data, payload.data(), static_cast<size_t>(payload.size()));
    return send;
}

QByteArray MessageCodec::decodeImageWirePayload(const QByteArray &wireBody)
{
    const QByteArray decoded = QByteArray::fromBase64(wireBody);
    return qUncompress(decoded);
}

QByteArray MessageCodec::decodeTextWirePayload(const QByteArray &wireBody)
{
    return qUncompress(wireBody);
}

QByteArray MessageCodec::decodeAudioWirePayload(const QByteArray &wireBody)
{
    const QByteArray decoded = QByteArray::fromBase64(wireBody);
    return qUncompress(decoded);
}

QImage MessageCodec::decodeImageMessage(const MESG *msg)
{
    QImage img;
    if (!msg || !msg->data || msg->len <= 0)
        return img;
    img.loadFromData(msg->data, msg->len);
    return img;
}

QString MessageCodec::decodeTextMessage(const MESG *msg)
{
    if (!msg || !msg->data || msg->len <= 0)
        return {};
    return QString::fromUtf8(reinterpret_cast<const char *>(msg->data), static_cast<int>(msg->len));
}
