#include "ui/shell/main_window.h"

#include "ui/foundation/theme_manager.h"
#include "ui/foundation/icon_provider.h"
#include "media/avatar_service.h"
#include "config/client_config.h"
#include "config/login_store.h"
#include "data/meeting_repository.h"
#include "config/preferences.h"
#include "ui/composites/recent_meeting_list.h"
#include "config/user_session.h"
#include "config/global_config.h"
#include "core/storage.h"
#include "ui/foundation/auth_http.h"

#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QListWidget>
#include <QAudioDevice>
#include <QCameraDevice>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaDevices>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
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
    spdlog::debug("[main_window] ctor begin");
    spdlog::default_logger()->flush();

    const auto &meeting = ClientConfig::instance().meeting_server();
    spdlog::info("[main_window] meeting server {}:{}",
                 meeting.host.toUtf8().constData(), meeting.port);
    spdlog::default_logger()->flush();

    init_ui();
    setWindowTitle(QString::fromStdString(config::app::main_window_title));
    set_style();
    refreshUserCard();

    // 延迟创建 ui::MeetingWindow：其 QSoundEffect 会加载 Qt Multimedia(FFmpeg)，
    // 过早加载后 WebRTC 摄像头枚举（设置页）在部分环境会直接崩溃。
    connect(create_meeting_widget, &ui::CreateMeetingPage::createMeetingClicked,
            this, &main_window::CreateMeeting_button_clicked);
    if (join_meeting_widget != nullptr) {
        connect(join_meeting_widget, &ui::JoinMeetingPage::join_requested, this,
                &main_window::JoinMeeting_button_clicked);
    }
    connect(user_profile_widget, &ui::UserProfilePage::backHomeRequested, this,
            &main_window::onProfileBackHome);
    connect(user_profile_widget, &ui::UserProfilePage::avatarUpdated, this,
            &main_window::onProfileAvatarUpdated);
    // 账号操作在个人资料页（退出登录 / 注销账号）
    connect(user_profile_widget, &ui::UserProfilePage::logoutRequested, this,
            &main_window::onLogoutClicked);
    connect(user_profile_widget, &ui::UserProfilePage::deleteAccountRequested,
            this, &main_window::onDeleteAccountClicked);

    // 预热 Qt Multimedia 设备枚举：首次 videoInputs() 冷启动 500ms+，
    // 放到主界面显示后的空闲时刻做完，之后打开设置页就不会再卡。
    QTimer::singleShot(config::ui::timing::main_window_device_warmup_ms, this, [this]() {
        QElapsedTimer timer;
        timer.start();
        const auto cameras = QMediaDevices::videoInputs();
        const auto mics = QMediaDevices::audioInputs();
        spdlog::info("[main_window] media devices warmed in {}ms (cameras={} "
                     "mics={})",
                     static_cast<long long>(timer.elapsed()), cameras.size(),
                     mics.size());
    });

    spdlog::debug("[main_window] ctor done");
    spdlog::default_logger()->flush();
}

void main_window::set_style() {
    // 统一走令牌替换：QSS 里可以写 @primary / @canvas 等占位符（见 ui/foundation/design_tokens.h）
    ui::ThemeManager::instance().apply_to(
        this, QString::fromLatin1(config::paths::style::main_window));
}

void main_window::destroyMeetingWindow() {
    // 兜底：会议窗口被销毁时主窗口必须回来
    restore_after_meeting();
    if (!widget) {
        return;
    }
    widget->hide();
    delete widget;
    widget = nullptr;
}

ui::MeetingWindow *main_window::ensureMeetingWindow() {
    if (widget) {
        return widget;
    }
    spdlog::debug("[main_window] lazy create ui::MeetingWindow");
    spdlog::default_logger()->flush();
    widget = new ui::MeetingWindow(nullptr);
    widget->hide();
    connect(widget, &ui::MeetingWindow::connect_server_finished_signal, this,
            &main_window::onConnectServerFinished);
    connect(widget, &ui::MeetingWindow::meeting_confirmed_signal, this,
            &main_window::onMeetingConfirmed);
    connect(widget, &ui::MeetingWindow::meeting_closed_signal, this,
            &main_window::onMeetingClosed);
    spdlog::debug("[main_window] ui::MeetingWindow ready");
    spdlog::default_logger()->flush();
    return widget;
}

main_window::~main_window() {
    destroyMeetingWindow();
}

void main_window::closeEvent(QCloseEvent *event) {
    destroyMeetingWindow();
    FramelessWindow<QWidget>::closeEvent(event);
}

void main_window::refreshUserCard() {
    if (!ui.nameLabel || !ui.avatarLabel) {
        return;
    }

    const auto &session = UserSession::instance();
    const QString displayName =
        session.isLoggedIn() ? session.name() : tr("未登录");
    const QString infoText =
        session.isLoggedIn() ? session.info().trimmed() : QString();

    // 卡片横排：可用宽度 = 卡片宽 - (头像 + 间距 + 两侧内边距)，见 config::ui::sidebar
    // 字号由 QSS 决定，这里只负责「按实际可用宽度省略」：
    // 文字宽度取标签真实宽度（布局完成后才有值），构造期兜底用侧栏常量。
    // 卡片尺寸变化时（见 eventFilter 的 Resize 分支）会重新算一遍。
    if (ui.nameLabel) {
        ui.nameLabel->setText(displayName);
    }
    if (ui.infoLabel) {
        // 没有简介时给一句占位，避免卡片高度随资料有无而跳动
        ui.infoLabel->setText(infoText.isEmpty() ? tr("暂无简介") : infoText);
        ui.infoLabel->setToolTip(infoText.isEmpty() ? tr("暂无简介") : infoText);
    }
    _userCardName = displayName;
    _userCardInfo = infoText;
    update_user_card_text();

    // 头像未就绪时显示姓名首字，避免侧栏出现一个空白圆圈
    const QString monogram =
        displayName.isEmpty() ? QStringLiteral("?") : displayName.left(1);
    ui.avatarLabel->setText(monogram);

    AvatarService::instance().load(
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
    if (watched == ui.userCard && event->type() == QEvent::Resize) {
        // 卡片拿到真实宽度后重新省略文字（构造期宽度还没算出来）
        update_user_card_text();
    }
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

void main_window::update_user_card_text() {
    if (!ui.nameLabel || !ui.infoLabel) {
        return;
    }

    // 可用宽度优先取标签真实宽度（布局完成后才有值）；
    // 构造期还没布局时用 config::ui::sidebar::user_card_fallback_text_width 兜底。
    int textWidth = ui.infoLabel->width();
    if (textWidth <= 0) {
        textWidth = config::ui::sidebar::user_card_fallback_text_width;
    }

    const QFontMetrics nameMetrics(ui.nameLabel->font());
    ui.nameLabel->setText(
        nameMetrics.elidedText(_userCardName, Qt::ElideRight, textWidth));

    const QFontMetrics infoMetrics(ui.infoLabel->font());
    const QString shown =
        _userCardInfo.isEmpty() ? tr("暂无简介") : _userCardInfo;
    ui.infoLabel->setText(
        infoMetrics.elidedText(shown, Qt::ElideRight, textWidth));
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
    m_pages.add({QStringLiteral("create"), tr("创建会议"),
                 QStringLiteral("video"), 0, true},
                [this](QWidget *) { return new ui::CreateMeetingPage(this); });
    m_pages.add({QStringLiteral("join"), tr("加入会议"),
                 QStringLiteral("users"), 10, true},
                [this](QWidget *) { return new ui::JoinMeetingPage(this); });
    m_pages.add({QStringLiteral("history"), tr("会议记录"),
                 QStringLiteral("calendar"), 20, true},
                [this](QWidget *) { return new ui::MeetingHistoryPage(this); });
    m_pages.add({QStringLiteral("recordings"), tr("录制"),
                 QStringLiteral("record"), 30, true},
                [this](QWidget *) { return new ui::RecordingsPage(this); });
    m_pages.add({QStringLiteral("settings"), tr("设置"),
                 QStringLiteral("settings"), 40, true},
                [this](QWidget *) { return new ui::SettingsPage(this); });

    // 不进侧栏、由用户卡片进入的页面
    m_pages.add({QStringLiteral("profile"), tr("个人资料"),
                 QStringLiteral("user"), 100, false},
                [this](QWidget *) { return new ui::UserProfilePage(this); });
}

void main_window::build_pages() {
    // 1) 按注册表创建全部页面，并按注册顺序加入内容栈
    for (const ui::PageMeta &meta : m_pages.all()) {
        ui::Page *page = m_pages.create(meta.id, this);
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
        qobject_cast<ui::CreateMeetingPage *>(m_pages.find(QStringLiteral("create")));
    join_meeting_widget = qobject_cast<ui::JoinMeetingPage *>(
        m_pages.find(QStringLiteral("join")));
    history_widget = qobject_cast<ui::MeetingHistoryPage *>(
        m_pages.find(QStringLiteral("history")));
    recordings_widget = qobject_cast<ui::RecordingsPage *>(
        m_pages.find(QStringLiteral("recordings")));
    user_profile_widget = qobject_cast<ui::UserProfilePage *>(
        m_pages.find(QStringLiteral("profile")));

    if (recordings_widget != nullptr) {
        connect(recordings_widget, &ui::RecordingsPage::playRequested, this,
                [this](const RecordingEntry &entry) {
                    auto *dialog = new ui::RecordingPlaybackDialog(entry, this);
                    dialog->setAttribute(Qt::WA_DeleteOnClose);
                    dialog->show();
                    dialog->play();
                });
    }

    // 3) 侧栏由注册表生成：新增页面只需 register_pages() 里加一行
    ui.sideNav->setIconSize(QSize(16, 16));
    ui.sideNav->clear();
    const QList<ui::PageMeta> nav_metas = m_pages.nav();
    for (const ui::PageMeta &meta : nav_metas) {
        auto *item = new QListWidgetItem(meta.title, ui.sideNav);
        // 图标跟随主题色（单色 SVG + tint）
        if (!meta.icon.isEmpty()) {
            item->setIcon(ui::IconProvider::instance().icon(
                meta.icon, ui::ThemeManager::instance().color(QStringLiteral("textMuted")), 16));
        }
        item->setSizeHint(QSize(0, 48));
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        item->setData(Qt::UserRole, meta.id);
        QFont font = item->font();
        font.setPointSize(11);
        font.setWeight(QFont::DemiBold);
        item->setFont(font);
        ui.sideNav->addItem(item);
    }

    // 3b) 主题切换后刷新侧栏图标颜色
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this, nav_metas]() {
                const QColor color =
                    ui::ThemeManager::instance().color(QStringLiteral("textMuted"));
                for (int i = 0; i < ui.sideNav->count() && i < nav_metas.size(); ++i) {
                    const QString icon_name = nav_metas.at(i).icon;
                    if (icon_name.isEmpty()) {
                        continue;
                    }
                    if (auto *item = ui.sideNav->item(i)) {
                        item->setIcon(ui::IconProvider::instance().icon(icon_name, color, 16));
                    }
                }
            });

    // 4) 侧栏切换：只负责切页。页面刷新统一由 ui::Page::showEvent → on_show() 驱动。
    //    这里**不能**再调一次 on_show()：切页时 setCurrentWidget 已经让新页收到
    //    showEvent（→ on_show），再手动调一次会让设置页枚举两遍设备、记录/录制页
    //    各查两遍库。
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
                ui.contentStack->setCurrentWidget(page);
            });
}

void main_window::init_ui() {
    setAttribute(Qt::WA_StyledBackground, true);
    ui.setupUi(this);

    ui.userCard->installEventFilter(this);

    register_pages();
    build_pages();


    {
        int startPage =
            preferences().value(QString::fromLatin1(config::prefs::start_page), 0).toInt();
        if (startPage < 0 || startPage >= ui.sideNav->count()) {
            startPage = 0;
        }
        // 越界/脏偏好兜底到第 0 页；切页本身会通过 showEvent 触发该页 on_show()
        ui.sideNav->setCurrentRow(startPage);
    }
}

void main_window::CreateMeeting_button_clicked(quint32 max_participants,
                                               quint32 duration_minutes) {
    spdlog::info(
        "[main_window] CreateMeeting max_participants={} duration_minutes={}",
        max_participants, duration_minutes);
    ui::MeetingWindow *meeting = ensureMeetingWindow();
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
    ui::MeetingWindow *meeting = ensureMeetingWindow();
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

void main_window::hide_for_meeting() {
    if (!isVisible()) {
        return;
    }
    hidden_for_meeting_ = true;
    hide();
    spdlog::info("[main_window] 会议已开始 -> 隐藏主窗口");
}

void main_window::restore_after_meeting() {
    if (!hidden_for_meeting_) {
        return;
    }
    hidden_for_meeting_ = false;
    // 可能被会议窗口抢走焦点/最小化，恢复时一并唤醒
    if (isMinimized()) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    show();
    raise();
    activateWindow();
    spdlog::info("[main_window] 会议已结束 -> 恢复主窗口");
}

void main_window::onMeetingConfirmed(bool created, int roomNo) {
    meeting_repository().append(created ? MeetingRecordKind::Created
                                   : MeetingRecordKind::Joined,
                           roomNo);
    refresh_meeting_summaries();

    // 需求：会议开始后主窗口让位，结束/终止时再出现
    hide_for_meeting();
}

void main_window::onMeetingClosed(qint64 durationMs) {
    meeting_repository().finish_open_session(durationMs);
    refresh_meeting_summaries();

    restore_after_meeting();
}

void main_window::refresh_meeting_summaries() {
    // 首页的「最近 N 个会议」与记录页都刷新一份
    if (history_widget) {
        history_widget->refresh();
    }
    if (create_meeting_widget) {
        if (auto *recent = create_meeting_widget->findChild<RecentMeetingList *>()) {
            recent->refresh();
        }
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

void main_window::onDeleteAccountClicked() {
    const auto &session = UserSession::instance();
    if (!session.isLoggedIn() || session.token().isEmpty()) {
        QMessageBox::warning(this, tr("注销账号"), tr("请先登录"));
        return;
    }
    if (_deletingAccount) {
        return;
    }

    const auto answer = QMessageBox::warning(
        this, tr("注销账号"),
        tr("将永久注销账号「%1」，并删除本机的会议记录与录制索引，无法恢复。\n\n"
           "确定继续？")
            .arg(session.username()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    _deletingAccount = true;
    if (user_profile_widget) {
        user_profile_widget->set_account_actions_enabled(false);
    }

    auto *nam = new QNetworkAccessManager(this);
    const qint64 deleted_user = session.userId();
    QNetworkReply *reply = nam->post(
        ui::auth_request(ui::auth_url(QStringLiteral("/api/delete-account"))),
        QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, nam, deleted_user]() {
                const QByteArray body = reply->readAll();
                const QString network_error = reply->errorString();
                const bool failed = reply->error() != QNetworkReply::NoError;
                reply->deleteLater();
                nam->deleteLater();

                _deletingAccount = false;
                if (user_profile_widget) {
                    user_profile_widget->set_account_actions_enabled(true);
                }

                const QJsonObject root =
                    QJsonDocument::fromJson(body).object();
                const int code = root.value(QStringLiteral("code")).toInt(-1);
                if (failed || code != 0) {
                    QString reason =
                        root.value(QStringLiteral("message")).toString();
                    if (code == 404 &&
                        reason == QStringLiteral("not found")) {
                        // 认证服务还是旧版本：没有 /api/delete-account 这条路由
                        reason = tr("服务端未启用注销接口，请更新并重启认证服务");
                    } else if (code == 401 || code == 403) {
                        reason = tr("登录状态已失效，请重新登录后再试");
                    } else if (reason.isEmpty()) {
                        reason = network_error;
                    }
                    spdlog::warn("[main_window] delete account failed: code={} {}",
                                 code, reason.toUtf8().constData());
                    QMessageBox::warning(
                        this, tr("注销账号"),
                        tr("注销失败：%1").arg(reason));
                    return;
                }

                // 服务端已注销：清本地凭据 + 账号库（会议记录/录制索引），回登录界面
                spdlog::info("[main_window] account deleted, purging local data");
                LoginStore::clear();
                Storage::instance().purge_user_data(deleted_user);
                UserSession::instance().clear();
                emit logoutRequested();
            });
}
