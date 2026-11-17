#include "app/meeting_session.h"

#include "media/video/meeting_video.h"
#include "features/meeting/chat_controller.h"
#include "features/meeting/meeting_controller.h"
#include "features/meeting/meeting_media_controller.h"
#include "features/meeting/meeting_protocol_dispatcher.h"
#include "media/recording/meeting_recorder.h"
#include "features/meeting/meeting_rtc_observer.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/room_settings_controller.h"
#include "protocol/message.h"

#include <QListWidget>
#include <QThread>
#include <QVBoxLayout>
#include <spdlog/spdlog.h>

namespace app {

MeetingSession::MeetingSession(const MeetingViewPorts &ports, QObject *parent)
    : QObject(parent), _ports(ports) {
    qRegisterMetaType<MessagePtr>("MessagePtr");
    qRegisterMetaType<ConnectAction>("ConnectAction");

    spdlog::info("[MeetingSession] create helpers begin");
    create_helpers();
    spdlog::info("[MeetingSession] create helpers done");

    spdlog::info("[MeetingSession] start workers begin");
    start_workers();
    spdlog::info("[MeetingSession] start workers done");
}

MeetingSession::~MeetingSession() {
    stop_workers();

    // 辅助对象的析构依赖窗口仍存活（它们持有控件指针），
    // 所以顺序固定为：先断连接，再按构造逆序释放。
    const auto detach = [this](QObject *child) {
        if (child == nullptr) {
            return;
        }
        if (_ports.owner != nullptr) {
            disconnect(child, nullptr, _ports.owner, nullptr);
        }
    };

    detach(_media);
    detach(_participants);
    detach(_rtc_observer);
    detach(_room_settings);
    detach(_chat);
    detach(_protocol);

    // 媒体控制器持有引擎，必须先于观察者销毁
    delete _media;
    _media = nullptr;
    delete _participants;
    _participants = nullptr;
    delete _chat;
    _chat = nullptr;
    delete _room_settings;
    _room_settings = nullptr;
    delete _protocol;
    _protocol = nullptr;
    delete _rtc_observer;
    _rtc_observer = nullptr;
    delete _recorder;
    _recorder = nullptr;
}

void MeetingSession::create_helpers() {
    // 录制器由本类持有（RoomSettingsController 只使用，不释放）
    _recorder = new MeetingRecorder(this);
    _room_settings = new RoomSettingsController(_recorder, _ports.owner, this);

    // 媒体层：观察者负责 xrtc 回调与帧合并，控制器负责引擎与本端音视频
    _rtc_observer = new MeetingRtcObserver(this);
    _media = new MeetingMediaController(_rtc_observer, _ports.owner, this);

    // 成员卡片 / 资料缓存 / 主画面选择
    _participants = new ParticipantsController(_ports.tiles_layout, _ports.video,
                                               this);

    // 聊天渲染 / @ 补全 / 提示音
    _chat = new ChatController(_ports.chat_list, _ports.chat_input, _ports.owner,
                               this);
}

void MeetingSession::start_workers() {
    _network = std::make_shared<NetworkManager>(nullptr);
    _worker_thread = std::make_unique<QThread>();
    _controller = std::make_unique<MeetingController>(_network);
    _controller->moveToThread(_worker_thread.get());
    _worker_thread->start();

    // 协议层：网络消息 → 语义信号（窗口只处理「发生了什么」）
    _protocol = new MeetingProtocolDispatcher(this);
}

void MeetingSession::stop_workers() {
    if (_workers_stopped) {
        return;
    }
    _workers_stopped = true;

    if (_network && _ports.owner != nullptr) {
        disconnect(_network.get(), nullptr, _ports.owner, nullptr);
    }
    if (_worker_thread) {
        _worker_thread->quit();
        _worker_thread->wait(3000);
    }
    _controller.reset();
    _worker_thread.reset();
    if (_network) {
        _network->stop();
    }
    _network.reset();
}

}  // namespace app
