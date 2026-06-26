#ifndef CHATMESSAGE_H
#define CHATMESSAGE_H

#include <QWidget>
#include <QLabel>

class QMovie;

/// 聊天列表中单条消息视图：自绘头像、三角、气泡、正文与时间/IP；配合 QListWidgetItem 的 sizeHint 使用。
class ChatMessage : public QWidget
{
    Q_OBJECT
public:
    explicit ChatMessage(QWidget *parent = nullptr);

    /// 消息类型：系统通知 / 本人 / 对方 / 仅时间分隔行
    enum User_Type{
        User_System,//系统
        User_Me,    //自己
        User_She,   //用户
        User_Time,  //时间
    };
    /// 标记本人消息已发送成功，隐藏「发送中」动画
    void setTextSuccess();
    /// 设置文案、时间戳、列表项整体尺寸、可选 IP 与角色；会触发重绘并在本人未成功时显示加载动画
    void setText(QString text, QString time, QSize allSize, QString ip = "",  User_Type userType = User_Time);

    /// 按当前字体与折行宽度计算文本排版后的逻辑宽高（供 fontRect 使用）
    QSize getRealString(QString src);
    /// 根据文本更新各类 QRect，并返回供 QListWidgetItem::setSizeHint 使用的建议尺寸
    QSize fontRect(QString str);
    /// splitter/窗口尺寸变化后，按新宽度重新排版并返回行高
    QSize relayoutForWidth(int width);

    inline QString text() {return m_msg;}
    inline QString time() {return m_time;}
    inline User_Type userType() {return m_userType;}
protected:
    void paintEvent(QPaintEvent *event);
private:
    QString m_msg;       ///< 消息正文
    QString m_time;      ///< 原始时间字段（多为秒级时间戳字符串）
    QString m_curTime;   ///< 格式化后的显示时间，如 "ddd hh:mm"
    QString m_ip;        ///< 可选，用于在气泡旁显示 IP

    QSize m_allSize;     ///< 所属列表/容器尺寸，用于布局参考
    User_Type m_userType = User_System;

    int m_kuangWidth;    ///< 气泡框总宽度
    int m_textWidth;     ///< 文本区域宽度（已扣除内边距）
    int m_spaceWid;      ///< 控件宽度减去文本宽度，左右留白相关
    int m_lineHeight;    ///< 当前字体行高（由 QFontMetrics 得到）

    QRect m_ipLeftRect;      ///< 左侧 IP 文字区域
    QRect m_ipRightRect;     ///< 右侧 IP 文字区域
    QRect m_iconLeftRect;    ///< 左侧头像
    QRect m_iconRightRect;   ///< 右侧头像
    QRect m_sanjiaoLeftRect; ///< 左侧气泡小三角
    QRect m_sanjiaoRightRect;///< 右侧气泡小三角
    QRect m_kuangLeftRect;   ///< 左侧气泡矩形
    QRect m_kuangRightRect;  ///< 右侧气泡矩形
    QRect m_textLeftRect;    ///< 左侧气泡内文本绘制区
    QRect m_textRightRect;   ///< 右侧气泡内文本绘制区
    QPixmap m_leftPixmap;    ///< 左侧头像图
    QPixmap m_rightPixmap;   ///< 右侧头像图
    QLabel* m_loading = Q_NULLPTR;   ///< 「发送中」GIF 容器
    QMovie* m_loadingMovie = Q_NULLPTR;
    bool m_isSending = false; ///< 本人消息是否已标记发送成功（与 setTextSuccess 配合）
};

#endif // CHATMESSAGE_H
