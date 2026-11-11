#ifndef MEETING_WIDGET_UI_H
#define MEETING_WIDGET_UI_H

class MyTextEdit;
class VideoGLWidget;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QToolButton;
class QTabWidget;
class QVBoxLayout;
class QWidget;

/** 会议窗口纯 UI，不含业务逻辑 */
namespace ui {

class MeetingWindowUi {
public:
    QVBoxLayout *verticalLayout = nullptr;
    QWidget *topStatusBar = nullptr;
    QLabel *topDurationChip = nullptr;
    QLabel *topMainTitle = nullptr;
    QLabel *topRoomChip = nullptr;
    QLabel *topMemberChip = nullptr;
    QLabel *topSpeakerLabel = nullptr;
    QLabel *topMeetStatus = nullptr;
    QPushButton *btnMeetingInfo = nullptr;
    QPushButton *btnMeetingSettings = nullptr;
    QGroupBox *groupBox_2 = nullptr;
    VideoGLWidget *mainshow_label = nullptr;
    QLabel *canvasHint = nullptr;
    QTabWidget *tabWidget = nullptr;
    QWidget *scrollAreaWidgetContents = nullptr;
    QVBoxLayout *verticalLayout_3 = nullptr;
    QListWidget *listWidget = nullptr;
    MyTextEdit *plainTextEdit = nullptr;
    QPushButton *sendmsg = nullptr;
    QLabel *labelMeetStatus = nullptr;
    QLabel *labelRoomNo = nullptr;
    QLabel *labelMemberCount = nullptr;
    QLabel *labelLocalIp = nullptr;
    QLabel *labelServer = nullptr;
    QLabel *labelSpeaker = nullptr;
    QWidget *avToolbar = nullptr;
    QWidget *controlPill = nullptr;
    QToolButton *btnSideMembers = nullptr;
    QToolButton *btnSideChat = nullptr;
    QToolButton *btnSideInfo = nullptr;
    QToolButton *openAudio = nullptr;
    QToolButton *audioDeviceBtn = nullptr;
    QToolButton *openVedio = nullptr;
    QToolButton *videoDeviceBtn = nullptr;
    QToolButton *shareScreenBtn = nullptr;
    QToolButton *btnTogglePanel = nullptr;
    QToolButton *leaveMeetingBtn = nullptr;

    void setupUi(QWidget *parent);
};

}  // namespace ui

#endif // MEETING_WIDGET_UI_H
