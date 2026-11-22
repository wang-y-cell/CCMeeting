#include "features/meeting/participants_controller.h"

#include "media/avatar_service.h"
#include "media/video/meeting_video.h"
#include "media/video/video_gl_widget.h"
#include "ui/composites/participant_card.h"

#include <QVBoxLayout>

namespace {
/// 头像统一按这个尺寸加载：小窗显示 72、主画面显示约 10% 控件高，够用且只下载一次
constexpr int kAvatarSourcePx = 256;
}  // namespace

ParticipantsController::ParticipantsController(QVBoxLayout *tiles_layout,
                                               MeetingVideo *video,
                                               QObject *parent)
    : QObject(parent), _tilesLayout(tiles_layout), _video(video) {}

ParticipantCard *ParticipantsController::add(qint64 user_id) {
    const auto found = _partners.find(user_id);
    if (found != _partners.end()) {
        return found->second;
    }

    auto *card = new ParticipantCard(
        user_id, _tilesLayout ? _tilesLayout->parentWidget() : nullptr);
    connect(card, &ParticipantCard::clicked, this,
            &ParticipantsController::member_clicked);
    _partners.emplace(user_id, card);
    if (_tilesLayout) {
        _tilesLayout->addWidget(card);
    }
    if (_video) {
        _video->bind(user_id, card->displayWidget());
    }
    emit count_changed(count());
    return card;
}

void ParticipantsController::remove(qint64 user_id) {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return;
    }
    ParticipantCard *card = it->second;
    disconnect(card, &ParticipantCard::clicked, this,
               &ParticipantsController::member_clicked);
    if (_video) {
        _video->bind(user_id, nullptr);
    }
    _avatarGen.remove(user_id);
    if (_tilesLayout) {
        _tilesLayout->removeWidget(card);
    }
    card->deleteLater();
    _partners.erase(it);

    // 主画面用户离开：清空主画面，等用户重新选择
    if (_mainUserId == user_id) {
        _mainUserId = 0;
        if (_video) {
            _video->setMainUser(0);
        }
        emit main_changed(0);
    }
    emit count_changed(count());
}

void ParticipantsController::clear() {
    for (auto it = _partners.begin(); it != _partners.end();) {
        ParticipantCard *card = it->second;
        disconnect(card, &ParticipantCard::clicked, this,
                   &ParticipantsController::member_clicked);
        if (_video) {
            _video->bind(it->first, nullptr);
        }
        if (_tilesLayout) {
            _tilesLayout->removeWidget(card);
        }
        card->deleteLater();
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
            it->second->show_avatar(avatar);
            if (_video) {
                _video->setAvatar(user_id, avatar);
            }
        });
}

void ParticipantsController::apply_profile(qint64 user_id,
                                           const QString &display_name,
                                           const QString &avatar_url) {
    ParticipantCard *card = add(user_id);
    if (!card) {
        return;
    }
    card->set_profile(display_name, avatar_url);
    load_avatar(user_id, avatar_url);
    emit mention_added(display_name);
}

QString ParticipantsController::display_name(qint64 user_id) const {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return QString::number(user_id);
    }
    const QString name = it->second->display_name();
    return name.isEmpty() ? QString::number(user_id) : name;
}

QString ParticipantsController::avatar_url(qint64 user_id) const {
    const auto it = _partners.find(user_id);
    if (it == _partners.end()) {
        return QString();
    }
    return it->second->avatar_url();
}

void ParticipantsController::set_speaking(qint64 user_id, bool speaking) {
    const auto it = _partners.find(user_id);
    if (it != _partners.end() && it->second) {
        it->second->set_speaking(speaking);
    }
}

void ParticipantsController::select_main(qint64 user_id) {
    _mainUserId = user_id;
    if (_video) {
        _video->setMainUser(_mainUserId);
    }
    emit main_changed(_mainUserId);
}

void ParticipantsController::set_main_user(qint64 user_id) {
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
