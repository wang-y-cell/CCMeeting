#ifndef WIDGET_H
#define WIDGET_H

#include "AudioInput.h"
#include "frameless_window.h"
#include <QVideoFrame>
#include <QCamera>
#include <QMediaCaptureSession>
#include "networkmanager.h"
#include "message.h"
#include "partner.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "AudioOutput.h"
#include "chatmessage.h"
#include <QSoundEffect>
#include <QCloseEvent>
#include <QEvent>
#include "cameravideo.h"


QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QListWidgetItem;


class Widget : public FramelessWindow<QWidget>
{
    Q_OBJECT
private:
    static QRect pos; //窗口位置
    std::uint32_t mainip; //主屏幕IP
    bool  _createmeet = false; //是否创建会议
    bool _joinmeet = false; //是否加入会议
    bool _openCamera = false; //是否打开摄像头
    bool _sessionActive = false; //是否已连接服务器（会议会话）
    NetworkManager *_network = nullptr; //统一网络收发
    std::unordered_map<std::uint32_t, Partner *> partner; //用于记录房间用户
    AudioInput* _ainput = nullptr;
    QThread* _ainputThread = nullptr;
    AudioOutput* _aoutput = nullptr;
    std::vector<QString> iplist;
    QSoundEffect* _soundEffect = nullptr;
    int m_lastChatListWidth = -1;
    bool m_inChatRelayout = false;
    CameraVideo *_cameraVideo = nullptr; //统一处理图像/视频显示
    int _roomNo = 0; //当前会议房间号
    QString _serverAddr; //当前服务器地址 ip:port
private:
    void initUI(); //初始化UI
    void initConnect(); //初始化信号与槽
    void initPartnerConnect(Partner *p); //连接 Partner 信号
    void paintEvent(QPaintEvent *event) override;
    void initPermanentWorkers();//初始化永久工作线程
    void endMeetingSession(); //结束会议会话
    void resetMeetingUi(); //重置会议UI
    void updateMeetingInfo(); //刷新会议信息页
    void shutdownAllWorkers(); //关闭所有工作线程
   Partner* addPartner(std::uint32_t); //添加用户
    void removePartner(std::uint32_t); //删除用户
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(std::uint32_t); //根据IP关闭图像
    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间
    void relayoutChatMessages();
    void handleCreateMeetingResponse(const Message &msg);
    void handleJoinMeetingResponse(const Message &msg);
    void handleImgRecv(const Message &msg);
    void handleTextRecv(const Message &msg);
    void handlePartnerJoin(const Message &msg);
    void handlePartnerExit(const Message &msg);
    void handleCloseCamera(const Message &msg);
    void handlePartnerJoin2(const Message &msg);
    void handleRemoteHostClosedError();//远程主机关闭错误
    void handleOtherNetError();//其他网络错误

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    void on_createmeetBtn_clicked(); //点击创建会议按钮
    void on_openVedio_clicked(); //点击打开视频按钮
    void on_openAudio_clicked();  //点击打开音频按钮
    bool on_connServer(QString ip, QString port); //点击连接服务器按钮，成功返回 true
    void on_disconnectServer(); //断开服务器连接
    void on_joinmeetBtn(QString roomNo); //点击加入会议按钮
private slots:
    void on_horizontalSlider_valueChanged(int value); //音量改变
    void audioError(QString); //音频错误处理
    void onRequestMessage(Message msg);
    void onUserInfoMessage(Message msg);
    void onTextMessage(Message msg);
    void onVideoMessage(Message msg);
    void recvip(std::uint32_t); //接收IP
    void onLocalFrameCaptured(const QImage &image);
    void speaks(QString); //说话
    void on_sendmsg_clicked(); //发送消息
    void textSend(); //发送文本
signals:
    void stopAudio();
    void startAudio();
    void volumnChange(int);
private:
    Ui::Widget *ui;
};
#endif // WIDGET_H
