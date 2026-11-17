#include "features/meeting/participants_controller.h"

#include "media/avatar_service.h"
#include "media/video/meeting_video.h"
#include "domain/partner.h"
#include "ui/composites/partner_tile.h"
#include "media/video/video_gl_widget.h"

#include <QVBoxLayout>

namespace {
/// 头像统一按这个尺寸加载：小窗显示 72、主画面显示约 10% 控件高，够用且只下载一次
constexpr int kAvatarSourcePx = 256;
}  // namespace

ParticipantsController::ParticipantsController(QVBoxLayout *tiles_layout,
                                               MeetingVideo *video,
                                               QObject *parent)
    : QObject(parent), _tilesLayout(tiles_layout), _video(video) {}

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
        if (_video) {
            _video->bind(user_id, widget);
        }
    }
    // 资料可能比成员事件晚到：先显示默认/已缓存头像，资料到了再刷新
    load_avatar(user_id, partner->avatarUrl());
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
    if (_video) {
        _video->bind(user_id, nullptr);
    }
    _avatarGen.remove(user_id);
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
        _avatarGen.clear();
        return;
    }
    for (auto it = _partners.begin(); it != _partners.end();) {
        Partner *partner = it->second;
        disconnect(partner, &Partner::clicked, this,
                   &ParticipantsController::member_clicked);
        if (_video) {
            _video->bind(it->first, nullptr);
        }
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
    _avatarGen.clear();
    _mainUserId = 0;
    emit count_changed(0);
}

void ParticipantsController::load_avatar(qint64 user_id, const QString &url) {
    const quint64 generation = ++_avatarGen[user_id];

    AvatarService::instance().load(
        url, QSize(kAvatarSourcePx, kAvatarSourcePx), this,
        [this, user_id, generation](const QPixmap &pixmap) {
            if (_avatarGen.value(user_id) != generation) {
                return;  // 该用户的头像已更新，丢弃过期结果
            }
            const auto it = _partners.find(user_id);
            if (it == _partners.end() || pixmap.isNull()) {
                return;
            }
            const QImage avatar = makeCircularAvatarImage(pixmap.toImage());
            if (PartnerTile *tile = it->second->tile()) {
                tile->showAvatar(avatar);
            }
            if (_video) {
                _video->setAvatar(user_id, avatar);
            }
        });
}

void ParticipantsController::apply_profile(qint64 user_id,
                                           const QString &display_name,
                                           const QString &avatar_url) {
    Partner *partner = add(user_id);
    if (!partner) {
        return;
    }
    partner->setProfile(display_name, avatar_url);
    load_avatar(user_id, avatar_url);
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
    if (_video) {
        _video->setMainUser(_mainUserId);
    }
    emit main_changed(_mainUserId);
}

void ParticipantsController::close_video(qint64 user_id) {
    if (_partners.find(user_id) == _partners.end()) {
        return;
    }
    if (_video) {
        _video->clearVideo(user_id);
    }
}
