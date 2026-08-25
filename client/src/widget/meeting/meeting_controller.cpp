#include "meeting_controller.h"

#include <QThread>
#include <spdlog/spdlog.h>

MeetingController::MeetingController(std::shared_ptr<NetworkManager> network,
                                     QObject *parent)
    : QObject(parent), _network(std::move(network)) {}

void MeetingController::connect_to_server_slot(QString ip, QString port,
                                               ConnectAction action,
                                               QString room_no) {
    spdlog::debug("[MeetingController] connect_to_server_slot on thread {}",
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));
    bool ok = false;
    if (_network)
        ok = _network->connectToServer(ip, port, nullptr);
    emit connect_finished_signal(ok, ip, port, action, room_no);
}

void MeetingController::create_meeting_slot(quint32 max_participants,
                                            quint32 duration_minutes) {
    spdlog::info(
        "[MeetingController] create_meeting_slot max_participants={} "
        "duration_minutes={}",
        max_participants, duration_minutes);
    if (spdlog::default_logger())
        spdlog::default_logger()->flush();
    if (_network)
        _network->sendCreateMeeting(max_participants, duration_minutes);
}

void MeetingController::join_meeting_slot(QString room_no) {
    const QByteArray room = room_no.toUtf8();
    spdlog::info("[MeetingController] join_meeting_slot room_no={}",
                 room.constData());
    if (_network)
        _network->sendJoinMeeting(
            std::string(room.constData(), static_cast<std::size_t>(room.size())));
}

void MeetingController::disconnect_from_host_slot() {
    spdlog::debug("[MeetingController] disconnect_from_host_slot");
    if (_network)
        _network->disconnectFromHost();
}
