#include "text/chatmessage.h"
#include "configure/configure.h"
#include "logger/Logger.h"
#include <QString>
#include <cmath>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QDateTime>
#include <QPainter>
#include <QMovie>
#include <QLabel>

ChatMessage::ChatMessage(QWidget *parent) : QWidget(parent)
{
    QFont te_font = this->font(); //取出当前控件的字体
    te_font.setFamily("MicrosoftYaHei"); //使用微软雅黑
    te_font.setPointSize(12); //设置字体大小
    this->setFont(te_font);
    //加载头像图片
    m_leftPixmap = QPixmap(QString::fromUtf8(Source::default_avatar));
    m_rightPixmap = QPixmap(QString::fromUtf8(Source::default_avatar));

    //加载加载动画
    m_loadingMovie = new QMovie(this);
    m_loadingMovie->setFileName(QString::fromUtf8(Source::wait_gif));
    m_loading = new QLabel(this);
    m_loading->setMovie(m_loadingMovie); //设置动画
    //setScaledContents: 用于控制标签内的图片是否自动缩放以适应标签大小
    m_loading->setScaledContents(true); //缩放
    //设置控件大小
    m_loading->resize(40,40); //设置大小
    //setAttribute: 用于设置控件的属性
    //Qt::WA_TranslucentBackground: 用于设置控件的背景是否透明
    m_loading->setAttribute(Qt::WA_TranslucentBackground , true); //设置透明背景
}

void ChatMessage::setTextSuccess()
{
    m_loading->hide();
    m_loadingMovie->stop();
    m_isSending = true;
}

void ChatMessage::setText(QString text, QString time, QSize allSize, QString ip,ChatMessage::User_Type userType)
{
    m_msg = text;
    m_userType = userType;
    m_time = time;
    m_curTime = QDateTime::fromSecsSinceEpoch(time.toInt()).toString("ddd hh:mm");
    m_allSize = allSize;
    m_ip = ip;
    if(userType == User_Me) {
        //如果是用户自己发送就显示发送中的加载动画
        if(!m_isSending) {
            //设置加载正在发送动画的位置
            m_loading->move(
                m_kuangRightRect.x() - m_loading->width() - 10, 
                m_kuangRightRect.y()+m_kuangRightRect.height()/2- m_loading->height()/2
            );
//            m_loading->move(0, 0);
            m_loading->show();
            m_loadingMovie->start();
        }
    } else {
        m_loading->hide(); //如果是其他用户发送就隐藏加载动画
    }

    this->update(); //更新界面
}

QSize ChatMessage::fontRect(QString str)
{
    m_msg = str;
    int minHei = 30; //最小高度
    int iconWH = 40; //头像宽度
    int iconSpaceW = 20; //头像与边缘的间距
    int iconRectW = 5; //头像边框宽度
    int iconTMPH = 10; //头像顶部间距
    int sanJiaoW = 6; //三角形宽度
    int kuangTMP = 20; //框顶部间距
    int textSpaceRect = 12; //文本与气泡框内边距
    //气泡框宽度 = 控件总宽度 - 额外宽度 - 2 × (头像宽 + 头像与边缘的间距 + 头像边框宽度),左右两边都有头像区域
    m_kuangWidth = this->width() - kuangTMP - 2*(iconWH+iconSpaceW+iconRectW);
    //文本宽度 = 气泡框宽度 - 2 × (文本与气泡框内边距)
    m_textWidth = m_kuangWidth - 2*textSpaceRect; //文本宽度
    //文本之外的空白区域宽度
    m_spaceWid = this->width() - m_textWidth; //文本之外的空白区域宽度
    m_iconLeftRect = QRect(iconSpaceW, iconTMPH + 10, iconWH, iconWH); //左头像矩形
    m_iconRightRect = QRect(this->width() - iconSpaceW - iconWH, iconTMPH + 10, iconWH, iconWH); //右头像矩形


    //根据文本内容和字体计算实际宽高
    QSize size = getRealString(m_msg); // 整个的size

    LOG_DEBUG("ChatMessage", "fontRect size " << size.width() << "x" << size.height());
    int hei = size.height() < minHei ? minHei : size.height(); //如果文本高度小于最小高度,则高度为最小高度,否则为文本高度

    m_sanjiaoLeftRect = QRect(
        iconWH+iconSpaceW+iconRectW, //头像右侧边缘
        m_lineHeight/2 + 10, //垂直居中
        sanJiaoW, //宽度
        hei - m_lineHeight //高度
    );
    m_sanjiaoRightRect = QRect(
        this->width() - iconRectW - iconWH - iconSpaceW - sanJiaoW, 
        m_lineHeight/2+10, //垂直居中
        sanJiaoW, //宽度
        hei - m_lineHeight //高度
    );

    if(size.width() < (m_textWidth+m_spaceWid)) {
        //如果文本宽度小于气泡框宽度,则气泡框宽度为文本宽度
        m_kuangLeftRect.setRect(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), m_lineHeight/4*3 + 10, size.width()-m_spaceWid+2*textSpaceRect, hei-m_lineHeight);
        m_kuangRightRect.setRect(this->width() - size.width() + m_spaceWid - 2*textSpaceRect - iconWH - iconSpaceW - iconRectW - sanJiaoW,
                                 m_lineHeight/4*3 + 10, size.width()-m_spaceWid+2*textSpaceRect, hei-m_lineHeight);
    } else { //如果文本宽度大于气泡框宽度,则气泡框宽度为气泡框宽度
        m_kuangLeftRect.setRect(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), m_lineHeight/4*3 + 10, m_kuangWidth, hei-m_lineHeight);
        m_kuangRightRect.setRect(iconWH + kuangTMP + iconSpaceW + iconRectW - sanJiaoW, m_lineHeight/4*3 + 10, m_kuangWidth, hei-m_lineHeight);
    }

    // 气泡内实际绘制正文的区域（与 paintEvent 中 drawText 一致；此前未赋值则为空矩形，文字无法显示）
    m_textLeftRect.setRect(
        m_kuangLeftRect.x() + textSpaceRect,
        m_kuangLeftRect.y() + textSpaceRect,
        m_kuangLeftRect.width() - 2 * textSpaceRect,
        m_kuangLeftRect.height() - 2 * textSpaceRect);
    m_textRightRect.setRect(
        m_kuangRightRect.x() + textSpaceRect,
        m_kuangRightRect.y() + textSpaceRect,
        m_kuangRightRect.width() - 2 * textSpaceRect,
        m_kuangRightRect.height() - 2 * textSpaceRect);

    //ip矩形
    m_ipLeftRect.setRect(m_kuangLeftRect.x(), m_kuangLeftRect.y()+iconTMPH - 20,
                           m_kuangLeftRect.width()-2*textSpaceRect + iconWH*2, 20);
    m_ipRightRect.setRect(m_kuangRightRect.x(), m_kuangRightRect.y()+iconTMPH - 30,
                            m_kuangRightRect.width()-2*textSpaceRect + iconWH*2 , 20);
    return QSize(size.width(), hei + 15); //返回文本宽度,高度
}

QSize ChatMessage::getRealString(QString src)
{
    QFontMetricsF fm(this->font());
    m_lineHeight = fm.lineSpacing();

    // 使用与 paintEvent::drawText 一致的 WordWrap 测量尺寸。旧实现误把像素宽度当「字符数」、且
    // mid(pos, len) 第二个参数写成了 (i+1)*size，导致折行高度偏小，正文被裁切（空格后整段不见等）。
    if (m_textWidth <= 0) {
        const qreal w = fm.horizontalAdvance(src);
        const qreal h = fm.boundingRect(src).height();
        return QSize(static_cast<int>(std::ceil(w + m_spaceWid)),
                     static_cast<int>(std::ceil(h + 2 * m_lineHeight)));
    }

    const QRectF textRect(0, 0, static_cast<qreal>(m_textWidth), 1000000.0);
    const QRectF br = fm.boundingRect(textRect,
                                      Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                      src);
    const int tw = qMin(m_textWidth, static_cast<int>(std::ceil(br.width())));
    const int th = static_cast<int>(std::ceil(br.height()));
    const int totalH = th + static_cast<int>(2 * m_lineHeight);
    return QSize(tw + m_spaceWid, totalH);
}

void ChatMessage::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);//消锯齿
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::gray));

    if(m_userType == User_Type::User_She) { // 用户
        //头像
//        painter.drawRoundedRect(m_iconLeftRect,m_iconLeftRect.width(),m_iconLeftRect.height());
        painter.drawPixmap(m_iconLeftRect, m_leftPixmap);

        //框加边
        QColor col_KuangB(234, 234, 234);
        painter.setBrush(QBrush(col_KuangB));
        painter.drawRoundedRect(m_kuangLeftRect.x()-1,m_kuangLeftRect.y()-1 + 10,m_kuangLeftRect.width()+2,m_kuangLeftRect.height()+2,4,4);
        //框
        QColor col_Kuang(255,255,255);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangLeftRect,4,4);

        //三角
        QPointF points[3] = {
            QPointF(m_sanjiaoLeftRect.x(), 40),
            QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 35),
            QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 45),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);

        //三角加边
        QPen penSanJiaoBian;
        penSanJiaoBian.setColor(col_KuangB);
        painter.setPen(penSanJiaoBian);
        painter.drawLine(QPointF(m_sanjiaoLeftRect.x() - 1, 30), QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 24));
        painter.drawLine(QPointF(m_sanjiaoLeftRect.x() - 1, 30), QPointF(m_sanjiaoLeftRect.x()+m_sanjiaoLeftRect.width(), 36));

        //ip
        //ip
        QPen penIp;
        penIp.setColor(Qt::darkGray);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(10);
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipLeftRect, m_ip, op);

        //内容
        QPen penText;
        penText.setColor(QColor(51,51,51));
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(m_textLeftRect, m_msg,option);
    }  else if(m_userType == User_Type::User_Me) { // 自己
        //头像
//        painter.drawRoundedRect(m_iconRightRect,m_iconRightRect.width(),m_iconRightRect.height());
        painter.drawPixmap(m_iconRightRect, m_rightPixmap);

        //框
        QColor col_Kuang(75,164,242);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangRightRect,4,4);

        //三角
        QPointF points[3] = {
            QPointF(m_sanjiaoRightRect.x()+m_sanjiaoRightRect.width(), 40),
            QPointF(m_sanjiaoRightRect.x(), 35),
            QPointF(m_sanjiaoRightRect.x(), 45),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);


        //ip
        QPen penIp;
        penIp.setColor(Qt::black);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(10);
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipRightRect, m_ip, op);

        //内容
        QPen penText;
        penText.setColor(Qt::white);
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(m_textRightRect,m_msg,option);
    }  else if(m_userType == User_Type::User_Time) { // 时间
        QPen penText;
        penText.setColor(QColor(153,153,153));
        painter.setPen(penText);
        QTextOption option(Qt::AlignCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        QFont te_font = this->font();
        te_font.setFamily("MicrosoftYaHei");
        te_font.setPointSize(10);
        painter.setFont(te_font);
        painter.drawText(this->rect(),m_curTime,option);
    }
};
