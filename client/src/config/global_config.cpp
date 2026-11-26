#include "config/global_config.h"

namespace config {

namespace app {

// 注意：app_name 决定 %LOCALAPPDATA%\<app_name> 目录名，
// Windows 下大小写不敏感（CCMeeting 与 ccmeeting 是同一个目录），
// 但换名字会导致找不到旧数据/日志，非必要不要改。
const std::string &app_name = "CCMeeting";
const std::string &ccmeeting_version = "1.2.0";
const std::string &display_name = "CloudMeeting";
const std::string &meeting_window_title = "云会议";
const std::string &main_window_title = "CloudMeeting";
const std::string &about_text =
    display_name + " " + ccmeeting_version;

}  // namespace app

}  // namespace config
