#include "main_window.h"
#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/user_session.h"
#include "style_loader.h"

#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
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

    widget = new MeetingWidget(nullptr);
    widget->hide();

    connect(create_meeting_widget, &stack_create_meet::createMeetingClicked,
            this, &main_window::CreateMeeting_button_clicked);
    connect(join_meeting_widget, &stack_join_meet::joinMeetingClicked, this,
            &main_window::JoinMeeting_button_clicked);
    connect(widget, &MeetingWidget::connect_server_finished_signal, this,
            &main_window::onConnectServerFinished);
    connect(user_profile_widget, &stack_user_profile::backHomeRequested, this,
            &main_window::onProfileBackHome);
    connect(user_profile_widget, &stack_user_profile::avatarUpdated, this,
            &main_window::onProfileAvatarUpdated);

    spdlog::info("[main_window] ctor done");
    spdlog::default_logger()->flush();
}

void main_window::set_style() {
    loadWidgetStyleSheet(this, QStringLiteral(":/Style/source/main_window.qss"));
}

void main_window::destroyMeetingWidget() {
    if (!widget) {
        return;
    }
    widget->hide();
    delete widget;
    widget = nullptr;
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
    ui.nameLabel->setText(session.isLoggedIn() ? session.name() : tr("未登录"));

    AvatarImageLoader::instance().load(
        session.avatar(), ui.avatarLabel->size(), this,
        [this](const QPixmap &pixmap) {
            if (ui.avatarLabel) {
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

void main_window::init_ui() {
    setAttribute(Qt::WA_StyledBackground, true);
    ui.setupUi(this);

    ui.userCard->installEventFilter(this);

    create_meeting_widget = new stack_create_meet(this);
    ui.contentStack->addWidget(create_meeting_widget);

    join_meeting_widget = new stack_join_meet(this);
    ui.contentStack->addWidget(join_meeting_widget);

    user_profile_widget = new stack_user_profile(this);
    ui.contentStack->addWidget(user_profile_widget);

    ui.sideNav->setCurrentRow(0);
    connect(ui.sideNav, &QListWidget::currentRowChanged, this,
            [this](int row) {
                if (!ui.contentStack) {
                    return;
                }
                if (row == 0 && create_meeting_widget) {
                    ui.contentStack->setCurrentWidget(create_meeting_widget);
                } else if (row == 1 && join_meeting_widget) {
                    ui.contentStack->setCurrentWidget(join_meeting_widget);
                }
            });
}

void main_window::CreateMeeting_button_clicked(quint32 max_participants,
                                               quint32 duration_minutes) {
    spdlog::info(
        "[main_window] CreateMeeting max_participants={} duration_minutes={}",
        max_participants, duration_minutes);
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }

    widget->show();
    widget->request_connect_to_server_slot(
        meeting_server_host(), meeting_server_port(),
        ConnectAction::CreateMeeting, QString(), max_participants,
        duration_minutes);
}

void main_window::JoinMeeting_button_clicked(const QString &roomNo) {
    spdlog::info("[main_window] JoinMeeting roomNo={}",
                 roomNo.toUtf8().constData());
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }
    if (roomNo.isEmpty()) {
        QMessageBox::warning(this, "RoomNo Error", "请输入房间号");
        return;
    }

    widget->show();
    widget->request_connect_to_server_slot(meeting_server_host(),
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
