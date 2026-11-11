#include "main_window.h"
#include "ui/foundation/theme_manager.h"
#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/login_store.h"
#include "configure/meeting_history.h"
#include "recent_meeting_list.h"
#include "configure/user_session.h"
#include "style_loader.h"

#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QMediaDevices>
#include <QMessageBox>
#include <QTimer>
#include <QMouseEvent>
#include "storage.h"
#include <QStackedWidget>
#include <spdlog/spdlog.h>

namespace {

QString meeting_server_host() {
    return ClientConfig::instance().meeting_server().host;
}

QString meeting_server_port() {
    return QString::number(ClientConfig::instance().meeting_server().port);
}

}  // namespace

main_window::main_window(QWidget *parent) : FramelessWindow<QWidget>(parent) {
    spdlog::info("[main_window] ctor begin");
    spdlog::default_logger()->flush();

    const auto &meeting = ClientConfig::instance().meeting_server();
    spdlog::info("[main_window] meeting server {}:{}",
                 meeting.host.toUtf8().constData(), meeting.port);
    spdlog::default_logger()->flush();

    init_ui();
    setWindowTitle(tr("CloudMeeting"));
    set_style();
    refreshUserCard();

    // 延迟创建 MeetingWidget：其 QSoundEffect 会加载 Qt Multimedia(FFmpeg)，
    // 过早加载后 WebRTC 摄像头枚举（设置页）在部分环境会直接崩溃。
    connect(create_meeting_widget, &stack_create_meet::createMeetingClicked,
            this, &main_window::CreateMeeting_button_clicked);
    if (join_meeting_widget != nullptr) {
        connect(join_meeting_widget, &ui::JoinMeetingPage::join_requested, this,
                &main_window::JoinMeeting_button_clicked);
    }
    if (ui.btnLogout) {
        connect(ui.btnLogout, &QPushButton::clicked, this,
                &main_window::onLogoutClicked);
    }
    connect(user_profile_widget, &stack_user_profile::backHomeRequested, this,
            &main_window::onProfileBackHome);
    connect(user_profile_widget, &stack_user_profile::avatarUpdated, this,
            &main_window::onProfileAvatarUpdated);

    // 预热 Qt Multimedia 设备枚举：首次 videoInputs() 冷启动 500ms+，
    // 放到主界面显示后的空闲时刻做完，之后打开设置页就不会再卡。
    QTimer::singleShot(5000, this, [this]() {
        QElapsedTimer timer;
        timer.start();
        const auto cameras = QMediaDevices::videoInputs();
        const auto mics = QMediaDevices::audioInputs();
        spdlog::info("[main_window] media devices warmed in {}ms (cameras={} "
                     "mics={})",
                     static_cast<long long>(timer.elapsed()), cameras.size(),
                     mics.size());
    });

    spdlog::info("[main_window] ctor done");
    spdlog::default_logger()->flush();
}

void main_window::set_style() {
    // 统一走令牌替换：QSS 里可以写 @primary / @canvas 等占位符（见 ui/foundation/design_tokens.h）
    ui::ThemeManager::instance().apply_to(
        this, QStringLiteral(":/Style/source/main_window.qss"));
}

void main_window::destroyMeetingWidget() {
    if (!widget) {
        return;
    }
    widget->hide();
    delete widget;
    widget = nullptr;
}

MeetingWidget *main_window::ensureMeetingWidget() {
    if (widget) {
        return widget;
    }
    spdlog::info("[main_window] lazy create MeetingWidget");
    spdlog::default_logger()->flush();
    widget = new MeetingWidget(nullptr);
    widget->hide();
    connect(widget, &MeetingWidget::connect_server_finished_signal, this,
            &main_window::onConnectServerFinished);
    connect(widget, &MeetingWidget::meeting_confirmed_signal, this,
            &main_window::onMeetingConfirmed);
    connect(widget, &MeetingWidget::meeting_closed_signal, this,
            &main_window::onMeetingClosed);
    spdlog::info("[main_window] MeetingWidget ready");
    spdlog::default_logger()->flush();
    return widget;
}

main_window::~main_window() {
    destroyMeetingWidget();
}

void main_window::closeEvent(QCloseEvent *event) {
    destroyMeetingWidget();
    FramelessWindow<QWidget>::closeEvent(event);
}

void main_window::refreshUserCard() {
    if (!ui.nameLabel || !ui.avatarLabel) {
        return;
    }

    const auto &session = UserSession::instance();
    const QString displayName =
        session.isLoggedIn() ? session.name() : tr("未登录");
    ui.nameLabel->setText(displayName);

    // 头像未就绪时显示姓名首字，避免侧栏出现一个空白圆圈
    const QString monogram =
        displayName.isEmpty() ? QStringLiteral("?") : displayName.left(1);
    ui.avatarLabel->setText(monogram);

    AvatarImageLoader::instance().load(
        session.avatar(), ui.avatarLabel->size(), this,
        [this, monogram](const QPixmap &pixmap) {
            if (!ui.avatarLabel) {
                return;
            }
            if (pixmap.isNull()) {
                ui.avatarLabel->setPixmap(QPixmap());
                ui.avatarLabel->setText(monogram);
            } else {
                ui.avatarLabel->setText(QString());
                ui.avatarLabel->setPixmap(pixmap);
            }
        });
}

bool main_window::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui.userCard && event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            onUserCardClicked();
            return true;
        }
    }
    return FramelessWindow<QWidget>::eventFilter(watched, event);
}

void main_window::onUserCardClicked() {
    if (!UserSession::instance().isLoggedIn()) {
        return;
    }
    if (user_profile_widget) {
        user_profile_widget->refreshFromSession();
    }
    if (ui.contentStack) {
        ui.contentStack->setCurrentWidget(user_profile_widget);
    }
}

void main_window::onProfileBackHome() {
    if (ui.contentStack && create_meeting_widget) {
        ui.contentStack->setCurrentWidget(create_meeting_widget);
    }
}

void main_window::onProfileAvatarUpdated() {
    refreshUserCard();
}

void main_window::register_pages() {
    // 侧栏顺序 = order 顺序，必须是 create(0) join(1) history(2)
    // recordings(3) settings(4)，与 sideNav 的行号一一对应。
    // 历史遗留页面暂时仍用具体类型；迁移到 ui/pages/ 时只改工厂这一行。
    m_pages.add({QStringLiteral("create"), tr("创建会议"),
                 QStringLiteral("video"), 0, true},
                [this](QWidget *) { return new stack_create_meet(this); });
    m_pages.add({QStringLiteral("join"), tr("加入会议"),
                 QStringLiteral("users"), 10, true},
                [this](QWidget *) { return new ui::JoinMeetingPage(this); });
    m_pages.add({QStringLiteral("history"), tr("会议记录"),
                 QStringLiteral("calendar"), 20, true},
                [this](QWidget *) { return new stack_meeting_history(this); });
    m_pages.add({QStringLiteral("recordings"), tr("录制"),
                 QStringLiteral("record"), 30, true},
                [this](QWidget *) { return new stack_recordings(this); });
    m_pages.add({QStringLiteral("settings"), tr("设置"),
                 QStringLiteral("settings"), 40, true},
                [this](QWidget *) { return new stack_setting(this); });

    // 不进侧栏、由用户卡片进入的页面
    m_pages.add({QStringLiteral("profile"), tr("个人资料"),
                 QStringLiteral("user"), 100, false},
                [this](QWidget *) { return new stack_user_profile(this); });
}

void main_window::build_pages() {
    // 1) 按注册表创建全部页面，并按注册顺序加入内容栈
    for (const ui::PageMeta &meta : m_pages.all()) {
        QWidget *page = m_pages.create(meta.id, this);
        if (page == nullptr) {
            continue;
        }
        ui.contentStack->addWidget(page);
        if (meta.in_nav) {
            m_nav_pages.append(page);
        }
    }

    // 2) 取出需要业务装配的具体类型（其余页面外壳不需要知道）
    create_meeting_widget =
        qobject_cast<stack_create_meet *>(m_pages.find(QStringLiteral("create")));
    join_meeting_widget = qobject_cast<ui::JoinMeetingPage *>(
        m_pages.find(QStringLiteral("join")));
    history_widget = qobject_cast<stack_meeting_history *>(
        m_pages.find(QStringLiteral("history")));
    recordings_widget = qobject_cast<stack_recordings *>(
        m_pages.find(QStringLiteral("recordings")));
    setting_widget = qobject_cast<stack_setting *>(
        m_pages.find(QStringLiteral("settings")));
    user_profile_widget = qobject_cast<stack_user_profile *>(
        m_pages.find(QStringLiteral("profile")));

    if (recordings_widget != nullptr) {
        connect(recordings_widget, &stack_recordings::playRequested, this,
                [this](const RecordingEntry &entry) {
                    auto *dialog = new RecordingPlaybackDialog(entry, this);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->show();
                    dialog->play();
                });
    }

    // 3) 侧栏切换：按"页"统一处理，新页面只要实现 ui::Page::on_show 就能懒刷新
    ui.sideNav->setCurrentRow(0);
    connect(ui.sideNav, &QListWidget::currentRowChanged, this,
            [this](int row) {
                if (ui.contentStack == nullptr || row < 0 ||
                    row >= m_nav_pages.size()) {
                    return;
                }
                QWidget *page = m_nav_pages.at(row);
                if (page == nullptr) {
                    return;
                }
                // 新页面：统一走 ui::Page 钩子
                if (auto *ui_page = qobject_cast<ui::Page *>(page)) {
                    ui_page->on_show();
                } else if (auto *history =
                               qobject_cast<stack_meeting_history *>(page)) {
                    history->refresh();  // 历史遗留页面（迁移后删除此分支）
                } else if (auto *recordings =
                               qobject_cast<stack_recordings *>(page)) {
                    recordings->refresh();  // 同上
                }
                ui.contentStack->setCurrentWidget(page);
            });
}

void main_window::init_ui() {
    setAttribute(Qt::WA_StyledBackground, true);
    ui.setupUi(this);

    ui.userCard->installEventFilter(this);

    register_pages();
    build_pages();

    connect(setting_widget, &stack_setting::uiPrefsChanged, this, [this]() {
        // 预留：主题等后续可在此刷新
        Q_UNUSED(this);
    });

    {
        Storage &s = Storage::instance();
        const int startPage = s.value(QStringLiteral("ui/startPage"), 0).toInt();
        if (startPage >= 0 && startPage < ui.sideNav->count()) {
            ui.sideNav->setCurrentRow(startPage);
        }
    }
}

void main_window::CreateMeeting_button_clicked(quint32 max_participants,
                                               quint32 duration_minutes) {
    spdlog::info(
        "[main_window] CreateMeeting max_participants={} duration_minutes={}",
        max_participants, duration_minutes);
    MeetingWidget *meeting = ensureMeetingWidget();
    if (meeting == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (meeting->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }

    meeting->show();
    meeting->request_connect_to_server_slot(
        meeting_server_host(), meeting_server_port(),
        ConnectAction::CreateMeeting, QString(), max_participants,
        duration_minutes);
}

void main_window::JoinMeeting_button_clicked(const QString &roomNo) {
    spdlog::info("[main_window] JoinMeeting roomNo={}",
                 roomNo.toUtf8().constData());
    MeetingWidget *meeting = ensureMeetingWidget();
    if (meeting == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (meeting->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }
    if (roomNo.isEmpty()) {
        QMessageBox::warning(this, "RoomNo Error", "请输入房间号");
        return;
    }

    meeting->show();
    meeting->request_connect_to_server_slot(meeting_server_host(),
                                           meeting_server_port(),
                                           ConnectAction::JoinMeeting, roomNo);
}

void main_window::onConnectServerFinished(bool ok, QString ip, QString port,
                                          ConnectAction action) {
    Q_UNUSED(ip);
    Q_UNUSED(port);
    Q_UNUSED(action);
    if (!ok) {
        if (widget && widget->isVisible())
            widget->hide();
        QMessageBox::warning(this, "Connection error", "连接服务器失败",
                             QMessageBox::Yes, QMessageBox::Yes);
    }
}

void main_window::onMeetingConfirmed(bool created, int roomNo) {
    MeetingHistory::append(created ? MeetingRecordKind::Created
                                   : MeetingRecordKind::Joined,
                           roomNo);
    // 首页的「最近 N 个会议」与记录页都刷新一份
    if (history_widget) {
        history_widget->refresh();
    }
    if (create_meeting_widget && create_meeting_widget->findChild<RecentMeetingList *>()) {
        create_meeting_widget->findChild<RecentMeetingList *>()->refresh();
    }
    if (join_meeting_widget != nullptr) {
        join_meeting_widget->refresh_recent();
    }
}

void main_window::onMeetingClosed(qint64 durationMs) {
    MeetingHistory::finish_open_session(durationMs);
    if (history_widget) {
        history_widget->refresh();
    }
    if (create_meeting_widget && create_meeting_widget->findChild<RecentMeetingList *>()) {
        create_meeting_widget->findChild<RecentMeetingList *>()->refresh();
    }
    if (join_meeting_widget != nullptr) {
        join_meeting_widget->refresh_recent();
    }
}

void main_window::onLogoutClicked() {
    // 先清凭据：下次启动不再自动登录
    LoginStore::clear();
    spdlog::info("[main_window] logout requested");
    emit logoutRequested();
}
