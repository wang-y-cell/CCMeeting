#include "features/meeting/meeting_top_bar_controller.h"

#include "config/preferences.h"
#include "config/user_session.h"
#include "features/meeting/meeting_state.h"
#include "features/meeting/meeting_window_host.h"
#include "features/meeting/participants_controller.h"
#include "features/meeting/room_settings_controller.h"
#include "ui/pages/meeting_window_ui.h"

#include <QDateTime>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

MeetingTopBarController::MeetingTopBarController(IMeetingWindowHost *host,
                                                 QObject *parent)
    : QObject(parent), _host(host) {
    _durationTimer = new QTimer(this);
    _durationTimer->setInterval(1000);
    connect(_durationTimer, &QTimer::timeout, this,
            &MeetingTopBarController::refresh_duration);
}

QString MeetingTopBarController::format_duration_hms(qint64 seconds) {
    if (seconds < 0) {
        seconds = 0;
    }
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds % 3600) / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void MeetingTopBarController::apply_tab_bar_visibility() {
    ui::MeetingWindowUi &ui = _host->ui();
    if (ui.tabWidget == nullptr || ui.tabWidget->tabBar() == nullptr) {
        return;
    }
    // 默认隐藏标签栏（可在 设置 → 界面 → 会议窗口显示右侧标签栏 打开）
    const bool show_tabs =
        preferences().value(QStringLiteral("meeting/showTabs"), false).toBool();
    ui.tabWidget->tabBar()->setVisible(show_tabs);
}

void MeetingTopBarController::refresh_meeting_info() {
    ui::MeetingWindowUi &ui = _host->ui();
    const meeting::MeetingState &state = _host->state();

    const bool in_meeting = state._createmeet || state._joinmeet;
    QString status_text;
    if (state._createmeet) {
        status_text = tr("已创建会议");
    } else if (state._joinmeet) {
        status_text = tr("已加入会议");
    } else {
        status_text = tr("未加入会议");
    }
    ui.labelMeetStatus->setText(status_text);
    ui.topMeetStatus->setText(status_text);

    // 入会状态胶囊换色（dynamic property，见 meeting_widget.qss）
    if (ui.topMeetStatus->property("joined").toBool() != in_meeting) {
        ui.topMeetStatus->setProperty("joined", in_meeting);
        ui.topMeetStatus->style()->unpolish(ui.topMeetStatus);
        ui.topMeetStatus->style()->polish(ui.topMeetStatus);
    }
    // 未入会时在画布居中显示占位提示
    if (ui.canvasHint) {
        ui.canvasHint->setVisible(!in_meeting);
    }

    const QString room_text =
        state._roomNo > 0 ? QString::number(state._roomNo) : QStringLiteral("-");
    ui.labelRoomNo->setText(room_text);
    ui.topRoomChip->setText(tr("房间 %1").arg(room_text));

    const int member_count =
        _host->participants() ? _host->participants()->count() : 0;
    ui.labelMemberCount->setText(QString::number(member_count));
    ui.topMemberChip->setText(tr("%1 人").arg(member_count));

    const QString display = UserSession::instance().name();
    ui.labelLocalIp->setText(display.isEmpty()
                                 ? QString::number(_host->local_user_id())
                                 : display);

    if (state._sessionActive && !state._serverAddr.isEmpty()) {
        ui.labelServer->setText(state._serverAddr);
    } else if (state._sessionActive) {
        ui.labelServer->setText(tr("已连接"));
    } else {
        ui.labelServer->setText(tr("未连接"));
    }

    refresh_speaker();
}

void MeetingTopBarController::refresh_duration() {
    ui::MeetingWindowUi &ui = _host->ui();
    const meeting::MeetingState &state = _host->state();
    if (!ui.topDurationChip) {
        return;
    }
    const bool in_meeting =
        (state._createmeet || state._joinmeet) && state._sessionStartMs > 0;
    QString text;
    if (in_meeting) {
        const qint64 seconds =
            (QDateTime::currentMSecsSinceEpoch() - state._sessionStartMs) / 1000;
        text = format_duration_hms(seconds);
    } else {
        text = tr("--:--:--");
    }
    if (ui.topDurationChip->text() == text) {
        return;
    }
    ui.topDurationChip->setText(text);
    if (ui.topDurationChip->property("running").toBool() != in_meeting) {
        ui.topDurationChip->setProperty("running", in_meeting);
        ui.topDurationChip->style()->unpolish(ui.topDurationChip);
        ui.topDurationChip->style()->polish(ui.topDurationChip);
    }
}

void MeetingTopBarController::refresh_speaker() {
    ui::MeetingWindowUi &ui = _host->ui();
    const meeting::MeetingState &state = _host->state();

    QString speaker = QStringLiteral("-");
    if (state._createmeet || state._joinmeet) {
        if (state._localAudioOn) {
            QString name = UserSession::instance().name();
            if (name.isEmpty() && _host->participants()) {
                name = _host->participants()->display_name(_host->local_user_id());
            }
            if (!name.isEmpty()) {
                speaker = name;
            }
        }
    }
    ui.labelSpeaker->setText(speaker);
    ui.topSpeakerLabel->setText(tr("正在讲话: %1").arg(speaker));
}

void MeetingTopBarController::set_main_screen_title(qint64 user_id) {
    ui::MeetingWindowUi &ui = _host->ui();
    ui.groupBox_2->setTitle(QString());
    ui.topMainTitle->setText(
        user_id > 0 && _host->participants()
            ? _host->participants()->display_name(user_id)
            : QString());
}

void MeetingTopBarController::start_duration() {
    if (_durationTimer) {
        _durationTimer->start();
    }
    refresh_duration();
}

void MeetingTopBarController::stop_duration() {
    if (_durationTimer) {
        _durationTimer->stop();
    }
    refresh_duration();
}

void MeetingTopBarController::show_info_popup() {
    ui::MeetingWindowUi &ui = _host->ui();
    const meeting::MeetingState &state = _host->state();

    if (_infoPopup == nullptr) {
        auto *popup = new QFrame(_host->window(),
                                 Qt::Popup | Qt::FramelessWindowHint);
        popup->setObjectName(QStringLiteral("meetingInfoPopup"));
        popup->setAttribute(Qt::WA_StyledBackground, true);
        popup->setFixedWidth(280);

        auto *layout = new QVBoxLayout(popup);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(10);

        auto *title = new QLabel(tr("会议详情"), popup);
        title->setObjectName(QStringLiteral("infoPopupTitle"));
        layout->addWidget(title);

        auto add_row = [popup, layout](const QString &key,
                                       const QString &object_name) {
            auto *row = new QWidget(popup);
            auto *row_layout = new QHBoxLayout(row);
            row_layout->setContentsMargins(0, 0, 0, 0);
            row_layout->setSpacing(12);
            auto *key_label = new QLabel(key, row);
            key_label->setObjectName(QStringLiteral("infoPopupKey"));
            key_label->setFixedWidth(64);
            auto *value_label = new QLabel(QStringLiteral("-"), row);
            value_label->setObjectName(object_name);
            value_label->setTextInteractionFlags(Qt::TextSelectableByMouse);
            row_layout->addWidget(key_label);
            row_layout->addWidget(value_label, 1);
            layout->addWidget(row);
        };
        add_row(tr("主题"), QStringLiteral("infoPopupSubject"));
        add_row(tr("会议号"), QStringLiteral("infoPopupRoomNo"));
        add_row(tr("发起人"), QStringLiteral("infoPopupHost"));
        add_row(tr("我的名称"), QStringLiteral("infoPopupSelfName"));
        add_row(tr("参会时长"), QStringLiteral("infoPopupDuration"));
        _infoPopup = popup;
    }

    // 刷新内容
    const QString self_name = UserSession::instance().name();
    const bool is_host =
        _host->room_settings() && _host->room_settings()->is_host();
    if (auto *label = _infoPopup->findChild<QLabel *>(
            QStringLiteral("infoPopupSubject"))) {
        label->setText(self_name.isEmpty() ? tr("会议")
                                           : tr("%1的会议").arg(self_name));
    }
    if (auto *label = _infoPopup->findChild<QLabel *>(
            QStringLiteral("infoPopupRoomNo"))) {
        label->setText(state._roomNo > 0 ? QString::number(state._roomNo)
                                         : QStringLiteral("-"));
    }
    if (auto *label =
            _infoPopup->findChild<QLabel *>(QStringLiteral("infoPopupHost"))) {
        label->setText(is_host ? self_name : tr("房主"));
    }
    if (auto *label = _infoPopup->findChild<QLabel *>(
            QStringLiteral("infoPopupSelfName"))) {
        label->setText(self_name.isEmpty() ? QStringLiteral("-") : self_name);
    }
    if (auto *label = _infoPopup->findChild<QLabel *>(
            QStringLiteral("infoPopupDuration"))) {
        const qint64 seconds =
            state._sessionStartMs > 0
                ? (QDateTime::currentMSecsSinceEpoch() - state._sessionStartMs) /
                      1000
                : 0;
        label->setText(format_duration_hms(seconds));
    }

    _infoPopup->adjustSize();
    const QPoint pos = ui.btnMeetingInfo->mapToGlobal(
        QPoint(0, ui.btnMeetingInfo->height() + 6));
    _infoPopup->move(pos);
    _infoPopup->show();
}
