#ifndef MEETING_STATE_H
#define MEETING_STATE_H

#include <QString>
#include <QtGlobal>

namespace meeting {

/**
 * @brief 会议会话状态（从 MeetingWindow 抽出的纯数据）
 *
 * 「连接挂起/串行化」相关字段在 ConnectCoordinator；这里只保留
 * 界面与会话本身需要的状态，由 MeetingWindow 持有、各视图控制器引用。
 */
struct MeetingState {
    bool _createmeet = false;      ///< 是否创建会议
    bool _joinmeet = false;        ///< 是否加入会议

    bool _rtcJoined = false;       ///< WebRTC join 是否已成功（才可调 start/stop_local_*）
    bool _localVideoOn = false;    ///< 本端摄像头是否已开采集推流
    bool _localAudioOn = false;    ///< 本端麦克风是否已开
    bool _localScreenOn = false;   ///< 是否正在共享屏幕（与摄像头互斥）

    bool _sessionActive = false;   ///< 会议是否活跃
    bool _sessionEnding = false;   ///< 会议是否正在结束

    int _roomNo = 0;                 ///< 当前房间号
    QString _serverAddr;             ///< "ip:port"，顶栏/信息面板显示用
    qint64 _sessionStartMs = 0;      ///< 本次会议开始时刻（统计时长），0 表示未开始
    qint64 _startupBeginMs = 0;      ///< 启动计时起点（点创建/加入的时刻）
    qint64 _startupJoinedMs = 0;     ///< 到 xrtc 进房成功的耗时（毫秒）
    qint64 _startupFirstFrameMs = 0; ///< 到首帧画面的耗时（毫秒）
};

}  // namespace meeting

#endif  // MEETING_STATE_H
