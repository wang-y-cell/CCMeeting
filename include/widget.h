#ifndef WIDGET_H
#define WIDGET_H

#include "Audio/AudioInput.h"
#include <QWidget>
#include <QVideoFrame>
#include <QTcpSocket>
#include "network/mytcpsocket.h"
#include <QCamera>
#include <QMediaCaptureSession>
#include "network/sendtext.h"
#include "network/recvsolve.h"
#include "network/partner.h"
#include "network/netheader.h"
#include <QMap>
#include "Audio/AudioOutput.h"
#include "text/chatmessage.h"
#include "Img/ImgDisplay.h"
#include <QStringListModel>
#include <QSoundEffect>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QCameraImageCapture;
class MyVideoSurface;
class SendImg;
class QListWidgetItem;
class Widget : public QWidget
{
    Q_OBJECT
private:
    static QRect pos;
    quint32 mainip;
    QCamera *_camera;
    QMediaCaptureSession _captureSession;
    bool  _createmeet;
    bool _joinmeet;
    bool _openCamera;

    MyVideoSurface *_myvideosurface;

    QVideoFrame mainshow;

    SendImg *_sendImg;

    RecvSolve *_recvThread;
    SendText * _sendText;
    // socket
    MyTcpSocket *_mytcpSocket;
    void paintEvent(QPaintEvent *event);

    QMap<quint32, Partner *> partner; //用于记录房间用户
    Partner* addPartner(quint32);
    void removePartner(quint32);
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(quint32); //根据IP重置图像

    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间

    //音频
    AudioInput* _ainput;
    QThread* _ainputThread;
    AudioOutput* _aoutput;

    QStringList iplist;

    QSoundEffect* _soundEffect;

    ImgDisplay m_videoImg;   ///< 主画面视频：按宽高比缩放以适配整块 label（尽可能“铺满”可视区域）
    ImgDisplay m_avatarImg; ///< 占位头像：高度为 label 的固定比例并居中

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private slots:
    void on_createmeetBtn_clicked(); //点击创建会议按钮
    void on_exitmeetBtn_clicked(); //点击退出会议按钮
    void on_openVedio_clicked(); //点击打开视频按钮
    void on_openAudio_clicked();  //点击打开音频按钮
    void on_connServer_clicked(); //点击连接服务器按钮
    void on_joinmeetBtn_clicked(); //点击加入会议按钮

    void on_horizontalSlider_valueChanged(int value); //音量改变
    void cameraError(QCamera::Error error, const QString &errorString); //摄像头错误处理
    void audioError(QString); //音频错误处理
    void datasolve(MESG *); //数据处理
    void recvip(quint32); //接收IP
    void cameraImageCapture(QVideoFrame frame); //摄像头图像捕获
    void speaks(QString); //说话
    void on_sendmsg_clicked(); //发送消息
    void textSend(); //发送文本
signals:
    void pushImg(QImage);
    void PushText(MSG_TYPE, QString = "");
    void stopAudio();
    void startAudio();
    void volumnChange(int);
private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
