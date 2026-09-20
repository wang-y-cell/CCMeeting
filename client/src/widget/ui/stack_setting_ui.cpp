#include "stack_setting_ui.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

QWidget *makeScrollPage(QWidget *parent, QWidget *content) {
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(content);
    return scroll;
}

QLabel *makeFieldLabel(const QString &text, QWidget *parent, int height = 36) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("fieldLabel"));
    label->setAlignment(Qt::AlignCenter);
    //label->setStyleSheet("border: 2px solid black;");
    label->setMinimumHeight(height);
    return label;
}

}  // namespace

void Ui_stack_setting::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("stack_setting"));
    parent->resize(720, 560);

    auto *root = new QVBoxLayout(parent);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(12);

    pageTitle = new QLabel(QObject::tr("设置"), parent);
    pageTitle->setObjectName(QStringLiteral("pageTitle"));
    root->addWidget(pageTitle);

    pageDesc = new QLabel(
        QObject::tr("调整音视频、服务器与界面相关选项"), parent);
    pageDesc->setObjectName(QStringLiteral("pageDesc"));
    root->addWidget(pageDesc);

    auto *body = new QFrame(parent);
    body->setObjectName(QStringLiteral("frame"));
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(12, 12, 12, 12);
    bodyLayout->setSpacing(12);

    categoryList = new QListWidget(body);
    categoryList->setObjectName(QStringLiteral("settingCategoryList"));
    categoryList->setFixedWidth(140);
    categoryList->setFrameShape(QListWidget::NoFrame);
    categoryList->setFocusPolicy(Qt::NoFocus);
    categoryList->setSpacing(2);
    const QStringList categories = {
        QObject::tr("视频"),
        QObject::tr("音频"),
        QObject::tr("通用"),
        QObject::tr("界面"),
    };
    for (const QString &name : categories) {
        auto *item = new QListWidgetItem(name);
        item->setSizeHint(QSize(0, 40));
        item->setTextAlignment(Qt::AlignCenter);
        categoryList->addItem(item);
    }
    bodyLayout->addWidget(categoryList);

    pageStack = new QStackedWidget(body);
    pageStack->setObjectName(QStringLiteral("settingPageStack"));

    // ---------- 视频 ----------
    auto *videoPage = new QWidget;
    auto *videoLayout = new QVBoxLayout(videoPage);
    videoLayout->setContentsMargins(16, 12, 16, 12);
    videoLayout->setSpacing(12);

    auto *videoForm = new QFormLayout();
    videoForm->setHorizontalSpacing(16);
    videoForm->setVerticalSpacing(12);
    videoForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter); // 标签靠左,竖直方向局中

    videoDeviceCombo = new QComboBox(videoPage);
    videoDeviceCombo->setObjectName(QStringLiteral("videoDeviceCombo"));
    videoDeviceCombo->setMinimumHeight(36);
    videoForm->addRow(makeFieldLabel(QObject::tr("摄像头"), videoPage),
                      videoDeviceCombo);

    videoResolution = new QComboBox(videoPage);
    videoResolution->setObjectName(QStringLiteral("videoResolution"));
    videoResolution->setMinimumHeight(36);
    videoForm->addRow(makeFieldLabel(QObject::tr("分辨率"), videoPage),
                      videoResolution);

    videoFps = new QComboBox(videoPage);
    videoFps->setObjectName(QStringLiteral("videoFps"));
    videoFps->setMinimumHeight(36);
    videoForm->addRow(makeFieldLabel(QObject::tr("帧率"), videoPage), videoFps);
    videoLayout->addLayout(videoForm);

    videoPreviewHint = new QLabel(QObject::tr("摄像头预览"), videoPage);
    videoPreviewHint->setObjectName(QStringLiteral("fieldLabel"));
    videoPreviewHint->setFixedHeight(36);
    videoLayout->addWidget(videoPreviewHint);

    // 勿用 QOpenGLWidget：嵌入主窗口会触发整窗 OpenGL 合成，部分环境下一点开设置即崩
    videoPreview = new QLabel(videoPage);
    videoPreview->setObjectName(QStringLiteral("settingsVideoPreview"));
    videoPreview->setFixedHeight(200);
    videoPreview->setAlignment(Qt::AlignCenter);
    videoPreview->setScaledContents(false);
    videoPreview->setStyleSheet(
        QStringLiteral("QLabel#settingsVideoPreview {"
                       "background-color: #12141a; color: #8a90a0;"
                       "border-radius: 8px;}"));
    videoPreview->setText(QObject::tr("预览未启动"));
    videoLayout->addWidget(videoPreview, 1);
    videoLayout->addStretch();

    pageStack->addWidget(makeScrollPage(body, videoPage));

    // ---------- 音频 ----------
    auto *audioPage = new QWidget;
    auto *audioLayout = new QVBoxLayout(audioPage);
    audioLayout->setContentsMargins(16, 12, 16, 12);
    audioLayout->setSpacing(12);

    //auto *audioForm = new QFormLayout();
    auto *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(16);
    gridLayout->setVerticalSpacing(12);

    audioDeviceCombo = new QComboBox(audioPage);
    audioDeviceCombo->setObjectName(QStringLiteral("audioDeviceCombo"));
    audioDeviceCombo->setMinimumHeight(36);
    auto *audioDeviceLabel = makeFieldLabel(QObject::tr("麦克风"), audioPage);
    audioDeviceLabel->setFixedWidth(60);
    gridLayout->addWidget(audioDeviceLabel, 0, 0);
    gridLayout->addWidget(audioDeviceCombo, 0, 1);

    micTestBtn = new QPushButton(QObject::tr("麦克风测试"), audioPage);
    micTestBtn->setObjectName(QStringLiteral("micTestBtn"));
    micTestBtn->setMinimumHeight(36);
    micTestBtn->setFixedWidth(120);
    micTestBtn->setCursor(Qt::PointingHandCursor);
    gridLayout->addWidget(micTestBtn, 0, 2);

    audioLayout->addLayout(gridLayout);
    // 输入电平
    micLevelLabel = new QLabel(QObject::tr("输入电平"), audioPage);
    micLevelLabel->setObjectName(QStringLiteral("fieldLabel"));
    audioLayout->addWidget(micLevelLabel);

    micLevelBar = new QProgressBar(audioPage);
    micLevelBar->setObjectName(QStringLiteral("micLevelBar"));
    micLevelBar->setRange(0, 100);
    micLevelBar->setValue(0);
    micLevelBar->setTextVisible(true);
    micLevelBar->setFormat(QStringLiteral("%p%"));
    micLevelBar->setMinimumHeight(22);
    audioLayout->addWidget(micLevelBar);

    audioMuteOnJoin = new QCheckBox(QObject::tr("加入会议时默认静音麦克风"),
                                    audioPage);
    audioMuteOnJoin->setObjectName(QStringLiteral("audioMuteOnJoin"));
    audioLayout->addWidget(audioMuteOnJoin);

    audioEchoCancel =
        new QCheckBox(QObject::tr("启用回声消除（下次进会生效）"), audioPage);
    audioEchoCancel->setObjectName(QStringLiteral("audioEchoCancel"));
    audioLayout->addWidget(audioEchoCancel);




    audioLayout->addStretch(1);

    pageStack->addWidget(makeScrollPage(body, audioPage));

    // ---------- 通用 ----------
    auto *generalPage = new QWidget;
    auto *generalForm = new QFormLayout(generalPage);
    generalForm->setContentsMargins(16, 12, 16, 12);
    generalForm->setHorizontalSpacing(16);
    generalForm->setVerticalSpacing(14);

    meetingHostEdit = new QLineEdit(generalPage);
    meetingHostEdit->setObjectName(QStringLiteral("meetingHostEdit"));
    meetingHostEdit->setPlaceholderText(QObject::tr("会议服务器地址"));
    generalForm->addRow(makeFieldLabel(QObject::tr("会议服务器"), generalPage),
                        meetingHostEdit);

    meetingPortEdit = new QLineEdit(generalPage);
    meetingPortEdit->setObjectName(QStringLiteral("meetingPortEdit"));
    meetingPortEdit->setPlaceholderText(QObject::tr("端口"));
    generalForm->addRow(makeFieldLabel(QObject::tr("会议端口"), generalPage),
                        meetingPortEdit);

    authHostEdit = new QLineEdit(generalPage);
    authHostEdit->setObjectName(QStringLiteral("authHostEdit"));
    authHostEdit->setPlaceholderText(QObject::tr("认证服务器地址"));
    generalForm->addRow(makeFieldLabel(QObject::tr("认证服务器"), generalPage),
                        authHostEdit);

    authPortEdit = new QLineEdit(generalPage);
    authPortEdit->setObjectName(QStringLiteral("authPortEdit"));
    authPortEdit->setPlaceholderText(QObject::tr("端口"));
    generalForm->addRow(makeFieldLabel(QObject::tr("认证端口"), generalPage),
                        authPortEdit);

    janusUrlEdit = new QLineEdit(generalPage);
    janusUrlEdit->setObjectName(QStringLiteral("janusUrlEdit"));
    janusUrlEdit->setPlaceholderText(QObject::tr("ws://host:8188/"));
    generalForm->addRow(makeFieldLabel(QObject::tr("Janus WebSocket"), generalPage),
                        janusUrlEdit);

    pageStack->addWidget(makeScrollPage(body, generalPage));

    // ---------- 界面 ----------
    auto *uiPage = new QWidget;
    auto *uiForm = new QFormLayout(uiPage);
    uiForm->setContentsMargins(16, 12, 16, 12);
    uiForm->setHorizontalSpacing(16);
    uiForm->setVerticalSpacing(14);

    uiStartPage = new QComboBox(uiPage);
    uiStartPage->setObjectName(QStringLiteral("uiStartPage"));
    uiStartPage->setMinimumHeight(36);
    uiStartPage->addItem(QObject::tr("创建会议"), 0);
    uiStartPage->addItem(QObject::tr("加入会议"), 1);
    uiStartPage->addItem(QObject::tr("设置"), 2);
    uiForm->addRow(makeFieldLabel(QObject::tr("启动默认页"), uiPage), uiStartPage);

    uiShowPageDesc = new QCheckBox(QObject::tr("显示页面说明文字"), uiPage);
    uiShowPageDesc->setObjectName(QStringLiteral("uiShowPageDesc"));
    uiForm->addRow(QString(), uiShowPageDesc);

    pageStack->addWidget(makeScrollPage(body, uiPage));

    bodyLayout->addWidget(pageStack, 1);
    root->addWidget(body, 1);

    auto *actions = new QHBoxLayout();
    actions->setSpacing(12);
    actions->addStretch(1);
    resetBtn = new QPushButton(QObject::tr("恢复默认"), parent);
    resetBtn->setObjectName(QStringLiteral("settingsResetBtn"));
    resetBtn->setMinimumWidth(120);
    resetBtn->setCursor(Qt::PointingHandCursor);
    saveBtn = new QPushButton(QObject::tr("保存设置"), parent);
    saveBtn->setObjectName(QStringLiteral("settingsSaveBtn"));
    saveBtn->setMinimumWidth(140);
    saveBtn->setCursor(Qt::PointingHandCursor);
    actions->addWidget(resetBtn);
    actions->addWidget(saveBtn);
    root->addLayout(actions);

    categoryList->setCurrentRow(0);
}
