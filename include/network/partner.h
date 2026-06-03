#ifndef PARTNER_H
#define PARTNER_H

#include <QLabel>
#include <QImage>

class Partner : public QLabel
{
    Q_OBJECT
private:
    quint32 ip;
    QImage m_sourceImage;
    int w;

    void mousePressEvent(QMouseEvent *ev) override; //点击事件
    void resizeEvent(QResizeEvent *event) override;
    void updatePixmap();
public:
    Partner(QWidget * parent = nullptr, quint32 = 0);
    void setpic(QImage img);
signals:
    void sendip(quint32); //发送ip
};

#endif // PARTNER_H
