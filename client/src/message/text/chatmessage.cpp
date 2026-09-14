#include "chatmessage.h"
#include "avatar_image_loader.h"
#include "configure/configure.h"
#include <QDateTime>
#include <QFontMetrics>
#include <QLabel>
#include <QMovie>
#include <QPaintEvent>
#include <QPainter>
#include <QString>
#include <cmath>
#include <spdlog/spdlog.h>

ChatMessage::ChatMessage(QWidget *parent) : QWidget(parent) {
    QFont te_font = this->font();
    te_font.setFamily("MicrosoftYaHei");
    te_font.setPointSize(m_metrics.s(m_metrics.fontPointSize));
    this->setFont(te_font);

    m_loadingMovie = new QMovie(this);
    m_loadingMovie->setFileName(QString::fromUtf8(Source::wait_gif));
    m_loading = new QLabel(this);
    m_loading->setMovie(m_loadingMovie);
    m_loading->setScaledContents(true);
    const int loading = m_metrics.s(m_metrics.loading);
    m_loading->resize(loading, loading);
    m_loadingMovie->setScaledSize(QSize(loading, loading));
    m_loading->setAttribute(Qt::WA_TranslucentBackground, true);
}

void ChatMessage::setTextSuccess() {
    m_loading->hide();
    m_loadingMovie->stop();
    m_isSending = true;
}

QSize ChatMessage::relayoutForWidth(int width) {
    if (width <= 0)
        width = this->width();
    if (width <= 0)
        return sizeHint();

    setFixedWidth(width);
    if (m_userType == User_Time) {
        const QSize size(width, m_metrics.s(m_metrics.timeRowHeight));
        resize(size);
        m_allSize = size;
        m_curTime =
            QDateTime::fromSecsSinceEpoch(m_time.toInt()).toString("ddd hh:mm");
        update();
        return size;
    }

    const QSize size = fontRect(m_msg);
    m_allSize = size;
    if (m_userType == User_Me && !m_isSending && m_loading &&
        m_loading->isVisible()) {
        m_loading->move(
            m_kuangRightRect.x() - m_loading->width() -
                m_metrics.s(m_metrics.loadingGap),
            m_kuangRightRect.y() + m_kuangRightRect.height() / 2 -
                m_loading->height() / 2);
    }
    update();
    return size;
}

void ChatMessage::setText(QString text, QString time, QSize allSize, QString ip,
                          ChatMessage::User_Type userType,
                          const QString &avatarUrl) {
    m_msg = text;
    m_userType = userType;
    m_time = time;
    m_curTime =
        QDateTime::fromSecsSinceEpoch(time.toInt()).toString("ddd hh:mm");
    m_allSize = allSize;
    m_ip = ip;
    if (userType == User_Me || userType == User_She) {
        loadAvatar(avatarUrl, userType);
    }
    if (userType == User_Me) {
        if (!m_isSending) {
            m_loading->move(
                m_kuangRightRect.x() - m_loading->width() -
                    m_metrics.s(m_metrics.loadingGap),
                m_kuangRightRect.y() + m_kuangRightRect.height() / 2 -
                    m_loading->height() / 2);
            m_loading->show();
            m_loadingMovie->start();
        }
    } else {
        m_loading->hide();
    }

    this->update();
}

void ChatMessage::loadAvatar(const QString &avatarUrl, User_Type userType) {
    const quint64 generation = ++m_avatarLoadGen;
    const int iconWH = m_metrics.s(m_metrics.icon);
    AvatarImageLoader::instance().load(
        avatarUrl, QSize(iconWH, iconWH), this,
        [this, userType, generation](const QPixmap &pixmap) {
            if (generation != m_avatarLoadGen || pixmap.isNull()) {
                return;
            }
            if (userType == User_Me) {
                m_rightPixmap = pixmap;
            } else if (userType == User_She) {
                m_leftPixmap = pixmap;
            }
            update();
        });
}

QSize ChatMessage::fontRect(QString str) {
    m_msg = str;
    const int minHei = m_metrics.s(m_metrics.minBubbleHeight);
    const int iconWH = m_metrics.s(m_metrics.icon);
    const int iconSpaceW = m_metrics.s(m_metrics.iconSpace);
    const int iconRectW = m_metrics.s(m_metrics.iconBorder);
    const int iconTMPH = m_metrics.s(m_metrics.iconTop);
    const int iconTopExtra = m_metrics.s(m_metrics.iconTopExtra);
    const int sanJiaoW = m_metrics.s(m_metrics.triangleW);
    const int kuangTMP = m_metrics.s(m_metrics.frameMargin);
    const int textSpaceRect = m_metrics.s(m_metrics.textPadding);
    const int topPad = m_metrics.s(m_metrics.topPad);
    const int minKuangWidth = m_metrics.s(m_metrics.minKuangWidth);
    const int ipHeight = m_metrics.s(m_metrics.ipHeight);
    const int bottomExtra = m_metrics.s(m_metrics.bottomExtra);

    m_kuangWidth = qMax(minKuangWidth,
                        this->width() - kuangTMP -
                            2 * (iconWH + iconSpaceW + iconRectW));
    m_textWidth = qMax(1, m_kuangWidth - 2 * textSpaceRect);
    m_spaceWid = this->width() - m_textWidth;
    m_iconLeftRect =
        QRect(iconSpaceW, iconTMPH + iconTopExtra, iconWH, iconWH);
    m_iconRightRect = QRect(this->width() - iconSpaceW - iconWH,
                            iconTMPH + iconTopExtra, iconWH, iconWH);

    QSize size = getRealString(m_msg);

    spdlog::debug("[ChatMessage] fontRect size {}x{}", size.width(),
                  size.height());
    int hei = size.height() < minHei ? minHei : size.height();
    const int sanjiaoHei = qMax(1, hei - m_lineHeight);

    m_sanjiaoLeftRect = QRect(iconWH + iconSpaceW + iconRectW,
                              m_lineHeight / 2 + topPad, sanJiaoW, sanjiaoHei);
    m_sanjiaoRightRect =
        QRect(this->width() - iconRectW - iconWH - iconSpaceW - sanJiaoW,
              m_lineHeight / 2 + topPad, sanJiaoW, sanjiaoHei);

    if (size.width() < (m_textWidth + m_spaceWid)) {
        m_kuangLeftRect.setRect(
            m_sanjiaoLeftRect.x() + m_sanjiaoLeftRect.width(),
            m_lineHeight / 4 * 3 + topPad,
            size.width() - m_spaceWid + 2 * textSpaceRect, hei - m_lineHeight);
        m_kuangRightRect.setRect(
            this->width() - size.width() + m_spaceWid - 2 * textSpaceRect -
                iconWH - iconSpaceW - iconRectW - sanJiaoW,
            m_lineHeight / 4 * 3 + topPad,
            size.width() - m_spaceWid + 2 * textSpaceRect, hei - m_lineHeight);
    } else {
        m_kuangLeftRect.setRect(
            m_sanjiaoLeftRect.x() + m_sanjiaoLeftRect.width(),
            m_lineHeight / 4 * 3 + topPad, m_kuangWidth, hei - m_lineHeight);
        m_kuangRightRect.setRect(
            iconWH + kuangTMP + iconSpaceW + iconRectW - sanJiaoW,
            m_lineHeight / 4 * 3 + topPad, m_kuangWidth, hei - m_lineHeight);
    }

    m_textLeftRect.setRect(m_kuangLeftRect.x() + textSpaceRect,
                           m_kuangLeftRect.y() + textSpaceRect,
                           m_kuangLeftRect.width() - 2 * textSpaceRect,
                           m_kuangLeftRect.height() - 2 * textSpaceRect);
    m_textRightRect.setRect(m_kuangRightRect.x() + textSpaceRect,
                            m_kuangRightRect.y() + textSpaceRect,
                            m_kuangRightRect.width() - 2 * textSpaceRect,
                            m_kuangRightRect.height() - 2 * textSpaceRect);

    m_ipLeftRect.setRect(
        m_kuangLeftRect.x(),
        m_kuangLeftRect.y() + iconTMPH + m_metrics.s(m_metrics.ipLeftYOffset),
        m_kuangLeftRect.width() - 2 * textSpaceRect + iconWH * 2, ipHeight);
    m_ipRightRect.setRect(
        m_kuangRightRect.x(),
        m_kuangRightRect.y() + iconTMPH + m_metrics.s(m_metrics.ipRightYOffset),
        m_kuangRightRect.width() - 2 * textSpaceRect + iconWH * 2, ipHeight);
    return QSize(size.width(), hei + bottomExtra);
}

QSize ChatMessage::getRealString(QString src) {
    QFontMetricsF fm(this->font());
    m_lineHeight = fm.lineSpacing();

    if (m_textWidth <= 0) {
        const qreal w = fm.horizontalAdvance(src);
        const qreal h = fm.boundingRect(src).height();
        return QSize(static_cast<int>(std::ceil(w + m_spaceWid)),
                     static_cast<int>(std::ceil(h + 2 * m_lineHeight)));
    }

    const QRectF textRect(0, 0, static_cast<qreal>(m_textWidth), 1000000.0);
    // 与 paintEvent 的 WrapAtWordBoundaryOrAnywhere 一致，否则无空格长串
    // 按 TextWordWrap 量高只有一行，绘制时折行会被裁切，看起来像“没发全”。
    const QRectF br = fm.boundingRect(
        textRect,
        Qt::TextWordWrap | Qt::TextWrapAnywhere | Qt::AlignLeft | Qt::AlignTop,
        src);
    const int tw = qMin(m_textWidth, static_cast<int>(std::ceil(br.width())));
    const int th = static_cast<int>(std::ceil(br.height()));
    const int totalH = th + static_cast<int>(2 * m_lineHeight);
    return QSize(tw + m_spaceWid, totalH);
}

void ChatMessage::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(Qt::gray));

    const int radius = m_metrics.s(m_metrics.cornerRadius);
    const int borderPad = m_metrics.s(m_metrics.borderPad);
    const int triHalf = m_metrics.s(m_metrics.triangleHalf);

    if (m_userType == User_Type::User_She) {
        painter.drawPixmap(m_iconLeftRect, m_leftPixmap);

        QColor col_KuangB(234, 234, 234);
        painter.setBrush(QBrush(col_KuangB));
        painter.drawRoundedRect(
            m_kuangLeftRect.adjusted(-borderPad, -borderPad, borderPad,
                                     borderPad),
            radius, radius);
        QColor col_Kuang(255, 255, 255);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangLeftRect, radius, radius);

        const qreal cy = m_sanjiaoLeftRect.center().y();
        QPointF points[3] = {
            QPointF(m_sanjiaoLeftRect.x(), cy),
            QPointF(m_sanjiaoLeftRect.x() + m_sanjiaoLeftRect.width(),
                    cy - triHalf),
            QPointF(m_sanjiaoLeftRect.x() + m_sanjiaoLeftRect.width(),
                    cy + triHalf),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);

        QPen penIp;
        penIp.setColor(Qt::darkGray);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(m_metrics.s(m_metrics.ipFontPointSize));
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipLeftRect, m_ip, op);

        QPen penText;
        penText.setColor(QColor(51, 51, 51));
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(m_textLeftRect, m_msg, option);
    } else if (m_userType == User_Type::User_Me) {
        painter.drawPixmap(m_iconRightRect, m_rightPixmap);

        QColor col_Kuang(75, 164, 242);
        painter.setBrush(QBrush(col_Kuang));
        painter.drawRoundedRect(m_kuangRightRect, radius, radius);

        const qreal cy = m_sanjiaoRightRect.center().y();
        QPointF points[3] = {
            QPointF(m_sanjiaoRightRect.x() + m_sanjiaoRightRect.width(), cy),
            QPointF(m_sanjiaoRightRect.x(), cy - triHalf),
            QPointF(m_sanjiaoRightRect.x(), cy + triHalf),
        };
        QPen pen;
        pen.setColor(col_Kuang);
        painter.setPen(pen);
        painter.drawPolygon(points, 3);

        QPen penIp;
        penIp.setColor(Qt::black);
        painter.setPen(penIp);
        QFont f = this->font();
        f.setPointSize(m_metrics.s(m_metrics.ipFontPointSize));
        QTextOption op(Qt::AlignHCenter | Qt::AlignVCenter);
        painter.setFont(f);
        painter.drawText(m_ipRightRect, m_ip, op);

        QPen penText;
        penText.setColor(Qt::white);
        painter.setPen(penText);
        QTextOption option(Qt::AlignLeft | Qt::AlignVCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        painter.setFont(this->font());
        painter.drawText(m_textRightRect, m_msg, option);
    } else if (m_userType == User_Type::User_Time) {
        QPen penText;
        penText.setColor(QColor(153, 153, 153));
        painter.setPen(penText);
        QTextOption option(Qt::AlignCenter);
        option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        QFont te_font = this->font();
        te_font.setFamily("MicrosoftYaHei");
        te_font.setPointSize(m_metrics.s(m_metrics.timeFontPointSize));
        painter.setFont(te_font);
        painter.drawText(this->rect(), m_curTime, option);
    }
};
