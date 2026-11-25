#include "meeting/room.h"
#include "protocol/message_type.h"
#include "protocol/codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <utility>

namespace meeting {

namespace {

using json = nlohmann::json;

bool read_bool(const json& data, const char* key, bool fallback) {
    if (!data.is_object()) {
        return fallback;
    }
    const auto it = data.find(key);
    if (it == data.end() || !it->is_boolean()) {
        return fallback;
    }
    return it->get<bool>();
}

std::string read_string(const json& data, const char* key,
                        const std::string& fallback = std::string()) {
    if (!data.is_object()) {
        return fallback;
    }
    const auto it = data.find(key);
    if (it == data.end() || !it->is_string()) {
        return fallback;
    }
    return it->get<std::string>();
}

/// 单个用户事件（partner_join / partner_exit / close_camera）：data = {"user_id":...}
protocol::Packet make_user_event_packet(protocol::MessageType type,
                                        uint64_t user_id) {
    protocol::Packet packet;
    packet.type = type;
    packet.user_id = user_id;
    packet.data = json{{"user_id", user_id}};
    return packet;
}

/// 房间开关广播：data = {"allow_record":bool,"allow_member_share":bool}
protocol::Packet make_room_settings_packet(bool allow_record,
                                           bool allow_member_share) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::RoomSettingsNotify;
    packet.data = json{{"allow_record", allow_record},
                       {"allow_member_share", allow_member_share}};
    return packet;
}

/// 用户资料：data = {"user_id":...,"nickname":"...","avatar":"..."}
protocol::Packet make_user_profile_packet(const Participant& p) {
    protocol::Packet packet;
    packet.type = protocol::MessageType::UserProfile;
    packet.user_id = p.user_id;
    packet.data = json{{"user_id", p.user_id},
                       {"nickname", p.display_name},
                       {"avatar", p.avatar_url}};
    return packet;
}

}  // namespace

Room::Room(uint32_t room_id,
           std::shared_ptr<network::Connection> owner,
           const config::ServerConfig& config,
           boost::asio::io_context& io_ctx,
           RoomOptions options,
           uint64_t owner_user_id)
    : _room_id(room_id),
      _config(config),
      _options(std::move(options)),
      _io_ctx(io_ctx) {
    if (_options.max_participants == 0) {
        _options.max_participants = _config.max_participants_per_room;
    }
    _options.max_participants =
        std::min(_options.max_participants, _config.max_participants_per_room);
    add_participant(std::move(owner), true, owner_user_id);
}

void Room::start_expire_timer() {
    if (_options.duration_minutes == 0 || _closed) {
        return;
    }
    _expire_timer = std::make_unique<boost::asio::steady_timer>(_io_ctx);
    _expire_timer->expires_after(std::chrono::minutes(_options.duration_minutes));
    auto self = shared_from_this();
    _expire_timer->async_wait(
        [self](const boost::system::error_code& ec) {
            if (ec) {
                return;
            }
            spdlog::info("room {} duration expired ({} minutes)", self->_room_id,
                         self->_options.duration_minutes);
            self->close_room();
        });
    spdlog::info("room {} expire timer started, duration={} minutes", _room_id,
                 _options.duration_minutes);
}

void Room::set_close_callback(CloseCallback callback) {
    _close_callback = std::move(callback);
}

bool Room::add_participant(std::shared_ptr<network::Connection> conn,
                           bool is_owner, uint64_t user_id) {
    if (_closed || !conn || !conn->is_open()) {
        spdlog::warn("room {} add participant failed", _room_id);
        return false;
    }
    if (_participants.size() >= _options.max_participants) {
        spdlog::warn("room {} is full (max={})", _room_id, _options.max_participants);
        return false;
    }
    if (user_id == 0) {
        spdlog::warn("room {} reject participant with user_id=0", _room_id);
        return false;
    }

    Participant participant;
    participant.connection = conn;
    participant.user_id = user_id;
    participant.is_owner = is_owner;
    _participants.emplace(conn->id(), std::move(participant));
    return true;
}

void Room::remove_participant(network::Connection::Id conn_id) {
    if (_closed) {
        return;
    }

    auto it = _participants.find(conn_id);
    if (it == _participants.end()) {
        return;
    }

    const bool was_owner = it->second.is_owner;
    const uint64_t user_id = it->second.user_id;
    _participants.erase(it);

    if (was_owner) {
        close_room();
        return;
    }

    // 普通成员离开：广播 partner_exit（exit_meeting 也走这里，不重复广播）
    broadcast(make_user_event_packet(protocol::MessageType::PartnerExit, user_id));
}

void Room::close_room() {
    if (_closed) {
        return;
    }
    _closed = true;

    if (_expire_timer) {
        _expire_timer->cancel();
        _expire_timer.reset();
    }

    auto participants = _participants;
    _participants.clear();
    for (auto& item : participants) {
        if (item.second.connection && item.second.connection->is_open()) {
            item.second.connection->close();
        }
    }

    if (_close_callback) {
        _close_callback(_room_id);
    }
}

void Room::send_to(std::shared_ptr<network::Connection> target,
                   const protocol::Packet& packet) {
    if (!target || !target->is_open()) {
        return;
    }
    target->send(protocol::Codec::encode(packet));
}

void Room::broadcast(const protocol::Packet& packet,
                       network::Connection::Id exclude) {
    spdlog::debug("room {} broadcast packet to {} participants", _room_id, _participants.size());
    const auto encoded = protocol::Codec::encode(packet);
    for (const auto& item : _participants) {
        if (item.first == exclude) {
            continue;
        }
        if (item.second.connection && item.second.connection->is_open()) {
            item.second.connection->send(encoded);
        }
    }
}

void Room::handle_packet(std::shared_ptr<network::Connection> from,
                         const protocol::Packet& packet) {
    if (_closed || !from) {
        return;
    }

    auto participant_it = _participants.find(from->id());
    if (participant_it == _participants.end()) {
        return;
    }

    // 身份来自房间成员表（服务层用 token 或连接状态填好），不信任上行 data
    const uint64_t sender_user_id = participant_it->second.user_id;
    spdlog::debug("room {} handle packet from connection {}, type: {}", _room_id, from->id(), protocol::get_type_name(packet.type));

    switch (packet.type) {
        case protocol::MessageType::TextSend: {
            protocol::Packet outbound;
            outbound.type = protocol::MessageType::TextRecv;
            outbound.user_id = sender_user_id;
            outbound.data = json{{"user_id", sender_user_id},
                                 {"text", read_string(packet.data, "text")}};
            broadcast(outbound, from->id());
            break;
        }
        case protocol::MessageType::ExitMeeting: {
            // 离开房间：remove_participant 负责广播 partner_exit；房主离开则关房。
            // 这里不再做二次清理，避免重复广播 / 重复关房。
            remove_participant(from->id());
            spdlog::info("room {} connection {} left by exit_meeting", _room_id,
                         from->id());
            break;
        }
        case protocol::MessageType::CloseCamera: {
            broadcast(make_user_event_packet(protocol::MessageType::CloseCamera,
                                             sender_user_id),
                      from->id());
            break;
        }
        case protocol::MessageType::UserProfile: {
            // 上行只带 nickname/avatar，user_id 以房间成员身份为准
            participant_it->second.display_name =
                read_string(packet.data, "nickname",
                            participant_it->second.display_name);
            participant_it->second.avatar_url =
                read_string(packet.data, "avatar",
                            participant_it->second.avatar_url);

            protocol::Packet outbound =
                make_user_profile_packet(participant_it->second);
            broadcast(outbound, from->id());
            break;
        }
        case protocol::MessageType::RoomSettings: {
            // 只有房主能改房间开关；其余成员忽略并记日志
            if (!participant_it->second.is_owner) {
                spdlog::warn(
                    "room {} rejected room_settings from non-owner connection {}",
                    _room_id, from->id());
                break;
            }
            _allow_record =
                read_bool(packet.data, "allow_record", _allow_record);
            _allow_member_share =
                read_bool(packet.data, "allow_member_share", _allow_member_share);
            spdlog::info(
                "room {} settings updated by owner: allow_record={} "
                "allow_member_share={}",
                _room_id, _allow_record, _allow_member_share);

            broadcast(make_room_settings_packet(_allow_record,
                                                _allow_member_share));
            break;
        }
        default:
            spdlog::warn("room {} ignored message type {}", _room_id,
                         static_cast<int>(packet.type));
            break;
    }
}

void Room::notify_user_joined(std::shared_ptr<network::Connection> newcomer) {
    if (_closed || !newcomer) {
        return;
    }

    auto newcomer_it = _participants.find(newcomer->id());
    if (newcomer_it == _participants.end()) {
        return;
    }
    const uint64_t newcomer_user_id = newcomer_it->second.user_id;

    broadcast(make_user_event_packet(protocol::MessageType::PartnerJoin,
                                     newcomer_user_id),
              newcomer->id());

    json user_ids = json::array();
    for (const auto& item : _participants) {
        if (item.first == newcomer->id()) {
            continue;
        }
        user_ids.push_back(item.second.user_id);
    }
    protocol::Packet partner_list;
    partner_list.type = protocol::MessageType::PartnerJoin2;
    partner_list.data = json{{"user_ids", std::move(user_ids)}};
    send_to(newcomer, partner_list);

    // 房间开关：新人入会先同步当前策略，避免界面默认值与房间实际不一致
    send_to(newcomer,
            make_room_settings_packet(_allow_record, _allow_member_share));

    // partner_join2 只有 user_id；把房内已缓存的资料补发给新人，否则新人看不到对方名称
    for (const auto& item : _participants) {
        if (item.first == newcomer->id()) {
            continue;
        }
        const auto& p = item.second;
        if (p.user_id == 0 ||
            (p.display_name.empty() && p.avatar_url.empty())) {
            continue;
        }
        send_to(newcomer, make_user_profile_packet(p));
    }
}

}  // namespace meeting
