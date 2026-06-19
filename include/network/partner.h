#ifndef PARTNER_H
#define PARTNER_H

#include <QWidget>

class QLabel;

class Partner : public QWidget
{
    Q_OBJECT
public:
    Partner(QWidget *parent = nullptr, quint32 ip = 0);
    quint32 getIp() const { return ip; }
    QLabel *displayLabel() const { return m_displayLabel; }

signals:
    void sendip(quint32);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLabelGeometry();

    quint32 ip = 0;
    QLabel *m_displayLabel = nullptr;
    int w = 40;
};

#endif // PARTNER_H
