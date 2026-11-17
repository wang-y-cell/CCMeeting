#ifndef APP_MEETING_SESSION_H
#define APP_MEETING_SESSION_H

#include "networkmanager.h"

#include <QObject>
#include <memory>

class MeetingVideo;
class ChatController;
class MeetingController;
class MeetingMediaController;
class MeetingProtocolDispatcher;
class MeetingRecorder;
class MeetingRtcObserver;
class MyTextEdit;
class ParticipantsController;
class QListWidget;
class QPushButton;
class QThread;
class QVBoxLayout;
class QWidget;
class RoomSettingsController;

namespace app {

/**
 * @brief 会议窗口交给会话层的「视图端口」
 *
 * 只借出指针，转移所有权一次性说明白：控件归窗口，会话层只使用。
 */
struct MeetingViewPorts {
    QVBoxLayout *tiles_layout = nullptr;     ///< 成员卡片容器
    MeetingVideo *video = nullptr;            ///< 画面路由（主画面 / 成员小窗）
    QListWidget *chat_list = nullptr;        ///< 聊天消息列表
    MyTextEdit *chat_input = nullptr;        ///< 聊天输入框
    QWidget *owner = nullptr;                ///< 弹窗/提示的父窗口
};

/**
 * @brief 会议会话装配层：网络、工作线程、控制器、协议分发的唯一创建者
 *
 * 背景：这些对象原先由 ui::MeetingWindow 自己 new、自己 moveToThread、
 * 自己维护 20 多条连接，导致「窗口一实例化就必须建网络和线程」，
 * 既无法离线预览，也无法对业务路径做单测。
 *
 * 现在装配与生命周期只在这里发生：
 *   - 构造：按固定顺序创建界面辅助对象（录制/房间设置/媒体/成员/聊天）
 *           与网络工作栈（NetworkManager + QThread + MeetingController + 协议分发）
 *   - 析构 / stop_workers()：停发送线程、停网络、按构造逆序释放辅助对象
 *
 * 窗口只保留「视图 + 视图事件连接」，通过访问器使用这些对象。
 * 所有成员都在 UI 线程创建；只有 MeetingController 被 moveToThread。
 */
class MeetingSession : public QObject {
    Q_OBJECT
public:
    /**
     * @param ports  视图端口（控件由窗口持有，本类不接管）
     * @param parent 父对象（通常为归属的窗口）
     */
    explicit MeetingSession(const MeetingViewPorts &ports,
                            QObject *parent = nullptr);
    ~MeetingSession() override;

    MeetingSession(const MeetingSession &) = delete;
    MeetingSession &operator=(const MeetingSession &) = delete;

    // ---- 访问器：对象均由本类持有，调用方只使用不释放 ----
    NetworkManager *network() const { return _network.get(); }
    MeetingController *controller() const { return _controller.get(); }
    MeetingProtocolDispatcher *protocol() const { return _protocol; }
    ParticipantsController *participants() const { return _participants; }
    ChatController *chat() const { return _chat; }
    RoomSettingsController *room_settings() const { return _room_settings; }
    MeetingMediaController *media() const { return _media; }
    MeetingRtcObserver *rtc_observer() const { return _rtc_observer; }
    MeetingRecorder *recorder() const { return _recorder; }

    /** @brief 网络工作栈是否就绪（stop_workers 之后为 false） */
    bool workers_running() const { return _network != nullptr; }

    /**
     * @brief 停止网络与工作线程（幂等，析构时自动调用）
     *
     * 只停工作栈，不释放界面辅助对象：它们的析构依赖窗口仍存活，
     * 因此放在本类析构函数里做。
     */
    void stop_workers();

private:
    /** @brief 创建不依赖网络工作栈的界面辅助对象 */
    void create_helpers();
    /** @brief 创建网络 + 工作线程 + 控制器 + 协议分发 */
    void start_workers();

    MeetingViewPorts _ports;
    std::shared_ptr<NetworkManager> _network;   ///< 网络统一入口（与控制器共享所有权）
    std::unique_ptr<QThread> _worker_thread;    ///< MeetingController 所在线程
    std::unique_ptr<MeetingController> _controller; ///< 运行在工作线程的会议控制器

    MeetingProtocolDispatcher *_protocol = nullptr;
    ParticipantsController *_participants = nullptr;
    ChatController *_chat = nullptr;
    RoomSettingsController *_room_settings = nullptr;
    MeetingMediaController *_media = nullptr;
    MeetingRtcObserver *_rtc_observer = nullptr;
    MeetingRecorder *_recorder = nullptr;

    bool _workers_stopped = false;
};

}  // namespace app

#endif  // APP_MEETING_SESSION_H
