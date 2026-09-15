#include "meeting_widget.h"

#include "text/mytextedit.h"
#include "videoglwidget.h"

#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpacerItem>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QLabel *makeLabel(const QString &objectName, const QString &text,
                  QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName(objectName);
    return label;
}

QPushButton *makeToolButton(const QString &objectName, const QString &text,
                            const QSize &minSize, QWidget *parent,
                            const QString &toolTip = QString()) {
    auto *btn = new QPushButton(text, parent);
    btn->setObjectName(objectName);
    btn->setMinimumSize(minSize);
    if (!toolTip.isEmpty())
        btn->setToolTip(toolTip);
    return btn;
}

QFrame *makePillSep(const QString &objectName, QWidget *parent) {
    auto *sep = new QFrame(parent);
    sep->setObjectName(objectName);
    sep->setFixedSize(1, 28);
    sep->setFrameShape(QFrame::NoFrame);
    return sep;
}

}  // namespace

void Ui_meeting_widget::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("Widget"));
    parent->resize(922, 610);
    parent->setWindowTitle(QObject::tr("云会议"));

    verticalLayout = new QVBoxLayout(parent);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    verticalLayout->setSpacing(0);
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    // ---- 顶栏 ----
    topStatusBar = new QWidget(parent);
    topStatusBar->setObjectName(QStringLiteral("topStatusBar"));
    topStatusBar->setMinimumHeight(40);
    topStatusBar->setMaximumHeight(40);

    auto *topStatusLayout = new QHBoxLayout(topStatusBar);
    topStatusLayout->setObjectName(QStringLiteral("topStatusLayout"));
    topStatusLayout->setSpacing(12);
    topStatusLayout->setContentsMargins(16, 4, 16, 4);

    topMainTitle =
        makeLabel(QStringLiteral("topMainTitle"), QObject::tr("主屏幕"),
                  topStatusBar);
    topRoomChip = makeLabel(QStringLiteral("topRoomChip"),
                            QObject::tr("房间 -"), topStatusBar);
    topMemberChip = makeLabel(QStringLiteral("topMemberChip"),
                              QObject::tr("0 人"), topStatusBar);
    topSpeakerLabel =
        makeLabel(QStringLiteral("topSpeakerLabel"),
                  QObject::tr("正在讲话: -"), topStatusBar);
    topMeetStatus =
        makeLabel(QStringLiteral("topMeetStatus"), QObject::tr("未加入会议"),
                  topStatusBar);

    topStatusLayout->addWidget(topMainTitle);
    topStatusLayout->addWidget(topRoomChip);
    topStatusLayout->addWidget(topMemberChip);
    topStatusLayout->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    topStatusLayout->addWidget(topSpeakerLabel);
    topStatusLayout->addWidget(topMeetStatus);
    verticalLayout->addWidget(topStatusBar);

    // ---- 主区域：画面 + 侧栏 ----
    auto *main_box_layout = new QHBoxLayout();
    main_box_layout->setObjectName(QStringLiteral("main_box_layout"));
    main_box_layout->setSpacing(0);
    main_box_layout->setContentsMargins(0, 0, 0, 0);

    groupBox_2 = new QGroupBox(parent);
    groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
    groupBox_2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    groupBox_2->setTitle(QString());
    groupBox_2->setFlat(true);

    auto *gridLayout = new QGridLayout(groupBox_2);
    gridLayout->setObjectName(QStringLiteral("gridLayout"));
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    mainshow_label = new VideoGLWidget(groupBox_2);
    mainshow_label->setObjectName(QStringLiteral("mainshow_label"));
    mainshow_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    gridLayout->addWidget(mainshow_label, 0, 0);
    main_box_layout->addWidget(groupBox_2, 1);

    tabWidget = new QTabWidget(parent);
    tabWidget->setObjectName(QStringLiteral("tabWidget"));
    tabWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    tabWidget->setFixedWidth(300);
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setTabShape(QTabWidget::Rounded);
    tabWidget->setIconSize(QSize(16, 16));
    tabWidget->setUsesScrollButtons(true);
    tabWidget->setTabsClosable(false);

    // 成员
    auto *tab_5 = new QWidget();
    tab_5->setObjectName(QStringLiteral("tab_5"));
    tab_5->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *verticalLayout_5 = new QVBoxLayout(tab_5);
    verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
    verticalLayout_5->setSpacing(0);
    verticalLayout_5->setContentsMargins(0, 0, 0, 0);

    auto *scrollArea = new QScrollArea(tab_5);
    scrollArea->setObjectName(QStringLiteral("scrollArea"));
    scrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    scrollArea->setLineWidth(0);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea->setWidgetResizable(true);

    scrollAreaWidgetContents = new QWidget();
    scrollAreaWidgetContents->setObjectName(
        QStringLiteral("scrollAreaWidgetContents"));
    scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred,
                                            QSizePolicy::Expanding);
    verticalLayout_3 = new QVBoxLayout(scrollAreaWidgetContents);
    verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
    verticalLayout_3->setSpacing(6);
    verticalLayout_3->setContentsMargins(8, 8, 8, 8);
    scrollArea->setWidget(scrollAreaWidgetContents);
    verticalLayout_5->addWidget(scrollArea);
    tabWidget->addTab(tab_5, QObject::tr("成员"));

    // 聊天
    auto *tab_6 = new QWidget();
    tab_6->setObjectName(QStringLiteral("tab_6"));
    tab_6->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *verticalLayout_6 = new QVBoxLayout(tab_6);
    verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
    verticalLayout_6->setSpacing(2);
    verticalLayout_6->setContentsMargins(0, 0, 0, 0);

    listWidget = new QListWidget(tab_6);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listWidget->setObjectName(QStringLiteral("listWidget"));
    listWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    listWidget->setFocusPolicy(Qt::NoFocus);
    listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    listWidget->setAutoScrollMargin(0);
    verticalLayout_6->addWidget(listWidget);

    plainTextEdit = new MyTextEdit(tab_6);
    plainTextEdit->setObjectName(QStringLiteral("plainTextEdit"));
    plainTextEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    plainTextEdit->setMaximumHeight(80);
    plainTextEdit->setPlaceholderText(QStringLiteral("请输入内容..."));
    verticalLayout_6->addWidget(plainTextEdit);

    sendmsg = new QPushButton(QObject::tr("发送"), tab_6);
    sendmsg->setObjectName(QStringLiteral("sendmsg"));
    sendmsg->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    verticalLayout_6->addWidget(sendmsg);
    tabWidget->addTab(tab_6, QObject::tr("聊天"));

    // 信息
    auto *tab_meetInfo = new QWidget();
    tab_meetInfo->setObjectName(QStringLiteral("tab_meetInfo"));
    auto *verticalLayout_meetInfo = new QVBoxLayout(tab_meetInfo);
    verticalLayout_meetInfo->setObjectName(
        QStringLiteral("verticalLayout_meetInfo"));
    verticalLayout_meetInfo->setSpacing(12);
    verticalLayout_meetInfo->setContentsMargins(12, 12, 12, 12);

    auto *formLayout_meetInfo = new QFormLayout();
    formLayout_meetInfo->setObjectName(QStringLiteral("formLayout_meetInfo"));
    formLayout_meetInfo->setHorizontalSpacing(12);
    formLayout_meetInfo->setVerticalSpacing(10);

    labelMeetStatus =
        makeLabel(QStringLiteral("labelMeetStatus"), QObject::tr("未加入会议"),
                  tab_meetInfo);
    labelRoomNo =
        makeLabel(QStringLiteral("labelRoomNo"), QStringLiteral("-"),
                  tab_meetInfo);
    labelMemberCount =
        makeLabel(QStringLiteral("labelMemberCount"), QStringLiteral("0"),
                  tab_meetInfo);
    labelLocalIp =
        makeLabel(QStringLiteral("labelLocalIp"), QStringLiteral("-"),
                  tab_meetInfo);
    labelServer =
        makeLabel(QStringLiteral("labelServer"), QObject::tr("未连接"),
                  tab_meetInfo);
    labelSpeaker =
        makeLabel(QStringLiteral("labelSpeaker"), QStringLiteral("-"),
                  tab_meetInfo);

    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelMeetStatusTitle"),
                  QObject::tr("会议状态"), tab_meetInfo),
        labelMeetStatus);
    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelRoomNoTitle"), QObject::tr("房间号"),
                  tab_meetInfo),
        labelRoomNo);
    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelMemberCountTitle"), QObject::tr("人数"),
                  tab_meetInfo),
        labelMemberCount);
    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelLocalIpTitle"), QObject::tr("本机 IP"),
                  tab_meetInfo),
        labelLocalIp);
    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelServerTitle"), QObject::tr("服务器"),
                  tab_meetInfo),
        labelServer);
    formLayout_meetInfo->addRow(
        makeLabel(QStringLiteral("labelSpeakerTitle"), QObject::tr("当前说话"),
                  tab_meetInfo),
        labelSpeaker);

    verticalLayout_meetInfo->addLayout(formLayout_meetInfo);
    verticalLayout_meetInfo->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
    tabWidget->addTab(tab_meetInfo, QObject::tr("信息"));
    tabWidget->setCurrentIndex(0);

    main_box_layout->addWidget(tabWidget);
    verticalLayout->addLayout(main_box_layout, 1);

    // ---- 底栏工具条 ----
    avToolbar = new QWidget(parent);
    avToolbar->setObjectName(QStringLiteral("avToolbar"));
    avToolbar->setMinimumHeight(84);

    auto *AV_box = new QHBoxLayout(avToolbar);
    AV_box->setObjectName(QStringLiteral("AV_box"));
    AV_box->setSpacing(0);
    AV_box->setContentsMargins(16, 10, 16, 12);
    AV_box->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    controlPill = new QWidget(avToolbar);
    controlPill->setObjectName(QStringLiteral("controlPill"));
    auto *controlPillLayout = new QHBoxLayout(controlPill);
    controlPillLayout->setObjectName(QStringLiteral("controlPillLayout"));
    controlPillLayout->setSpacing(1);
    controlPillLayout->setContentsMargins(10, 8, 10, 8);

    btnSideMembers =
        makeToolButton(QStringLiteral("btnSideMembers"), QObject::tr("成员"),
                       QSize(64, 48), controlPill, QObject::tr("成员列表"));
    btnSideChat =
        makeToolButton(QStringLiteral("btnSideChat"), QObject::tr("聊天"),
                       QSize(64, 48), controlPill, QObject::tr("打开聊天"));
    btnSideInfo =
        makeToolButton(QStringLiteral("btnSideInfo"), QObject::tr("信息"),
                       QSize(64, 48), controlPill, QObject::tr("会议信息"));

    openAudio =
        makeToolButton(QStringLiteral("openAudio"), QObject::tr("开启音频"),
                       QSize(88, 48), controlPill);
    audioDeviceBtn =
        makeToolButton(QStringLiteral("audioDeviceBtn"), QStringLiteral("▲"),
                       QSize(28, 48), controlPill, QObject::tr("选择音频设备"));
    audioDeviceBtn->setMaximumWidth(28);

    openVedio =
        makeToolButton(QStringLiteral("openVedio"), QObject::tr("开启摄像头"),
                       QSize(88, 48), controlPill);
    videoDeviceBtn =
        makeToolButton(QStringLiteral("videoDeviceBtn"), QStringLiteral("▲"),
                       QSize(28, 48), controlPill, QObject::tr("选择摄像头"));
    videoDeviceBtn->setMaximumWidth(28);

    btnTogglePanel =
        makeToolButton(QStringLiteral("btnTogglePanel"), QObject::tr("侧栏"),
                       QSize(64, 48), controlPill,
                       QObject::tr("显示/隐藏侧栏"));
    btnTogglePanel->setCheckable(true);
    btnTogglePanel->setChecked(true);

    leaveMeetingBtn =
        makeToolButton(QStringLiteral("leaveMeetingBtn"),
                       QObject::tr("结束会议"), QSize(88, 48), controlPill);

    controlPillLayout->addWidget(btnSideMembers);
    controlPillLayout->addWidget(btnSideChat);
    controlPillLayout->addWidget(btnSideInfo);
    controlPillLayout->addWidget(makePillSep(QStringLiteral("pillSep1"),
                                             controlPill));
    controlPillLayout->addWidget(openAudio);
    controlPillLayout->addWidget(audioDeviceBtn);
    controlPillLayout->addWidget(openVedio);
    controlPillLayout->addWidget(videoDeviceBtn);
    controlPillLayout->addWidget(makePillSep(QStringLiteral("pillSep2"),
                                             controlPill));
    controlPillLayout->addWidget(btnTogglePanel);
    controlPillLayout->addWidget(leaveMeetingBtn);

    AV_box->addWidget(controlPill);
    AV_box->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
    verticalLayout->addWidget(avToolbar);
}
