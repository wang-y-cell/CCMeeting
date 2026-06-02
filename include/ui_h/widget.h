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
#include <QCloseEvent>

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
    static QRect pos; //窗口位置
    quint32 mainip; //主屏幕IP
    QCamera *_camera; //摄像头
    QMediaCaptureSession _captureSession;
    bool  _createmeet; //是否创建会议
    bool _joinmeet; //是否加入会议
    bool _openCamera; //是否打开摄像头
    bool _sessionActive; //是否已连接服务器（会议会话）

    void initPermanentWorkers();
    void endMeetingSession();
    void resetMeetingUi();
    void shutdownAllWorkers();

    MyVideoSurface *_myvideosurface; //视频表面

    QVideoFrame mainshow; //主屏幕视频

    SendImg *_sendImg; //发送图像

    RecvSolve *_recvThread; //接收线程
    SendText * _sendText;
    // socket
    MyTcpSocket *_mytcpSocket; //socket
    void paintEvent(QPaintEvent *event) override;

    QMap<quint32, Partner *> partner; //用于记录房间用户
    Partner* addPartner(quint32); //添加用户
    void removePartner(quint32); //删除用户
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(quint32); //根据IP关闭图像

    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间

    void handleCreateMeetingResponse(MESG *msg);//创建会议响应
    void handleJoinMeetingResponse(MESG *msg);//加入会议响应
    void handleImgRecv(MESG *msg);//图像接收
    void handleTextRecv(MESG *msg);//文本接收
    void handlePartnerJoin(MESG *msg);//人员加入
    void handlePartnerExit(MESG *msg);//人员退出
    void handleCloseCamera(MESG *msg);//关闭摄像头
    void handlePartnerJoin2(MESG *msg);//人员加入2
    void handleRemoteHostClosedError();//远程主机关闭错误
    void handleOtherNetError();//其他网络错误

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

protected:
    void closeEvent(QCloseEvent *event) override;

public slots:
    void on_createmeetBtn_clicked(); //点击创建会议按钮
    //void on_exitmeetBtn_clicked(); //点击退出会议按钮
    void on_openVedio_clicked(); //点击打开视频按钮
    void on_openAudio_clicked();  //点击打开音频按钮
    bool on_connServer(QString ip, QString port); //点击连接服务器按钮，成功返回 true
    void on_joinmeetBtn(QString roomNo); //点击加入会议按钮
private slots:
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
