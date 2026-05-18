#include "video/myvideosurface.h"
#include <QDebug>

MyVideoSurface::MyVideoSurface(QObject *parent)
    : QObject(parent)
{
    m_videoSink = new QVideoSink(this);
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &MyVideoSurface::handleVideoFrame);
}

QVideoSink* MyVideoSurface::getVideoSink() const
{
    return m_videoSink;
}

void MyVideoSurface::handleVideoFrame(const QVideoFrame &frame)
{
    if (frame.isValid())
    {
        QVideoFrame cloneFrame(frame);
        emit frameAvailable(cloneFrame);
    }
}
