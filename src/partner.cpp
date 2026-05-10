#include "partner.h"
#include "configure/configure.h"
#include <QString>
#include <QDebug>
#include <QEvent>
#include <QHostAddress>
Partner::Partner(QWidget *parent, quint32 p):QLabel(parent)
{
//    qDebug() <<"dsaf" << this->parent();
    ip = p; //保存ip

    //水平方向尽量随父布局拉伸，垂直方向高度取“最小需要”，适合放在网格/横向列表里和别的项对齐。
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    w = ((QWidget *)this->parent())->size().width(); //获取父窗口的宽度
    this->setPixmap(QPixmap::fromImage(QImage(QString::fromUtf8(Source::default_avatar)).scaled(w-10, w-10)));
    this->setFrameShape(QFrame::Box); //设置边框形状

    //设置 1px 蓝色半透明描边
    this->setStyleSheet("border-width: 1px; border-style: solid; border-color:rgba(0, 0 , 255, 0.7)");
//    this->setToolTipDuration(5);

    //鼠标悬停时显示 该 IP 的点分十进制字符串
    this->setToolTip(QHostAddress(ip).toString());
}


void Partner::mousePressEvent(QMouseEvent *)
{
    emit sendip(ip);
}
void Partner::setpic(QImage img)
{
    this->setPixmap(QPixmap::fromImage(img.scaled(w-10, w-10)));
}
