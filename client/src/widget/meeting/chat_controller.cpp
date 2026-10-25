#include "chat_controller.h"

#include "chatmessage.h"
#include "configure/user_session.h"
#include "mytextedit.h"

#include <QCompleter>
#include <QDateTime>
#include <QEvent>
#include <QListWidget>
#include "storage.h"
#include <QSoundEffect>
#include <QUrl>
#include <QWidget>

#include <spdlog/spdlog.h>

#include <algorithm>

ChatController::ChatController(QListWidget *list, MyTextEdit *input,
                               QWidget *owner, QObject *parent)
    : QObject(parent), _list(list), _input(input), _owner(owner) {
    if (_list) {
        _list->viewport()->installEventFilter(this);
    }
}

QString ChatController::submit_input() {
    if (!_input || !_list) {
        return QString();
    }
    const QString text = _input->toPlainText().trimmed();
    if (text.isEmpty()) {
        return QString();
    }
    _input->setPlainText(QString());

    const QString time =
        QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    auto *message = new ChatMessage(_list);
    auto *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, text, time, UserSession::instance().name(),
                 ChatMessage::User_Me, UserSession::instance().avatar());
    _pending = message;
    return text;
}

void ChatController::finish_pending_immediately() {
    if (_pending) {
        _pending->setTextSuccess();
        _pending = nullptr;
    }
}

void ChatController::mark_pending_sent() { finish_pending_immediately(); }

void ChatController::append_remote(const QString &text,
                                   const QString &sender_name,
                                   const QString &avatar_url, bool mention_me) {
    if (!_list) {
        return;
    }
    const QString time =
        QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    auto *message = new ChatMessage(_list);
    auto *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, text, time, sender_name,
                 ChatMessage::User_She, avatar_url);

    if (mention_me) {
        play_mention_sound();
    }
}

void ChatController::append_local(const QString &text,
                                  const QString &sender_name,
                                  const QString &avatar_url) {
    if (!_list) {
        return;
    }
    const QString time =
        QString::number(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());
    auto *message = new ChatMessage(_list);
    auto *item = new QListWidgetItem();
    deal_message_time(time);
    deal_message(message, item, text, time, sender_name, ChatMessage::User_Me,
                 avatar_url);
}

void ChatController::play_mention_sound() {
    // 提示音可以在「设置 → 通用」里关闭
    const bool enabled =
        Storage::instance()
            .value(QStringLiteral("ui/soundEnabled"), true)
            .toBool();
    if (!enabled) {
        return;
    }
    if (!_sound) {
        _sound = new QSoundEffect(this);
        _sound->setSource(QUrl(QStringLiteral("qrc:/myEffect/2.wav")));
        _sound->setVolume(1.0);
    }
    _sound->play();
}

void ChatController::clear() {
    if (!_list) {
        return;
    }
    while (_list->count() > 0) {
        QListWidgetItem *item = _list->item(0);
        QWidget *widget = _list->itemWidget(item);
        _list->takeItem(0);
        delete widget;
        delete item;
    }
    _pending = nullptr;
}

void ChatController::set_input_enabled(bool enabled) {
    if (_input) {
        _input->setEnabled(enabled);
    }
}

void ChatController::add_mention_tag(const QString &tag) {
    if (!_input || tag.isEmpty()) {
        return;
    }
    if (std::find(_mentionTags.begin(), _mentionTags.end(), tag) !=
        _mentionTags.end()) {
        return;
    }
    _mentionTags.push_back(tag);
    _input->setCompleter(_mentionTags);
}

void ChatController::remove_mention_tag(const QString &tag) {
    if (!_input || tag.isEmpty()) {
        return;
    }
    _mentionTags.erase(
        std::remove(_mentionTags.begin(), _mentionTags.end(), tag),
        _mentionTags.end());
    _input->setCompleter(_mentionTags);
}

bool ChatController::eventFilter(QObject *watched, QEvent *event) {
    if (_list && watched == _list->viewport() &&
        event->type() == QEvent::Resize) {
        relayout_messages();
    }
    return QObject::eventFilter(watched, event);
}

void ChatController::relayout_messages() {
    if (_inRelayout || !_list) {
        return;
    }
    const int listWidth = _list->viewport()->width();
    if (listWidth <= 0 || listWidth == _lastListWidth) {
        return;
    }
    _inRelayout = true;
    _lastListWidth = listWidth;
    _list->setUpdatesEnabled(false);
    for (int i = 0; i < _list->count(); ++i) {
        QListWidgetItem *item = _list->item(i);
        if (auto *message = qobject_cast<ChatMessage *>(_list->itemWidget(item))) {
            item->setSizeHint(message->relayoutForWidth(listWidth));
        }
    }
    _list->setUpdatesEnabled(true);
    _inRelayout = false;
}

void ChatController::deal_message(ChatMessage *message, QListWidgetItem *item,
                                  const QString &text, const QString &time,
                                  const QString &senderName,
                                  ChatMessage::User_Type type,
                                  const QString &avatarUrl) {
    _list->addItem(item);
    const int listWidth = _list->viewport()->width();
    message->setFixedWidth(listWidth > 0 ? listWidth : _list->width());
    const QSize size = message->fontRect(text);
    item->setSizeHint(size);
    message->setText(text, time, size, senderName, type, avatarUrl);
    _list->setItemWidget(item, message);
}

void ChatController::deal_message_time(const QString &curMsgTime) {
    bool isShowTime = _list->count() == 0;
    if (!isShowTime) {
        QListWidgetItem *lastItem = _list->item(_list->count() - 1);
        if (auto *message =
                qobject_cast<ChatMessage *>(_list->itemWidget(lastItem))) {
            isShowTime = (curMsgTime.toInt() - message->time().toInt()) > 60;
        }
    }
    if (!isShowTime) {
        return;
    }

    auto *timeMessage = new ChatMessage(_list);
    auto *itemTime = new QListWidgetItem();
    _list->addItem(itemTime);
    const int listWidth = _list->viewport()->width();
    const int w = listWidth > 0 ? listWidth : _list->width();
    const QSize size(w, 20);
    timeMessage->setFixedWidth(w);
    timeMessage->resize(size);
    itemTime->setSizeHint(size);
    timeMessage->setText(curMsgTime, curMsgTime, size);
    _list->setItemWidget(itemTime, timeMessage);
}
