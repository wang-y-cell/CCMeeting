#ifndef WIDGET_H
#define WIDGET_H

#include "Audio/AudioInput.h"
#include <QWidget>
#include <QVideoFrame>
#include <QCamera>
#include <QMediaCaptureSession>
#include "network/partner.h"
#include "network/netheader.h"
#include "network/networkmanager.h"
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "Audio/AudioOutput.h"
#include "text/chatmessage.h"
#include <QSoundEffect>
#include <QCloseEvent>
#include <QEvent>
#include "video/cameravideo.h"


QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QListWidgetItem;


class Widget : public QWidget 
{
    Q_OBJECT
private:
    static QRect pos; //窗口位置
    std::uint32_t mainip; //主屏幕IP
    bool  _createmeet; //是否创建会议
    bool _joinmeet; //是否加入会议
    bool _openCamera; //是否打开摄像头
    bool _sessionActive; //是否已连接服务器（会议会话）
    NetworkManager *_network; //统一网络收发
    std::unordered_map<std::uint32_t, Partner *> partner; //用于记录房间用户
    AudioInput* _ainput;
    QThread* _ainputThread;
    AudioOutput* _aoutput;
    std::vector<QString> iplist;
    QSoundEffect* _soundEffect;
    int m_lastChatListWidth = -1;
    bool m_inChatRelayout = false;
    CameraVideo *_cameraVideo; //统一处理图像/视频显示

    void initUI(); //初始化UI
    void paintEvent(QPaintEvent *event) override;
    void initPermanentWorkers();//初始化永久工作线程
    void endMeetingSession(); //结束会议会话
    void resetMeetingUi(); //重置会议UI
    void shutdownAllWorkers(); //关闭所有工作线程
   Partner* addPartner(std::uint32_t); //添加用户
    void removePartner(std::uint32_t); //删除用户
    void clearPartner(); //退出会议，或者会议结束
    void closeImg(std::uint32_t); //根据IP关闭图像
    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip ,ChatMessage::User_Type type); //用户发送文本
    void dealMessageTime(QString curMsgTime); //处理时间
    void relayoutChatMessages();
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
    void on_joinmeetBtn(QString roomNo); //点击加入会议按钮
private slots:
    void on_horizontalSlider_valueChanged(int value); //音量改变
    void audioError(QString); //音频错误处理
    void datasolve(MESG *); //数据处理
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
