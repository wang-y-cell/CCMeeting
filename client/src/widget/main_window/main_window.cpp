#include "main_window.h"
#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/user_session.h"
#include "stack_join_meet.h"

#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QStringList>
#include <qnamespace.h>
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
    QFile file(":/Style/source/main_window.qss");
    if (file.open(QFile::ReadOnly)) {
        spdlog::info("main_window.qss loaded");
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        spdlog::warn("main_window.qss not found");
    }
}

main_window::~main_window() {
    delete widget;
    widget = nullptr;
}

void main_window::refreshUserCard() {
    if (!name_label_ || !avatar_label_) {
        return;
    }

    const auto &session = UserSession::instance();
    name_label_->setText(session.isLoggedIn() ? session.name() : tr("未登录"));

    AvatarImageLoader::instance().load(
        session.avatar(), avatar_label_->size(), [this](const QPixmap &pixmap) {
            if (avatar_label_) {
                avatar_label_->setPixmap(pixmap);
            }
        });
}

bool main_window::eventFilter(QObject *watched, QEvent *event) {
    if (watched == user_card_ && event->type() == QEvent::MouseButtonRelease) {
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
    if (content_stack_) {
        content_stack_->setCurrentWidget(user_profile_widget);
    }
}

void main_window::onProfileBackHome() {
    if (content_stack_ && create_meeting_widget) {
        content_stack_->setCurrentWidget(create_meeting_widget);
    }
}

void main_window::onProfileAvatarUpdated() {
    refreshUserCard();
}

void main_window::init_ui() {
    setObjectName(QStringLiteral("main_window"));
    resize(960, 640);
    setMinimumSize(760, 480);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(18, 42, 18, 18);
    mainLayout->setSpacing(16);

    auto *leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(12);

    user_card_ = new QWidget(this);
    user_card_->setObjectName(QStringLiteral("userProfile"));
    user_card_->setCursor(Qt::PointingHandCursor);
    user_card_->installEventFilter(this);
    auto *userLayout = new QVBoxLayout(user_card_);
    userLayout->setContentsMargins(12, 12, 12, 12);
    userLayout->setSpacing(8);

    avatar_label_ = new QLabel(user_card_);
    avatar_label_->setObjectName(QStringLiteral("userAvatar"));
    avatar_label_->setFixedSize(72, 72);
    avatar_label_->setAlignment(Qt::AlignCenter);
    avatar_label_->setScaledContents(true);

    name_label_ = new QLabel(user_card_);
    name_label_->setObjectName(QStringLiteral("userName"));
    name_label_->setAlignment(Qt::AlignCenter);
    name_label_->setWordWrap(true);

    userLayout->addWidget(avatar_label_, 0, Qt::AlignHCenter);
    userLayout->addWidget(name_label_);
    leftColumn->addWidget(user_card_);

    auto *left_bar = new QListWidget(this);
    left_bar->setObjectName(QStringLiteral("sideNav"));
    left_bar->setSpacing(4);
    left_bar->setFrameShape(QListWidget::NoFrame);
    left_bar->setFocusPolicy(Qt::NoFocus);
    left_bar->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    left_bar->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    const QStringList left_bar_items = {
        QStringLiteral("创建会议"),
        QStringLiteral("加入会议"),
    };

    for (const auto &item : left_bar_items) {
        auto *listWidgetItem = new QListWidgetItem(item);
        listWidgetItem->setSizeHint(QSize(0, 48));
        listWidgetItem->setTextAlignment(Qt::AlignCenter);
        QFont font = listWidgetItem->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        listWidgetItem->setFont(font);
        left_bar->addItem(listWidgetItem);
    }

    content_stack_ = new QStackedWidget(this);
    content_stack_->setObjectName(QStringLiteral("contentStack"));

    leftColumn->addWidget(left_bar, 1);

    auto *leftWrap = new QWidget(this);
    leftWrap->setFixedWidth(196);
    auto *leftWrapLayout = new QVBoxLayout(leftWrap);
    leftWrapLayout->setContentsMargins(0, 0, 0, 0);
    leftWrapLayout->setSpacing(0);
    leftWrapLayout->addLayout(leftColumn);

    mainLayout->addWidget(leftWrap);
    mainLayout->addWidget(content_stack_, 1);

    create_meeting_widget = new stack_create_meet(this);
    content_stack_->addWidget(create_meeting_widget);

    join_meeting_widget = new stack_join_meet(this);
    content_stack_->addWidget(join_meeting_widget);

    user_profile_widget = new stack_user_profile(this);
    content_stack_->addWidget(user_profile_widget);

    left_bar->setCurrentRow(0);
    connect(left_bar, &QListWidget::currentRowChanged, this,
            [this](int row) {
                if (!content_stack_) {
                    return;
                }
                if (row == 0 && create_meeting_widget) {
                    content_stack_->setCurrentWidget(create_meeting_widget);
                } else if (row == 1 && join_meeting_widget) {
                    content_stack_->setCurrentWidget(join_meeting_widget);
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
