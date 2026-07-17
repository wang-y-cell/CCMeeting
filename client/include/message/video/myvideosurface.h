#ifndef MYVIDEOSURFACE_H
#define MYVIDEOSURFACE_H

#include <QObject>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImage>

/**
 * @brief 摄像头视频帧接收桥接：通过 QVideoSink 获取帧并转发
 */
class MyVideoSurface : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造视频表面
     * @param parent 父对象
     */
    MyVideoSurface(QObject *parent = nullptr);
    /**
     * @brief 获取视频输出 sink
     * @return QVideoSink 指针
     */
    QVideoSink* getVideoSink() const;

private:
    QVideoSink *m_videoSink; ///< 视频输出

public slots:
    /**
     * @brief 处理视频帧
     * @param frame 视频帧
     */
    void handleVideoFrame(const QVideoFrame &frame);

signals:
    /**
     * @brief 视频帧可用
     * @param frame 视频帧
     */
    void frameAvailable(QVideoFrame);
};

#endif // MYVIDEOSURFACE_H
