#include "ui/pages/meeting_window_ui.h"

#include "config/global_config.h"
#include "ui/foundation/icon_provider.h"
#include "ui/foundation/theme_manager.h"

#include "ui/composites/my_text_edit.h"
#include "media/video/video_gl_widget.h"

#include <QAbstractScrollArea>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QScrollArea>
#include <QScrollBar>
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

QToolButton *makeToolButton(const QString &objectName, const QString &text,
                            QWidget *parent,
                            const QString &toolTip = QString()) {
    auto *btn = new QToolButton(parent);
    btn->setObjectName(objectName);
    btn->setText(text);
    // 图标在上、文字在下，与主流会议软件一致
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    // 与背景融为一体：默认无边框、无底色，悬停才出现淡底色
    btn->setAutoRaise(true);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumSize(config::ui::meeting::tool_button_min_width,
                        config::ui::meeting::tool_button_height);
    btn->setIconSize(QSize(config::ui::meeting::tool_button_icon_size,
                           config::ui::meeting::tool_button_icon_size));
    if (!toolTip.isEmpty()) {
        btn->setToolTip(toolTip);
    }

    // 图标：按 objectName 映射到 SVG 名字（中性色，不使用强调色）
    static const QHash<QString, QString> kIconByName = {
        {QStringLiteral("btnSideMembers"), QStringLiteral("users")},
        {QStringLiteral("btnSideChat"), QStringLiteral("chat")},
        {QStringLiteral("openAudio"), QStringLiteral("mic")},
        {QStringLiteral("openVedio"), QStringLiteral("camera")},
        {QStringLiteral("shareScreenBtn"), QStringLiteral("screen-share")},
        {QStringLiteral("btnTogglePanel"), QStringLiteral("panel")},
        {QStringLiteral("leaveMeetingBtn"), QStringLiteral("hangup")},
        {QStringLiteral("audioDeviceBtn"), QStringLiteral("chevron-up")},
        {QStringLiteral("videoDeviceBtn"), QStringLiteral("chevron-up")},
    };
    if (const auto it = kIconByName.constFind(objectName); it != kIconByName.constEnd()) {
        const bool is_device = objectName == QStringLiteral("audioDeviceBtn") ||
                               objectName == QStringLiteral("videoDeviceBtn");
        // 设备下拉箭头小一号；其余图标统一用中性色
        // （结束会议与麦克风/摄像头一致：图标本体不染色，红色只留给文字/斜杠）
        const int icon_size = is_device
                                  ? config::ui::meeting::device_button_icon_size
                                  : config::ui::meeting::tool_button_icon_size;
        // 结束会议是危险操作：图标用 danger 色（与 QSS 的红色文字呼应）
        const bool is_danger = objectName == QStringLiteral("leaveMeetingBtn");
        const QColor icon_color = ui::ThemeManager::instance().color(
            is_danger ? QStringLiteral("danger") : QStringLiteral("icon"));
        btn->setIcon(ui::IconProvider::instance().icon(it.value(), icon_color, icon_size));
        btn->setIconSize(QSize(icon_size, icon_size));
        if (is_device) {
            // 设备箭头：宽度保持窄条，高度与其它工具按钮一致
            btn->setToolButtonStyle(Qt::ToolButtonIconOnly);
            btn->setFixedWidth(config::ui::meeting::device_button_width);
            btn->setFixedHeight(config::ui::meeting::tool_button_height);
        } else {
            // 所有工具按钮统一宽度：文字长短不同也不该宽窄不一
            btn->setFixedWidth(objectName == QStringLiteral("leaveMeetingBtn")
                                   ? config::ui::meeting::tool_button_leave_width
                                   : config::ui::meeting::tool_button_width);
            btn->setFixedHeight(config::ui::meeting::tool_button_height);
        }
    }

    return btn;
}
QFrame *makePillSep(const QString &objectName, QWidget *parent) {
    auto *sep = new QFrame(parent);
    sep->setObjectName(objectName);
    sep->setFixedSize(config::ui::meeting::pill_sep_width,
                      config::ui::meeting::pill_sep_height);
    sep->setFrameShape(QFrame::NoFrame);
    return sep;
}

/** AsNeeded 显示滚动条，不需要时用 viewport 右边距占位，避免内容区宽度跳动 */
void setupStableVerticalScrollBar(QAbstractScrollArea *area) {
    if (!area)
        return;
    area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    auto *sb = area->verticalScrollBar();
    // 窄滚动条：固定宽度，预留与显示一致，避免系统默认 ~16px 过宽
    constexpr int kScrollBarWidth = config::ui::meeting::side_scrollbar_width;
    sb->setFixedWidth(kScrollBarWidth);

    // setViewportMargins 为 protected，经派生类访问
    struct MarginAccess : QAbstractScrollArea {
        using QAbstractScrollArea::setViewportMargins;
    };
    const auto sync = [area, sb]() {
        static_cast<MarginAccess *>(area)->setViewportMargins(
            0, 0, sb->maximum() > 0 ? 0 : kScrollBarWidth, 0);
    };
    QObject::connect(sb, &QScrollBar::rangeChanged, area,
                     [sync](int, int) { sync(); });
    sync();
}

}  // namespace

void ui::MeetingWindowUi::setupUi(QWidget *parent) {
    parent->setObjectName(QStringLiteral("Widget"));
    parent->resize(config::ui::meeting::default_width, config::ui::meeting::default_height);
    parent->setWindowTitle(QObject::tr("云会议"));

    verticalLayout = new QVBoxLayout(parent);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    verticalLayout->setSpacing(0);// 控件之间间距为0
    verticalLayout->setContentsMargins(0, 0, 0, 0); //布局四周内边距为0

    // ---- 顶栏：与最小化/最大化/关闭同一行（由 MeetingWindow 摆进标题栏区域） ----
    topStatusBar = new QWidget(parent);
    topStatusBar->setObjectName(QStringLiteral("topStatusBar"));
    topStatusBar->setMinimumHeight(config::window::meeting_title_bar_height);
    topStatusBar->setMaximumHeight(config::window::meeting_title_bar_height);

    auto *topStatusLayout = new QHBoxLayout(topStatusBar);
    topStatusLayout->setObjectName(QStringLiteral("topStatusLayout"));
    topStatusLayout->setSpacing(config::ui::meeting::top_status_spacing);
    // 垂直方向不留边：26px 的按钮在 42px 的标题栏里自然居中
    topStatusLayout->setContentsMargins(config::ui::meeting::top_status_margin_h, 0,
                                        config::ui::meeting::top_status_margin_h, 0);


    // 会议时长：每秒走动（未入会显示占位）
    topDurationChip = makeLabel(QStringLiteral("topDurationChip"),
                                QObject::tr("--:--:--"), topStatusBar);
    topDurationChip->setToolTip(QObject::tr("本次会议已进行时长"));

    // 顶栏右侧：会议设置入口（点开后是锚定在按钮下方的弹出面板）
    btnMeetingSettings = new QPushButton(QObject::tr("会议设置"), topStatusBar);
    btnMeetingSettings->setObjectName(QStringLiteral("btnMeetingSettings"));
    btnMeetingSettings->setCursor(Qt::PointingHandCursor);
    btnMeetingSettings->setToolTip(QObject::tr("房间权限与会议快捷操作"));
    btnMeetingSettings->setFixedHeight(config::ui::meeting::top_button_height);
    btnMeetingSettings->setIcon(ui::IconProvider::instance().icon(
        QStringLiteral("settings"),
        ui::ThemeManager::instance().color(QStringLiteral("textMuted")), 12));

    // 左上角：会议详情（点开显示房间号/发起人/我的名称/时长）
    btnMeetingInfo = new QPushButton(QObject::tr("会议详情"), topStatusBar);
    btnMeetingInfo->setObjectName(QStringLiteral("btnMeetingInfo"));
    btnMeetingInfo->setCursor(Qt::PointingHandCursor);
    btnMeetingInfo->setToolTip(QObject::tr("查看会议主题、房间号、发起人与时长"));
    btnMeetingInfo->setFixedHeight(config::ui::meeting::top_button_height);
    btnMeetingInfo->setIcon(ui::IconProvider::instance().icon(
        QStringLiteral("info"),
        ui::ThemeManager::instance().color(QStringLiteral("textMuted")), 14));
    topStatusLayout->addWidget(btnMeetingInfo);

    topStatusLayout->addWidget(topDurationChip);
    topStatusLayout->addItem(
        new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

    topStatusLayout->addWidget(btnMeetingSettings);
    // 注意：顶栏不再加入 verticalLayout —— 它作为浮层摆在标题栏区域，
    // 位置由 MeetingWindow::update_title_overlay_geometry() 计算。

    // ---- 主区域：画面 + 侧栏 ----
    auto *main_box_layout = new QHBoxLayout();
    main_box_layout->setObjectName(QStringLiteral("main_box_layout"));
    // 「浮动卡片」：画面卡片与右侧面板之间留空隙
    main_box_layout->setSpacing(config::ui::meeting::panel_gap);
    main_box_layout->setContentsMargins(0, 0, 0, 0);

    groupBox_2 = new QWidget(parent);
    groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
    groupBox_2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 画面在卡片内四周留白，圆角才不会被画面方角顶穿
    groupBox_2->setContentsMargins(config::ui::meeting::video_host_margin_h,
                                   config::ui::meeting::video_host_margin_h,
                                   config::ui::meeting::video_host_margin_h,
                                   config::ui::meeting::video_host_margin_h);

    auto *gridLayout = new QGridLayout(groupBox_2);
    gridLayout->setObjectName(QStringLiteral("gridLayout"));
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    mainshow_label = new VideoGLWidget(groupBox_2);
    mainshow_label->setObjectName(QStringLiteral("mainshow_label"));
    mainshow_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    mainshow_label->setCornerRadius(config::ui::meeting::video_corner_radius);
    gridLayout->addWidget(mainshow_label, 0, 0);

    // 未入会时的画布占位提示（与画面同格叠放，入会后隐藏）
    canvasHint = new QLabel(groupBox_2);
    canvasHint->setObjectName(QStringLiteral("canvasHint"));
    canvasHint->setAlignment(Qt::AlignCenter);
    canvasHint->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // 颜色走主题令牌，避免浅色主题下仍是深色文字
    canvasHint->setText(
        QStringLiteral("<div style='font-size:%1px;color:%2;'>%3</div>"
                       "<div style='font-size:%4px;color:%5;"
                       "margin-top:%6px;'>%7</div>")
            .arg(config::ui::meeting::canvas_hint_title_font)
            .arg(ui::ThemeManager::instance()
                     .color(QStringLiteral("textWeak"))
                     .name())
            .arg(QObject::tr("未加入会议"))
            .arg(config::ui::meeting::canvas_hint_desc_font)
            .arg(ui::ThemeManager::instance()
                     .color(QStringLiteral("textDisabled"))
                     .name())
            .arg(config::ui::meeting::canvas_hint_desc_margin)
            .arg(QObject::tr("创建或加入会议后，这里显示画面")));
    gridLayout->addWidget(canvasHint, 0, 0, Qt::AlignCenter);

    main_box_layout->addWidget(groupBox_2, 1);

    tabWidget = new QTabWidget(parent);
    tabWidget->setObjectName(QStringLiteral("tabWidget"));
    tabWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    tabWidget->setFixedWidth(config::ui::meeting::side_panel_width);
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setTabShape(QTabWidget::Rounded);
    tabWidget->setIconSize(QSize(config::ui::meeting::tab_icon_size, config::ui::meeting::tab_icon_size));
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
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setLineWidth(0);
    scrollArea->setWidgetResizable(true);
    setupStableVerticalScrollBar(scrollArea);

    auto *scrollAreaWidgetContents = new QWidget();
    scrollAreaWidgetContents->setObjectName(
        QStringLiteral("scrollAreaWidgetContents"));
    scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred,
                                            QSizePolicy::Expanding);
    verticalLayout_3 = new QVBoxLayout(scrollAreaWidgetContents);
    verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
    verticalLayout_3->setSpacing(config::ui::meeting::side_scroll_spacing);
    verticalLayout_3->setContentsMargins(config::ui::meeting::side_scroll_margin, config::ui::meeting::side_scroll_margin, config::ui::meeting::side_scroll_margin, config::ui::meeting::side_scroll_margin);
    verticalLayout_3->setAlignment(Qt::AlignTop);
    scrollArea->setWidget(scrollAreaWidgetContents);
    verticalLayout_5->addWidget(scrollArea);
    tabWidget->addTab(tab_5, QObject::tr("成员"));

    // 聊天
    auto *tab_6 = new QWidget();
    tab_6->setObjectName(QStringLiteral("tab_6"));
    tab_6->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *verticalLayout_6 = new QVBoxLayout(tab_6);
    verticalLayout_6->setObjectName(QStringLiteral("verticalLayout_6"));
    verticalLayout_6->setSpacing(config::ui::meeting::chat_list_spacing);
    verticalLayout_6->setContentsMargins(0, 0, 0, 0);

    listWidget = new QListWidget(tab_6);
    listWidget->setFrameShape(QFrame::NoFrame);
    listWidget->setFrameShadow(QFrame::Plain);
    listWidget->setLineWidth(0);
    listWidget->setMidLineWidth(0);
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listWidget->setObjectName(QStringLiteral("listWidget"));
    listWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    listWidget->setFocusPolicy(Qt::NoFocus);
    listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listWidget->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    listWidget->setAutoScrollMargin(0);
    // 背景交给 QSS 的 @inset（配合主题切换自动重绘）；这里只关闭自动填充
    listWidget->viewport()->setAutoFillBackground(false);
    setupStableVerticalScrollBar(listWidget);
    verticalLayout_6->addWidget(listWidget);

    plainTextEdit = new MyTextEdit(tab_6);
    plainTextEdit->setObjectName(QStringLiteral("plainTextEdit"));
    plainTextEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    plainTextEdit->setMaximumHeight(config::ui::meeting::chat_input_max_height);
    plainTextEdit->setPlaceholderText(QStringLiteral("请输入内容..."));
    verticalLayout_6->addWidget(plainTextEdit);

    sendmsg = new QPushButton(QObject::tr("发送"), tab_6);
    sendmsg->setObjectName(QStringLiteral("sendmsg"));
    sendmsg->setIcon(ui::IconProvider::instance().icon(
        QStringLiteral("send"),
        ui::ThemeManager::instance().color(QStringLiteral("onPrimary")),
        config::ui::meeting::send_button_icon_size));
    sendmsg->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    verticalLayout_6->addWidget(sendmsg);
    tabWidget->addTab(tab_6, QObject::tr("聊天"));

    tabWidget->setCurrentIndex(0);

    main_box_layout->addWidget(tabWidget);
    verticalLayout->addLayout(main_box_layout, 1);

    // ---- 底栏工具条 ----
    // 高度 = 按钮 44 + 上下各 6 的内边距；比原来（约 63）略紧凑，
    // 按钮在条内垂直居中，不再靠不对称 margin 撑高。
    avToolbar = new QWidget(parent);
    avToolbar->setObjectName(QStringLiteral("avToolbar"));
    avToolbar->setFixedHeight(config::ui::meeting::toolbar_height);

    auto *AV_box = new QHBoxLayout(avToolbar);
    AV_box->setObjectName(QStringLiteral("AV_box"));
    AV_box->setSpacing(0);
    AV_box->setContentsMargins(config::ui::meeting::toolbar_margin_h, 0, config::ui::meeting::toolbar_margin_h, 0);
    AV_box->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));

    controlPill = new QWidget(avToolbar);
    controlPill->setObjectName(QStringLiteral("controlPill"));
    auto *controlPillLayout = new QHBoxLayout(controlPill);
    controlPillLayout->setObjectName(QStringLiteral("controlPillLayout"));
    controlPillLayout->setSpacing(config::ui::meeting::pill_spacing);
    controlPillLayout->setContentsMargins(config::ui::meeting::pill_margin_h, config::ui::meeting::pill_margin_v, config::ui::meeting::pill_margin_h, config::ui::meeting::pill_margin_v);

    btnSideMembers =
        makeToolButton(QStringLiteral("btnSideMembers"), QObject::tr("成员"),
                       controlPill, QObject::tr("成员列表"));
    btnSideChat =
        makeToolButton(QStringLiteral("btnSideChat"), QObject::tr("聊天"),
                       controlPill, QObject::tr("打开聊天"));
    openAudio =
        makeToolButton(QStringLiteral("openAudio"), QObject::tr("开启音频"),
                       controlPill);
    audioDeviceBtn =
        makeToolButton(QStringLiteral("audioDeviceBtn"), QStringLiteral("▲"),
                       controlPill, QObject::tr("选择音频设备"));
    audioDeviceBtn->setMaximumWidth(config::ui::meeting::device_button_max_width);

    openVedio =
        makeToolButton(QStringLiteral("openVedio"), QObject::tr("开启摄像头"),
                       controlPill);
    videoDeviceBtn =
        makeToolButton(QStringLiteral("videoDeviceBtn"), QStringLiteral("▲"),
                       controlPill, QObject::tr("选择摄像头"));
    videoDeviceBtn->setMaximumWidth(config::ui::meeting::device_button_max_width);

    shareScreenBtn =
        makeToolButton(QStringLiteral("shareScreenBtn"), QObject::tr("共享屏幕"),
                       controlPill,
                       QObject::tr("共享屏幕（与摄像头互斥，共用同一路视频）"));

    btnTogglePanel =
        makeToolButton(QStringLiteral("btnTogglePanel"), QObject::tr("侧栏"),
                       controlPill,
                       QObject::tr("显示/隐藏侧栏"));
    btnTogglePanel->setCheckable(true);
    btnTogglePanel->setChecked(true);

    leaveMeetingBtn =
        makeToolButton(QStringLiteral("leaveMeetingBtn"),
                       QObject::tr("结束会议"), controlPill);

    controlPillLayout->addWidget(btnSideMembers);
    controlPillLayout->addWidget(btnSideChat);
    controlPillLayout->addWidget(makePillSep(QStringLiteral("pillSep1"),
                                             controlPill));
    controlPillLayout->addWidget(openAudio);
    controlPillLayout->addWidget(audioDeviceBtn);
    controlPillLayout->addWidget(openVedio);
    controlPillLayout->addWidget(videoDeviceBtn);
    controlPillLayout->addWidget(shareScreenBtn);
    controlPillLayout->addWidget(makePillSep(QStringLiteral("pillSep2"),
                                             controlPill));
    controlPillLayout->addWidget(btnTogglePanel);
    controlPillLayout->addWidget(leaveMeetingBtn);

    AV_box->addWidget(controlPill);
    AV_box->addItem(
        new QSpacerItem(20, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
    verticalLayout->addWidget(avToolbar);
}
