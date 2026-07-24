#ifndef MEETING_WIDGET_H
#define MEETING_WIDGET_H

#include "AudioInput.h"
#include "frameless_window.h"
#include "meeting_controller.h"
#include <QVideoFrame>
#include <QCamera>
#include <QMediaCaptureSession>
#include <QThread>
#include "networkmanager.h"
#include "message.h"
#include "partner.h"
#include "partner_tile.h"
#include <cstdint>
#include <qtmetamacros.h>
#include <unordered_map>
#include <vector>
#include "AudioOutput.h"
#include "chatmessage.h"
#include <QSoundEffect>
#include <QCloseEvent>
#include <QEvent>
#include <memory>
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
class MeetingWidget : public FramelessWindow<QWidget>
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
    bool _connecting = false; ///< 是否正在异步连接服务器
    bool _hasPendingConnect = false; ///< 断线未完成时是否有延后连接请求
    QString _pendingConnectIp;
    QString _pendingConnectPort;
    ConnectAction _pendingConnectAction = ConnectAction::None;
    QString _pendingConnectRoomNo;
    std::shared_ptr<NetworkManager> _network; ///< 统一网络收发（与 controller 共享）
    std::unique_ptr<MeetingController> _controller; ///< 业务控制器（独立线程）
    std::unique_ptr<QThread> _controller_thread; ///< 业务工作线程
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
    void init_ui();
    /** @brief 初始化信号与槽连接 */
    void init_connect();
    /**
     * @brief 连接 Partner 点击信号（由 PartnerTile 转发）
     * @param p 成员对象
     */
     void init_partner_connect(Partner *p);


    /** @brief 初始化网络与音视频工作对象 */
    void init_permanent_workers();
    /** @brief 结束会议会话,退出会议的时候调用（停采集、异步断线、清 UI） */
    void end_meeting_session();
    /** @brief 重置会议相关 UI 到初始态 */
    void reset_meeting_ui();
    /** @brief 刷新「会议信息」页显示 */
    void update_meeting_info();
    /** @brief 关闭并回收所有工作线程, 只有在释放窗口的时候使用*/
    void shutdown_all_workers();
    /**
     * @brief 添加房间成员
     * @param ip 成员 IP（主机序）
     * @return 新建的 Partner；已存在则返回 nullptr
     */
    Partner* add_partner(std::uint32_t ip);
    /**
     * @brief 移除房间成员
     * @param ip 成员 IP
     */
    void remove_partner(std::uint32_t ip);
    /** @brief 清空全部成员（退出会议） */
    void clear_partner();
    /**
     * @brief 关闭指定 IP 的视频画面（显示头像）
     * @param ip 成员 IP
     */
    void close_img(std::uint32_t ip);
    /**
     * @brief 向聊天列表追加一条消息气泡
     * @param messageW 消息控件
     * @param item 列表项
     * @param text 正文
     * @param time 时间戳字符串
     * @param ip 发送方 IP 字符串
     * @param type 消息角色
     */
    void deal_message(ChatMessage *messageW, QListWidgetItem *item, QString text, QString time, QString ip, ChatMessage::User_Type type);
    /**
     * @brief 按需插入时间分隔行
     * @param curMsgTime 当前消息时间戳
     */
    void deal_message_time(QString curMsgTime);
    /** @brief 按列表宽度重新排版全部聊天气泡 */
    void relayout_chat_messages();
    /** @brief 处理创建会议响应 */
    void handle_create_meeting_response(const MessagePtr &msg);
    /** @brief 处理加入会议响应 */
    void handle_join_meeting_response(const MessagePtr &msg);
    /** @brief 处理收到的视频帧 */
    void handle_img_recv(const MessagePtr &msg);
    /** @brief 处理收到的文本 */
    void handle_text_recv(const MessagePtr &msg);
    /** @brief 处理成员加入 */
    void handle_partner_join(const MessagePtr &msg);
    /** @brief 处理成员退出 */
    void handle_partner_exit(const MessagePtr &msg);
    /** @brief 处理对方关闭摄像头通知 */
    void handle_close_camera(const MessagePtr &msg);
    /** @brief 处理一次性成员列表（PartnerJoin2） */
    void handle_partner_join2(const MessagePtr &msg);
    /** @brief 处理远端主机关闭连接 */
    void handle_remote_host_closed_error();
    /** @brief 处理其它网络错误 */
    void handle_other_net_error();

public:
    /**
     * @brief 构造会议窗口
     * @param parent 父控件，一般为 nullptr（独立顶层窗）
     */
    explicit MeetingWidget(QWidget *parent = nullptr);
    ~MeetingWidget() override;

protected:
    /** @brief 窗口关闭事件,点击窗口叉号的时候调用 */
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;




    

    
public slots:
    /** @brief 点击创建会议 */
    void on_create_meet_btn_clicked_slot();
    /** @brief 点击开关摄像头 */
    void on_open_vedio_clicked_slot();
    /** @brief 点击开关麦克风 */
    void on_open_audio_clicked_slot();
    /**
     * @brief 请求连接服务器,MeetingController 线程完成
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 连接成功后的后续动作
     * @param room_no 加入会议时的房间号（仅 JoinMeeting 使用）
     */
    void request_connect_to_server_slot(QString ip, QString port, ConnectAction action, QString room_no = QString());
    /** @brief 断开服务器连接（异步） */
    void on_disconnect_server_slot();
    /**
     * @brief 加入指定房间
     * @param room_no 房间号字符串
     */
    void on_join_meet_btn_slot(QString room_no);

private slots:
    /**
     * @brief 工作线程连接完成回调（UI 线程）
     * @param ok 是否成功
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 后续动作
     * @param room_no 房间号
     */
    void on_connect_finished_slot(bool ok, QString ip, QString port, ConnectAction action, QString room_no);
    /**
     * @brief 音量滑条变化
     * @param value 音量值
     */
    void on_horizontal_slider_value_changed_slot(int value);
    /**
     * @brief 音频设备错误提示
     * @param err 错误信息
     */
    void audio_error_slot(QString err);
    void on_request_message_slot(MessagePtr msg);
    void on_user_info_message_slot(MessagePtr msg);
    void on_text_message_slot(MessagePtr msg);
    void on_video_message_slot(MessagePtr msg);
    /**
     * @brief 将指定成员切换到主屏幕
     * @param ip 成员 IP
     */
    void on_recv_ip_slot(std::uint32_t ip);
    /**
     * @brief 本地摄像头帧就绪，编码发送
     * @param image 帧图像
     */
    void on_local_frame_captured_slot(const QImage &image);
    /**
     * @brief 更新「当前说话」显示
     * @param ip 说话者 IP 字符串
     */
    void on_speaks_slot(QString ip);
    /** @brief 发送聊天消息 */
    void on_send_msg_clicked_slot();
    /** @brief 文本发送完成回调 */
    void on_text_send_slot();
    /** @brief 异步断线完成 */
    void on_network_disconnected_slot();
    /** @brief 若有延后连接请求则立刻发起 */
    void flush_pending_connect();
signals:
    void stop_audio_signal(); ///< 通知停止麦克风采集
    void start_audio_signal(); ///< 通知开始麦克风采集
    void volumn_change_signal(int); ///< 音量变化
    /**
     * @brief 请求工作线程连接服务器
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 后续动作
     * @param room_no 房间号
     */
    void request_connect_signal(QString ip, QString port, ConnectAction action, QString room_no);
    /** @brief 请求工作线程创建会议 */
    void create_meeting_requested_signal();
    /**
     * @brief 请求工作线程加入会议
     * @param room_no 房间号
     */
    void join_meeting_requested_signal(QString room_no);
    /**
     * @brief 连接结果通知主窗口（UI 线程）
     * @param ok 是否成功
     * @param ip 服务器 IP
     * @param port 端口
     * @param action 后续动作
     */
    void connect_server_finished_signal(bool ok, QString ip, QString port, ConnectAction action);

private:
    Ui::Widget *ui; ///< Designer 生成的 UI
};
#endif // MEETING_WIDGET_H
