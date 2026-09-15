#ifndef MEETING_WIDGET_UI_H
#define MEETING_WIDGET_UI_H

class MyTextEdit;
class VideoGLWidget;
class QGroupBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;
class QVBoxLayout;
class QWidget;

/** 会议窗口纯 UI，不含业务逻辑 */
class Ui_meeting_widget {
public:
    QVBoxLayout *verticalLayout = nullptr;
    QWidget *topStatusBar = nullptr;
    QLabel *topMainTitle = nullptr;
    QLabel *topRoomChip = nullptr;
    QLabel *topMemberChip = nullptr;
    QLabel *topSpeakerLabel = nullptr;
    QLabel *topMeetStatus = nullptr;
    QGroupBox *groupBox_2 = nullptr;
    VideoGLWidget *mainshow_label = nullptr;
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
    QPushButton *btnSideMembers = nullptr;
    QPushButton *btnSideChat = nullptr;
    QPushButton *btnSideInfo = nullptr;
    QPushButton *openAudio = nullptr;
    QPushButton *audioDeviceBtn = nullptr;
    QPushButton *openVedio = nullptr;
    QPushButton *videoDeviceBtn = nullptr;
    QPushButton *btnTogglePanel = nullptr;
    QPushButton *leaveMeetingBtn = nullptr;

    void setupUi(QWidget *parent);
};

#endif // MEETING_WIDGET_UI_H
