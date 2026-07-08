#include "protocol/message_type.h"

namespace protocol {

std::string get_type_name(MessageType type) {
    switch (type) {
        case MessageType::ImgSend:
            return "ImgSend";
        case MessageType::ImgRecv:
            return "ImgRecv";
        case MessageType::AudioSend:
            return "AudioSend";
        case MessageType::AudioRecv:
            return "AudioRecv";
        case MessageType::TextSend:
            return "TextSend";
        case MessageType::TextRecv:
            return "TextRecv";
        case MessageType::CreateMeeting:
            return "CreateMeeting";
        case MessageType::ExitMeeting:
            return "ExitMeeting";
        case MessageType::JoinMeeting:
            return "JoinMeeting";
        case MessageType::CloseCamera:
            return "CloseCamera";
        case MessageType::CreateMeetingResponse:
            return "CreateMeetingResponse";
        case MessageType::PartnerExit:
            return "PartnerExit";
        case MessageType::PartnerJoin:
            return "PartnerJoin";
        case MessageType::JoinMeetingResponse:
            return "JoinMeetingResponse";
        case MessageType::PartnerJoin2:
            return "PartnerJoin2";
        default:
            return "Unknown";
    }
}

}  // namespace protocol