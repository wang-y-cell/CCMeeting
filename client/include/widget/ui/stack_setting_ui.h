#ifndef STACK_SETTING_UI_H
#define STACK_SETTING_UI_H

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QWidget;

class Ui_stack_setting {
public:
    void setupUi(QWidget *parent);

    QLabel *pageTitle = nullptr;
    QLabel *pageDesc = nullptr;
    QListWidget *categoryList = nullptr;
    QStackedWidget *pageStack = nullptr;

    // 视频（预览用 QLabel，避免 QOpenGLWidget 嵌入主窗口导致整窗 OpenGL 合成崩溃）
    QComboBox *videoDeviceCombo = nullptr;
    QComboBox *videoResolution = nullptr;
    QComboBox *videoFps = nullptr;
    QLabel *videoPreview = nullptr;
    QLabel *videoPreviewHint = nullptr;

    // 音频
    QComboBox *audioDeviceCombo = nullptr;
    QCheckBox *audioMuteOnJoin = nullptr;
    QCheckBox *audioEchoCancel = nullptr;
    QPushButton *micTestBtn = nullptr;
    QProgressBar *micLevelBar = nullptr;
    QLabel *micLevelLabel = nullptr;

    // 通用
    QLineEdit *meetingHostEdit = nullptr;
    QLineEdit *meetingPortEdit = nullptr;
    QLineEdit *authHostEdit = nullptr;
    QLineEdit *authPortEdit = nullptr;
    QLineEdit *janusUrlEdit = nullptr;

    // 界面
    QComboBox *uiStartPage = nullptr;
    QCheckBox *uiShowPageDesc = nullptr;

    QPushButton *saveBtn = nullptr;
    QPushButton *resetBtn = nullptr;
};

#endif // STACK_SETTING_UI_H
