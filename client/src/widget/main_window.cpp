#include "main_window.h"
#include "configure/client_config.h"
#include "configure/user_session.h"
#include "connection.h"
#include "stack_conn_server.h"
#include "stack_join_meet.h"
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QUrl>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QStringList>
#include <qnamespace.h>
#include <spdlog/spdlog.h>

main_window::main_window(QWidget *parent) : FramelessWindow<QWidget>(parent) {
    spdlog::info("[main_window] ctor begin");
    spdlog::default_logger()->flush();

    const auto &meeting = ClientConfig::instance().meeting_server();
    ip = meeting.host;
    port = QString::number(meeting.port);
    spdlog::info("[main_window] default meeting server {}:{}",
                 ip.toUtf8().constData(), port.toUtf8().constData());
    spdlog::default_logger()->flush();

    spdlog::info("[main_window] init_ui begin");
    spdlog::default_logger()->flush();
    init_ui();
    spdlog::info("[main_window] init_ui done");
    spdlog::default_logger()->flush();

    setWindowTitle(tr("CloudMeeting"));
    set_style();

    spdlog::info("[main_window] creating MeetingWidget ...");
    spdlog::default_logger()->flush();
    widget = new MeetingWidget(nullptr);
    spdlog::info("[main_window] MeetingWidget created");
    spdlog::default_logger()->flush();

    // widget->setWindowFlags(Qt::Window);
    widget->hide();

    connect(create_meeting_widget, &stack_create_meet::createMeetingClicked,
            this, &main_window::CreateMeeting_button_clicked);

    connect(join_meeting_widget, &stack_join_meet::joinMeetingClicked, this,
            &main_window::JoinMeeting_button_clicked);

    connect(connect_to_server_widget, &stack_conn_server::ConnServerClicked,
            this, &main_window::ConnectToServer_button_clicked);

    connect(widget, &MeetingWidget::connect_server_finished_signal, this,
            &main_window::onConnectServerFinished);
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
    if (widget != nullptr) {
        delete widget;
        widget = nullptr;
    }
    // delete ui;
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

    auto *userCard = new QWidget(this);
    userCard->setObjectName(QStringLiteral("userProfile"));
    auto *userLayout = new QVBoxLayout(userCard);
    userLayout->setContentsMargins(12, 12, 12, 12);
    userLayout->setSpacing(8);

    auto *avatarLabel = new QLabel(userCard);
    avatarLabel->setObjectName(QStringLiteral("userAvatar"));
    avatarLabel->setFixedSize(72, 72);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setScaledContents(true);

    auto *nameLabel = new QLabel(userCard);
    nameLabel->setObjectName(QStringLiteral("userName"));
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);

    const auto &session = UserSession::instance();
    nameLabel->setText(session.isLoggedIn() ? session.name() : tr("未登录"));
    avatarLabel->setPixmap(
        QPixmap(QString::fromUtf8(":/myImage/source/EmptyHead.png"))
            .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    if (session.isLoggedIn() && !session.avatar().isEmpty()) {
        auto *nam = new QNetworkAccessManager(userCard);
        QNetworkRequest req{QUrl(session.avatar())};
        auto *reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, userCard,
                [avatarLabel, reply]() {
                    if (reply->error() == QNetworkReply::NoError) {
                        QPixmap pix;
                        if (pix.loadFromData(reply->readAll())) {
                            avatarLabel->setPixmap(
                                pix.scaled(72, 72, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation));
                        }
                    }
                    reply->deleteLater();
                });
    }

    userLayout->addWidget(avatarLabel, 0, Qt::AlignHCenter);
    userLayout->addWidget(nameLabel);
    leftColumn->addWidget(userCard);

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
        QStringLiteral("连接服务器"),
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

    auto *stackedWidget = new QStackedWidget(this);
    stackedWidget->setObjectName(QStringLiteral("contentStack"));

    leftColumn->addWidget(left_bar, 1);

    auto *leftWrap = new QWidget(this);
    leftWrap->setFixedWidth(196);
    auto *leftWrapLayout = new QVBoxLayout(leftWrap);
    leftWrapLayout->setContentsMargins(0, 0, 0, 0);
    leftWrapLayout->setSpacing(0);
    leftWrapLayout->addLayout(leftColumn);

    mainLayout->addWidget(leftWrap);
    mainLayout->addWidget(stackedWidget, 1);

    create_meeting_widget = new stack_create_meet(this);
    stackedWidget->addWidget(create_meeting_widget);

    join_meeting_widget = new stack_join_meet(this);
    stackedWidget->addWidget(join_meeting_widget);

    connect_to_server_widget = new stack_conn_server(this);
    stackedWidget->addWidget(connect_to_server_widget);

    left_bar->setCurrentRow(0);
    connect(left_bar, &QListWidget::currentRowChanged, stackedWidget,
            &QStackedWidget::setCurrentIndex);
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
    widget->request_connect_to_server_slot(this->ip, this->port,
                                           ConnectAction::CreateMeeting,
                                           QString(), max_participants,
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
    widget->request_connect_to_server_slot(this->ip, this->port,
                                           ConnectAction::JoinMeeting, roomNo);
}

void main_window::ConnectToServer_button_clicked(QString ip, QString port) {
    spdlog::info("[main_window] ConnectToServer ip={} port={}",
                 ip.toUtf8().constData(), port.toUtf8().constData());
    if (widget == nullptr) {
        QMessageBox::warning(this, "warning", "会议窗口未初始化");
        return;
    }
    if (widget->isVisible()) {
        QMessageBox::warning(this, "warning", "目前有一打开的会议");
        return;
    }

    ip = ip.trimmed();
    port = port.trimmed();
    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "请输入 IP 和端口");
        return;
    }
    if (!Connection::validateIpPort(this, ip, port)) {
        return;
    }

    widget->request_connect_to_server_slot(ip, port, ConnectAction::None);
}

void main_window::onConnectServerFinished(bool ok, QString ip, QString port,
                                          ConnectAction action) {
    if (action == ConnectAction::None) {
        if (ok) {
            this->ip = ip;
            this->port = port;
            if (widget)
                widget->on_disconnect_server_slot();
            spdlog::info(
                "[main_window] probe connect ok then disconnect: {}:{}",
                ip.toUtf8().constData(), port.toUtf8().constData());
            QMessageBox::information(this, "Connection",
                                     QString("成功连接到 %1:%2").arg(ip, port));
        } else {
            QMessageBox::warning(this, "Connection error", "连接服务器失败",
                                 QMessageBox::Yes, QMessageBox::Yes);
        }
        return;
    }

    if (!ok) {
        QMessageBox::warning(this, "Connection error", "连接服务器失败",
                             QMessageBox::Yes, QMessageBox::Yes);
    }
}
