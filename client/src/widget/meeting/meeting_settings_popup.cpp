#include "meeting_settings_popup.h"

#include "netheader.h"
#include "style_loader.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QLabel *make_label(const QString &objectName, const QString &text,
                   QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

QCheckBox *make_switch(const QString &objectName, const QString &text,
                       QWidget *parent) {
    auto *box = new QCheckBox(text, parent);
    box->setObjectName(objectName);
    box->setCursor(Qt::PointingHandCursor);
    return box;
}

}  // namespace

MeetingSettingsPopup::MeetingSettingsPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setObjectName(QStringLiteral("meetingSettingsPopup"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(304);
    loadWidgetStyleSheet(this, QStringLiteral(":/Style/source/meeting_settings_popup.qss"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // ---- 标题 ----
    root->addWidget(make_label(QStringLiteral("msTitle"),
                               tr("会议设置"), this));

    // ---- 房间号 ----
    auto *roomRow = new QHBoxLayout();
    roomRow->setSpacing(8);
    roomRow->addWidget(
        make_label(QStringLiteral("msSection"), tr("房间号"), this));
    _roomValue = make_label(QStringLiteral("msValue"), QStringLiteral("-"), this);
    roomRow->addWidget(_roomValue);
    roomRow->addStretch(1);
    _roleBadge = make_label(QStringLiteral("msRoleBadge"), tr("成员"), this);
    roomRow->addWidget(_roleBadge);
    root->addLayout(roomRow);

    root->addWidget(make_separator());

    // ---- 房间权限（仅房主可改）----
    root->addWidget(make_label(QStringLiteral("msSection"),
                               tr("房间权限"), this));

    _allowRecord = make_switch(QStringLiteral("msSwitch"),
                               tr("允许录制本场会议"), this);
    root->addWidget(_allowRecord);

    _allowShare = make_switch(QStringLiteral("msSwitch"),
                              tr("允许成员共享屏幕"), this);
    root->addWidget(_allowShare);

    _hostHint = make_label(QStringLiteral("msHint"),
                           tr("仅房主可修改以上选项"), this);
    _hostHint->setWordWrap(true);
    root->addWidget(_hostHint);

    root->addWidget(make_separator());

    // ---- 录制 ----
    root->addWidget(make_label(QStringLiteral("msSection"), tr("录制"), this));

    auto *recordRow = new QHBoxLayout();
    recordRow->setSpacing(8);
    _recordBtn = new QPushButton(tr("开始录制"), this);
    _recordBtn->setObjectName(QStringLiteral("msRecordBtn"));
    _recordBtn->setCursor(Qt::PointingHandCursor);
    _recordStatus = make_label(QStringLiteral("msRecordStatus"), tr("未录制"), this);
    recordRow->addWidget(_recordBtn);
    recordRow->addStretch(1);
    recordRow->addWidget(_recordStatus);
    root->addLayout(recordRow);

    _recordHint = make_label(QStringLiteral("msHint"), tr("房主已关闭本场录制"), this);
    _recordHint->setVisible(false);
    root->addWidget(_recordHint);

    root->addWidget(make_separator());

    // ---- 快捷操作 ----
    root->addWidget(make_label(QStringLiteral("msSection"),
                               tr("快捷操作"), this));

    _copyBtn = new QPushButton(tr("复制会议号"), this);
    _copyBtn->setObjectName(QStringLiteral("msAction"));
    _copyBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(_copyBtn);

    _resendBtn = new QPushButton(tr("重新广播我的昵称/头像"), this);
    _resendBtn->setObjectName(QStringLiteral("msAction"));
    _resendBtn->setCursor(Qt::PointingHandCursor);
    root->addWidget(_resendBtn);

    connect(_allowRecord, &QCheckBox::toggled, this,
            [this](bool) { emit_settings(); });
    connect(_allowShare, &QCheckBox::toggled, this,
            [this](bool) { emit_settings(); });
    connect(_copyBtn, &QPushButton::clicked, this, [this]() {
        emit copyRoomNoRequested();
        hide();
    });
    connect(_resendBtn, &QPushButton::clicked, this, [this]() {
        emit resendProfileRequested();
        hide();
    });
    connect(_recordBtn, &QPushButton::clicked, this, [this]() {
        if (!_canRecord) {
            return;
        }
        emit recordToggled(!_recording);
    });
}

QFrame *MeetingSettingsPopup::make_separator() {
    auto *line = new QFrame(this);
    line->setObjectName(QStringLiteral("msSep"));
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return line;
}

void MeetingSettingsPopup::set_room_no(int room_no) {
    _roomValue->setText(room_no > 0 ? QString::number(room_no)
                                    : QStringLiteral("-"));
}

void MeetingSettingsPopup::set_host(bool is_host) {
    _isHost = is_host;
    _roleBadge->setText(is_host ? tr("房主") : tr("成员"));
    _roleBadge->setProperty("isHost", is_host);
    _roleBadge->style()->unpolish(_roleBadge);
    _roleBadge->style()->polish(_roleBadge);

    _allowRecord->setEnabled(is_host);
    _allowShare->setEnabled(is_host);
    _hostHint->setVisible(!is_host);
}

void MeetingSettingsPopup::set_settings(std::uint32_t flags) {
    _flags = flags;
    const QSignalBlocker blockRecord(_allowRecord);
    const QSignalBlocker blockShare(_allowShare);
    _allowRecord->setChecked((flags & ROOM_SETTINGS_ALLOW_RECORD) != 0);
    _allowShare->setChecked((flags & ROOM_SETTINGS_ALLOW_MEMBER_SHARE) != 0);
}

void MeetingSettingsPopup::set_can_record(bool can_record) {
    _canRecord = can_record;
    _recordBtn->setEnabled(can_record);
    _recordHint->setVisible(!can_record);
}

void MeetingSettingsPopup::set_recording(bool recording, qint64 elapsed_ms) {
    _recording = recording;
    _recordBtn->setText(recording ? tr("停止录制") : tr("开始录制"));
    _recordBtn->setProperty("recording", recording);
    _recordBtn->style()->unpolish(_recordBtn);
    _recordBtn->style()->polish(_recordBtn);

    if (!recording) {
        _recordStatus->setText(tr("未录制"));
        return;
    }
    const qint64 total = elapsed_ms / 1000;
    const qint64 hours = total / 3600;
    const qint64 minutes = (total % 3600) / 60;
    const qint64 seconds = total % 60;
    _recordStatus->setText(
        tr("● 录制中 %1")
            .arg(QStringLiteral("%1:%2:%3")
                     .arg(hours, 2, 10, QLatin1Char('0'))
                     .arg(minutes, 2, 10, QLatin1Char('0'))
                     .arg(seconds, 2, 10, QLatin1Char('0'))));
}

void MeetingSettingsPopup::emit_settings() {
    if (!_isHost) {
        // 非房主不会走到这里（控件已禁用），兜底再挡一次
        set_settings(_flags);
        return;
    }
    std::uint32_t flags = 0;
    if (_allowRecord->isChecked())
        flags |= ROOM_SETTINGS_ALLOW_RECORD;
    if (_allowShare->isChecked())
        flags |= ROOM_SETTINGS_ALLOW_MEMBER_SHARE;
    if (flags == _flags)
        return;
    _flags = flags;
    emit settingsChanged(flags);
}
