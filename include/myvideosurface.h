#ifndef MYVIDEOSURFACE_H
#define MYVIDEOSURFACE_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>

class MyVideoSurface : public QObject
{
    Q_OBJECT
public:
    MyVideoSurface(QObject *parent = nullptr); //构造函数
    QVideoSink* getVideoSink() const; //获取视频输出

private:
    QVideoSink *m_videoSink; //视频输出

public slots:
    void handleVideoFrame(const QVideoFrame &frame); //处理视频帧

signals:
    void frameAvailable(QVideoFrame); //视频帧可用信号
};

#endif // MYVIDEOSURFACE_H
