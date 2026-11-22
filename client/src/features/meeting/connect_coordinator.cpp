#include "features/meeting/connect_coordinator.h"

#include <spdlog/spdlog.h>

ConnectCoordinator::ConnectCoordinator(QObject *parent) : QObject(parent) {}

void ConnectCoordinator::request(const QString &ip, const QString &port,
                                 ConnectAction action, const QString &room_no,
                                 quint32 max_participants,
                                 quint32 duration_minutes) {
    if (_sessionEnding || _connecting) {
        // 上一次连接/结束还没走完：记下参数，等 on_connected()/flush_pending() 补发
        _hasPending = true;
        _pendingIp = ip;
        _pendingPort = port;
        _pendingAction = action;
        _pendingRoomNo = room_no;
        _pendingMaxParticipants = max_participants;
        _pendingDurationMinutes = duration_minutes;
        spdlog::debug("[Connect] deferred (ending={} connecting={})",
                     _sessionEnding, _connecting);
        return;
    }

    dispatch(ip, port, action, room_no, max_participants, duration_minutes);
}

void ConnectCoordinator::dispatch(const QString &ip, const QString &port,
                                  ConnectAction action, const QString &room_no,
                                  quint32 max_participants,
                                  quint32 duration_minutes) {
    _connecting = true;
    _maxParticipants = max_participants;
    _durationMinutes = duration_minutes;
    _currentAction = action;
    _currentRoomNo = room_no;
    emit connect_requested(ip, port, action, room_no);
}

void ConnectCoordinator::on_connected(bool ok, const QString &ip,
                                      const QString &port,
                                      ConnectAction action) {
    _connecting = false;
    emit connect_finished(ok, ip, port, action, _currentRoomNo);

    if (!ok) {
        return;
    }

    if (action == ConnectAction::CreateMeeting) {
        emit create_requested(_maxParticipants, _durationMinutes);
    } else if (action == ConnectAction::JoinMeeting) {
        emit join_requested(_currentRoomNo);
    }
}

void ConnectCoordinator::flush_pending() {
    if (!_hasPending || _sessionEnding) {
        return;
    }
    _hasPending = false;
    const QString ip = _pendingIp;
    const QString port = _pendingPort;
    const ConnectAction action = _pendingAction;
    const QString roomNo = _pendingRoomNo;
    const quint32 maxParticipants = _pendingMaxParticipants;
    const quint32 durationMinutes = _pendingDurationMinutes;
    _pendingIp.clear();
    _pendingPort.clear();
    _pendingRoomNo.clear();
    _pendingAction = ConnectAction::CreateMeeting;
    _pendingMaxParticipants = 8;
    _pendingDurationMinutes = 60;
    request(ip, port, action, roomNo, maxParticipants, durationMinutes);
}

void ConnectCoordinator::cancel_pending() {
    _hasPending = false;
    _pendingIp.clear();
    _pendingPort.clear();
    _pendingRoomNo.clear();
    _pendingAction = ConnectAction::CreateMeeting;
}
