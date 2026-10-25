#include "participants_controller.h"

#include "cameravideo.h"
#include "partner.h"
#include "partner_tile.h"
#include "videoglwidget.h"

#include <QVBoxLayout>

ParticipantsController::ParticipantsController(QVBoxLayout *tiles_layout,
                                               CameraVideo *camera,
                                               QObject *parent)
    : QObject(parent), _tilesLayout(tiles_layout), _camera(camera) {}

Partner *ParticipantsController::add(qint64 user_id) {
    const auto found = _partners.find(user_id);
    if (found != _partners.end()) {
        return found->second;
    }

    auto *partner = new Partner(user_id, nullptr);
    auto *tile = new PartnerTile(partner,
                                 _tilesLayout ? _tilesLayout->parentWidget()
                                              : nullptr);
    partner->setParent(tile);
    connect(partner, &Partner::clicked, this,
            &ParticipantsController::member_clicked);
    _partners.emplace(user_id, partner);
    if (_tilesLayout) {
        _tilesLayout->addWidget(tile);
    }
    if (VideoGLWidget *widget = partner->displayWidget()) {
        if (_camera) {
            _camera->addPartnerDisplay(user_id, widget);
        }
    }
    emit count_changed(count());
    return partner;
}

void ParticipantsController::remove(qint64 user_id) {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return;
    }
    Partner *partner = it->second;
    disconnect(partner, &Partner::clicked, this,
               &ParticipantsController::member_clicked);
    if (_camera) {
        _camera->removePartnerDisplay(user_id);
    }
    if (PartnerTile *tile = partner->tile()) {
        if (_tilesLayout) {
            _tilesLayout->removeWidget(tile);
        }
        partner->setTile(nullptr);
        tile->deleteLater();
    }
    partner->deleteLater();
    _partners.erase(it);
    emit count_changed(count());
}

void ParticipantsController::clear() {
    if (_partners.empty()) {
        return;
    }
    if (_camera) {
        _camera->clearAllPartnerDisplays();
    }
    for (auto it = _partners.begin(); it != _partners.end();) {
        Partner *partner = it->second;
        disconnect(partner, &Partner::clicked, this,
                   &ParticipantsController::member_clicked);
        if (PartnerTile *tile = partner->tile()) {
            if (_tilesLayout) {
                _tilesLayout->removeWidget(tile);
            }
            partner->setTile(nullptr);
            tile->deleteLater();
        }
        partner->deleteLater();
        it = _partners.erase(it);
    }
    _mainUserId = 0;
    emit count_changed(0);
}

void ParticipantsController::apply_profile(qint64 user_id,
                                           const QString &display_name,
                                           const QString &avatar_url) {
    Partner *partner = add(user_id);
    if (!partner) {
        return;
    }
    partner->setProfile(display_name, avatar_url);

    if (_camera) {
        _camera->setAvatarUrlForUser(user_id, avatar_url);
        if (!_camera->hasActiveVideo(user_id)) {
            _camera->showAvatarForUser(user_id);
        }
        if (user_id == _mainUserId && !_camera->hasActiveVideo(user_id)) {
            _camera->showMainAvatar();
        }
    }

    emit mention_added(display_name);
}

QString ParticipantsController::display_name(qint64 user_id) const {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return QString::number(user_id);
    }
    const QString name = it->second->displayName();
    return name.isEmpty() ? QString::number(user_id) : name;
}

QString ParticipantsController::avatar_url(qint64 user_id) const {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return QString();
    }
    return it->second->avatarUrl();
}

void ParticipantsController::select_main(qint64 user_id) {
    const auto previous = _partners.find(_mainUserId);
    if (previous != _partners.end()) {
        previous->second->resetBorder();
    }
    const auto current = _partners.find(user_id);
    if (current != _partners.end()) {
        current->second->setSelected(true);
    }
    _mainUserId = user_id;
    if (_camera) {
        _camera->refreshMainForUser(_mainUserId);
    }
    emit main_changed(_mainUserId);
}

void ParticipantsController::close_video(qint64 user_id) {
    if (_partners.find(user_id) == _partners.end()) {
        return;
    }
    if (_camera) {
        _camera->showAvatarForUser(user_id);
    }
}
