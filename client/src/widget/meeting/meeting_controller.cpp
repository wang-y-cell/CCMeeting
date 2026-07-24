#include "meeting_controller.h"

#include <QThread>
#include <spdlog/spdlog.h>

MeetingController::MeetingController(std::shared_ptr<NetworkManager> network, QObject *parent)
    : QObject(parent)
    , _network(std::move(network))
{
}

void MeetingController::connect_to_server_slot(QString ip, QString port, ConnectAction action, QString room_no)
{
    spdlog::debug("[MeetingController] connect_to_server_slot on thread {}",
                  reinterpret_cast<quintptr>(QThread::currentThreadId()));
    bool ok = false;
    if (_network)
        ok = _network->connectToServer(ip, port, nullptr);
    emit connect_finished_signal(ok, ip, port, action, room_no);
}

void MeetingController::create_meeting_slot()
{
    spdlog::debug("[MeetingController] create_meeting_slot");
    if (_network)
        _network->sendCreateMeeting();
}

void MeetingController::join_meeting_slot(QString room_no)
{
    spdlog::debug("[MeetingController] join_meeting_slot room_no={}", room_no.toStdString());
    if (_network)
        _network->sendJoinMeeting(room_no.toStdString());
}

void MeetingController::disconnect_from_host_slot()
{
    spdlog::debug("[MeetingController] disconnect_from_host_slot");
    if (_network)
        _network->disconnectFromHost();
}
