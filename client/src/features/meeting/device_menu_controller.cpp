#include "features/meeting/device_menu_controller.h"

#include "features/meeting/meeting_media_controller.h"
#include "ui/foundation/icon_provider.h"
#include "ui/foundation/theme_manager.h"

#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <QMenu>
#include <QPoint>
#include <QToolButton>
#include <QVariant>
#include <QWidget>

DeviceMenuController::DeviceMenuController(MeetingMediaController *media,
                                           QWidget *owner, QObject *parent)
    : QObject(parent), _media(media), _owner(owner) {}

void DeviceMenuController::bind_buttons(QToolButton *audio_button,
                                        QToolButton *video_button) {
    _audioButton = audio_button;
    _videoButton = video_button;
}

QString DeviceMenuController::device_name(const xrtc::XRTCDeviceInfo &info) {
    if (!info.device_name.empty()) {
        return QString::fromUtf8(info.device_name.data(),
                                 static_cast<int>(info.device_name.size()));
    }
    return QString::fromUtf8(info.device_id.data(),
                             static_cast<int>(info.device_id.size()));
}

void DeviceMenuController::popup_menu(QMenu *menu, QToolButton *button) {
    if (menu == nullptr || button == nullptr) {
        return;
    }
    auto &theme = ui::ThemeManager::instance();
    const QColor color = theme.color(QStringLiteral("textMuted"));
    auto &icons = ui::IconProvider::instance();

    button->setIcon(icons.icon(QStringLiteral("chevron-down"), color, 12));

    QObject::connect(menu, &QMenu::aboutToHide, button, [button, color]() {
        button->setIcon(ui::IconProvider::instance().icon(
            QStringLiteral("chevron-up"), color, 12));
    });
    menu->popup(menu->pos().isNull() ? QCursor::pos() : menu->pos());
}

void DeviceMenuController::show_audio_menu() {
    if (_media == nullptr || _audioButton == nullptr) {
        return;
    }

    auto *menu = new QMenu(_owner);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // ---- 扬声器 ----
    auto *speaker_title = menu->addAction(tr("选择扬声器"));
    speaker_title->setEnabled(false);
    auto *speaker_group = new QActionGroup(menu);
    speaker_group->setExclusive(true);
    const auto speakers = _media->playout_devices();
    if (speakers.empty()) {
        auto *empty = menu->addAction(tr("（无可用扬声器）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->playout_device_id();
        if (current.empty()) {
            current = speakers.front().device_id;
        }
        for (const auto &dev : speakers) {
            auto *action = menu->addAction(device_name(dev));
            action->setCheckable(true);
            action->setChecked(dev.device_id == current);
            action->setData(
                QVariant::fromValue(QString::fromStdString(dev.device_id)));
            speaker_group->addAction(action);
            connect(action, &QAction::triggered, this,
                    [this, id = dev.device_id]() {
                        _media->select_playout_device(id);
                    });
        }
    }

    menu->addSeparator();

    // ---- 麦克风 ----
    auto *mic_title = menu->addAction(tr("选择麦克风"));
    mic_title->setEnabled(false);
    auto *mic_group = new QActionGroup(menu);
    mic_group->setExclusive(true);
    const auto mics = _media->audio_devices();
    if (mics.empty()) {
        auto *empty = menu->addAction(tr("（无可用麦克风）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->audio_device_id();
        if (current.empty()) {
            current = mics.front().device_id;
        }
        for (const auto &dev : mics) {
            auto *action = menu->addAction(device_name(dev));
            action->setCheckable(true);
            action->setChecked(dev.device_id == current);
            action->setData(
                QVariant::fromValue(QString::fromStdString(dev.device_id)));
            mic_group->addAction(action);
            connect(action, &QAction::triggered, this,
                    [this, id = dev.device_id]() {
                        _media->select_audio_device(id);
                    });
        }
    }

    const QPoint pos =
        _audioButton->mapToGlobal(QPoint(0, -menu->sizeHint().height()));
    menu->move(pos);
    popup_menu(menu, _audioButton);
}

void DeviceMenuController::show_video_menu() {
    if (_media == nullptr || _videoButton == nullptr) {
        return;
    }

    auto *menu = new QMenu(_owner);
    menu->setObjectName(QStringLiteral("deviceSelectMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);

    auto *cam_title = menu->addAction(tr("选择摄像头"));
    cam_title->setEnabled(false);
    auto *cam_group = new QActionGroup(menu);
    cam_group->setExclusive(true);
    const auto cameras = _media->video_devices();
    if (cameras.empty()) {
        auto *empty = menu->addAction(tr("（无可用摄像头）"));
        empty->setEnabled(false);
    } else {
        std::string current = _media->video_device_id();
        if (current.empty()) {
            current = cameras.front().device_id;
        }
        for (const auto &dev : cameras) {
            auto *action = menu->addAction(device_name(dev));
            action->setCheckable(true);
            action->setChecked(dev.device_id == current);
            action->setData(
                QVariant::fromValue(QString::fromStdString(dev.device_id)));
            cam_group->addAction(action);
            connect(action, &QAction::triggered, this,
                    [this, id = dev.device_id]() {
                        _media->select_video_device(id);
                    });
        }
    }

    const QPoint pos =
        _videoButton->mapToGlobal(QPoint(0, -menu->sizeHint().height()));
    menu->move(pos);
    popup_menu(menu, _videoButton);
}
