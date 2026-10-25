#include "meeting_protocol_dispatcher.h"

#include <spdlog/spdlog.h>

MeetingProtocolDispatcher::MeetingProtocolDispatcher(QObject *parent)
    : QObject(parent) {}

namespace {

QString to_qstring(const std::string &value) {
    return QString::fromUtf8(value.c_str(), static_cast<int>(value.size()));
}

}  // namespace

void MeetingProtocolDispatcher::dispatch_request(MessagePtr msg) {
    if (!msg) {
        return;
    }
    switch (msg->kind()) {
    case MessageKind::CreateMeetingResponse: {
        const auto *resp =
            dynamic_cast<const CreateMeetingResponseMessage *>(msg.get());
        if (!resp) {
            return;
        }
        emit create_meeting_result(static_cast<int>(resp->room_no()));
        break;
    }
    case MessageKind::JoinMeetingResponse: {
        const auto *resp =
            dynamic_cast<const JoinMeetingResponseMessage *>(msg.get());
        if (!resp) {
            return;
        }
        emit join_meeting_result(resp->response_code());
        break;
    }
    case MessageKind::RemoteHostClosedError:
        emit remote_host_closed();
        break;
    case MessageKind::OtherNetError:
        emit other_net_error();
        break;
    default:
        break;
    }
}

void MeetingProtocolDispatcher::dispatch_user_info(MessagePtr msg) {
    if (!msg) {
        return;
    }
    switch (msg->kind()) {
    case MessageKind::PartnerJoin:
        emit partner_joined(msg->user_id());
        break;
    case MessageKind::PartnerExit:
        emit partner_exited(msg->user_id());
        break;
    case MessageKind::CloseCameraNotify:
        emit camera_closed(msg->user_id());
        break;
    case MessageKind::PartnerJoin2: {
        const auto *snapshot = dynamic_cast<const PartnerJoin2Message *>(msg.get());
        if (!snapshot) {
            return;
        }
        QList<qint64> ids;
        ids.reserve(static_cast<int>(snapshot->partner_user_ids().size()));
        for (const qint64 user_id : snapshot->partner_user_ids()) {
            ids.push_back(user_id);
        }
        emit partners_snapshot(ids);
        break;
    }
    case MessageKind::RoomSettingsNotify: {
        const auto *settings =
            dynamic_cast<const RoomSettingsNotifyMessage *>(msg.get());
        if (settings) {
            emit room_settings_received(settings->flags());
        }
        break;
    }
    case MessageKind::UserProfileNotify: {
        const auto *profile =
            dynamic_cast<const UserProfileNotifyMessage *>(msg.get());
        if (!profile) {
            return;
        }
        emit user_profile(profile->user_id(),
                          to_qstring(profile->display_name()),
                          to_qstring(profile->avatar_url()));
        break;
    }
    default:
        break;
    }
}

void MeetingProtocolDispatcher::dispatch_text(MessagePtr msg) {
    const auto *text_msg = dynamic_cast<const RecvTextMessage *>(msg.get());
    if (!text_msg) {
        return;
    }
    emit text_received(text_msg->user_id(), to_qstring(text_msg->text()));
}
