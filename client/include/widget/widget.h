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
#include "partner_tile.h"
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

/**
 * @brief 视频会议主窗口
 *
 * 负责会议创建/加入、音视频收发、聊天与成员管理。
 * 继承无边框窗口，支持拖动、缩放与最大化。
 */
class Widget : public FramelessWindow<QWidget>
{
    Q_OBJECT
private:
    static QRect pos; ///< 窗口默认几何（相对屏幕）
    std::uint32_t mainip; ///< 主屏幕显示的用户 IP
    bool  _createmeet = false; ///< 是否已创建会议
    bool _joinmeet = false; ///< 是否已加入会议
    bool _openCamera = false; ///< 是否打开摄像头
    bool _sessionActive = false; ///< 是否已连接服务器（会议会话）
    bool _sessionEnding = false; ///< 是否正在异步结束会议
    bool _offlineMode = false; ///< 离线调试模式（不连服务器）
    NetworkManager *_network = nullptr; ///< 统一网络收发
    std::unordered_map<std::uint32_t, Partner *> partner; ///< 房间内成员（IP → Partner）
    AudioInput* _ainput = nullptr; ///< 麦克风采集
    QThread* _ainputThread = nullptr; ///< 音频采集线程
    AudioOutput* _aoutput = nullptr; ///< 扬声器播放
    std::vector<QString> iplist; ///< @ 补全用的成员列表
    QSoundEffect* _soundEffect = nullptr; ///< @ 提醒音效
    int m_lastChatListWidth = -1; ///< 上次聊天列表宽度（防抖）
    bool m_inChatRelayout = false; ///< 是否正在重排聊天气泡
    CameraVideo *_cameraVideo = nullptr; ///< 本地/远端图像显示
    int _roomNo = 0; ///< 当前会议房间号
    QString _serverAddr; ///< 当前服务器地址 ip:port

private:
    /** @brief 初始化界面布局与边距 */
    void initUI();
    /** @brief 初始化信号与槽连接 */
    void initConnect();
    /**
     * @brief 连接 Partner 点击信号（由 PartnerTile 转发）
     * @param p 成员对象
     */
    void initPartnerConnect(Partner *p);
    void paintEvent(QPaintEvent *event) override;
    /** @brief 初始化网络与音视频工作对象 */
    void initPermanentWorkers();
    /** @brief 结束会议会话（停采集、异步断线、清 UI） */
    void endMeetingSession();
    /** @brief 重置会议相关 UI 到初始态 */
    void resetMeetingUi();
    /** @brief 刷新「会议信息」页显示 */
    void updateMeetingInfo();
    /** @brief 关闭并回收所有工作线程 */
    void shutdownAllWorkers();
    /**
     * @brief 添加房间成员
     * @param ip 成员 IP（主机序）
     * @return 新建的 Partner；已存在则返回 nullptr
     */
    Partner* addPartner(std::uint32_t ip);
    /**
     * @brief 移除房间成员
     * @param ip 成员 IP
     */
    void removePartner(std::uint32_t ip);
    /** @brief 清空全部成员（退出会议） */
    void clearPartner();
    /**
     * @brief 关闭指定 IP 的视频画面（显示头像）
     * @param ip 成员 IP
     */
    void closeImg(std::uint32_t ip);
    /**
     * @brief 向聊天列表追加一条消息气泡
     * @param messageW 消息控件
     * @param item 列表项
     * @param text 正文
     * @param time 时间戳字符串
     * @param ip 发送方 IP 字符串
     * @param type 消息角色
     */
    void dealMessage(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip, ChatMessage::User_Type type);
    /**
     * @brief 按需插入时间分隔行
     * @param curMsgTime 当前消息时间戳
     */
    void dealMessageTime(QString curMsgTime);
    /** @brief 按列表宽度重新排版全部聊天气泡 */
    void relayoutChatMessages();
    /** @brief 处理创建会议响应 */
    void handleCreateMeetingResponse(const Message &msg);
    /** @brief 处理加入会议响应 */
    void handleJoinMeetingResponse(const Message &msg);
    /** @brief 处理收到的视频帧 */
    void handleImgRecv(const Message &msg);
    /** @brief 处理收到的文本 */
    void handleTextRecv(const Message &msg);
    /** @brief 处理成员加入 */
    void handlePartnerJoin(const Message &msg);
    /** @brief 处理成员退出 */
    void handlePartnerExit(const Message &msg);
    /** @brief 处理对方关闭摄像头通知 */
    void handleCloseCamera(const Message &msg);
    /** @brief 处理一次性成员列表（PartnerJoin2） */
    void handlePartnerJoin2(const Message &msg);
    /** @brief 处理远端主机关闭连接 */
    void handleRemoteHostClosedError();
    /** @brief 处理其它网络错误 */
    void handleOtherNetError();

public:
    /**
     * @brief 构造会议窗口
     * @param parent 父控件，一般为 nullptr（独立顶层窗）
     */
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

public slots:
    /** @brief 点击创建会议 */
    void on_createmeetBtn_clicked();
    /**
     * @brief 进入离线调试模式：不连服务器，预置假成员，便于测 UI / 摄像头
     */
    void enterOfflineMode();
    /** @brief 点击开关摄像头 */
    void on_openVedio_clicked();
    /** @brief 点击开关麦克风 */
    void on_openAudio_clicked();
    /**
     * @brief 连接服务器
     * @param ip 服务器 IP
     * @param port 端口
     * @return 连接成功返回 true
     */
    bool on_connServer(QString ip, QString port);
    /** @brief 断开服务器连接（异步） */
    void on_disconnectServer();
    /**
     * @brief 加入指定房间
     * @param roomNo 房间号字符串
     */
    void on_joinmeetBtn(QString roomNo);

private slots:
    /**
     * @brief 音量滑条变化
     * @param value 音量值
     */
    void on_horizontalSlider_valueChanged(int value);
    /**
     * @brief 音频设备错误提示
     * @param err 错误信息
     */
    void audioError(QString err);
    void onRequestMessage(Message msg);
    void onUserInfoMessage(Message msg);
    void onTextMessage(Message msg);
    void onVideoMessage(Message msg);
    /**
     * @brief 将指定成员切换到主屏幕
     * @param ip 成员 IP
     */
    void recvip(std::uint32_t ip);
    /**
     * @brief 本地摄像头帧就绪，编码发送
     * @param image 帧图像
     */
    void onLocalFrameCaptured(const QImage &image);
    /**
     * @brief 更新「当前说话」显示
     * @param ip 说话者 IP 字符串
     */
    void speaks(QString ip);
    /** @brief 发送聊天消息 */
    void on_sendmsg_clicked();
    /** @brief 文本发送完成回调 */
    void textSend();
    /** @brief 异步断线完成 */
    void onNetworkDisconnected();

signals:
    void stopAudio(); ///< 通知停止麦克风采集
    void startAudio(); ///< 通知开始麦克风采集
    void volumnChange(int); ///< 音量变化

private:
    Ui::Widget *ui; ///< Designer 生成的 UI
};
#endif // WIDGET_H
