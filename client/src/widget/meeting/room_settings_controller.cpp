#include "room_settings_controller.h"

#include "meeting_recorder.h"
#include "meeting_settings_popup.h"
#include "room_settings_flags.h"

#include <QMessageBox>
#include <QPushButton>
#include <QWidget>

#include <spdlog/spdlog.h>

RoomSettingsController::RoomSettingsController(MeetingRecorder *recorder,
                                               QWidget *owner, QObject *parent)
    : QObject(parent), _recorder(recorder), _owner(owner) {}

void RoomSettingsController::init_popup(QPushButton *anchor_button) {
    _anchor = anchor_button;
    if (_popup) {
        return;
    }
    _popup = new MeetingSettingsPopup(_owner);

    connect(_popup, &MeetingSettingsPopup::settingsChanged, this,
            [this](std::uint32_t flags) {
                _flags = flags;
                emit settings_edited(flags);
            });
    connect(_popup, &MeetingSettingsPopup::copyRoomNoRequested, this,
            &RoomSettingsController::copy_room_no_requested);
    connect(_popup, &MeetingSettingsPopup::resendProfileRequested, this,
            &RoomSettingsController::resend_profile_requested);
    connect(_popup, &MeetingSettingsPopup::recordToggled, this,
            [this](bool start) {
                if (start) {
                    start_recording();
                } else {
                    stop_recording();
                }
            });
}

void RoomSettingsController::set_host(bool is_host) { _isHost = is_host; }

bool RoomSettingsController::can_share_screen() const {
    if (_isHost) {
        return true;
    }
    return (_flags & ROOM_SETTINGS_ALLOW_MEMBER_SHARE) != 0;
}

bool RoomSettingsController::can_record() const {
    return (_flags & ROOM_SETTINGS_ALLOW_RECORD) != 0;
}

bool RoomSettingsController::is_recording() const {
    return _recorder != nullptr && _recorder->is_recording();
}

void RoomSettingsController::update_popup_state() {
    if (!_popup) {
        return;
    }
    _popup->set_room_no(_roomNo);
    _popup->set_host(_isHost);
    _popup->set_settings(_flags);
    _popup->set_can_record(can_record());
    _popup->set_recording(is_recording(),
                          _recorder ? _recorder->elapsed_ms() : 0);
    _popup->adjustSize();
}

void RoomSettingsController::refresh_popup_recording() {
    if (!_popup) {
        return;
    }
    _popup->set_recording(is_recording(),
                          _recorder ? _recorder->elapsed_ms() : 0);
}

void RoomSettingsController::show_popup() {
    if (!_popup || !_anchor) {
        return;
    }
    update_popup_state();

    // 锚定在按钮下方，点外部自动关闭（Qt::Popup，不可拖动）
    const QPoint pos = _anchor->mapToGlobal(QPoint(
        _anchor->width() - _popup->width(), _anchor->height() + 6));
    _popup->move(pos);
    _popup->show();
}

void RoomSettingsController::apply_flags(std::uint32_t flags) {
    _flags = flags;

    spdlog::info("[RoomSettings] flags=0x{:x} allow_record={} "
                 "allow_member_share={} host={}",
                 flags, (flags & ROOM_SETTINGS_ALLOW_RECORD) != 0,
                 (flags & ROOM_SETTINGS_ALLOW_MEMBER_SHARE) != 0, _isHost);

    // 房主关闭「允许录制」时，任何人都要立即停录
    if (!can_record() && is_recording()) {
        spdlog::info("[RoomSettings] recording revoked by host, stopping");
        stop_recording();
    }

    update_popup_state();
    emit flags_applied(flags);
}

bool RoomSettingsController::start_recording() {
    if (!_recorder || _recorder->is_recording()) {
        return false;
    }
    if (!_sessionActive) {
        QMessageBox::warning(_owner, tr("录制"), tr("请先进入会议再开始录制"));
        return false;
    }
    if (!can_record()) {
        QMessageBox::warning(_owner, tr("录制"), tr("房主已关闭本场录制"));
        return false;
    }
    if (!_recorder->start(_roomNo)) {
        return false;
    }
    update_popup_state();
    return true;
}

void RoomSettingsController::stop_recording() {
    if (_recorder && _recorder->is_recording()) {
        _recorder->stop();
    }
    update_popup_state();
}
