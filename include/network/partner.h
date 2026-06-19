#ifndef PARTNER_H
#define PARTNER_H

#include <QWidget>
#include <cstdint>

class QLabel;

class Partner : public QWidget
{
    Q_OBJECT
public:
    Partner(QWidget *parent = nullptr, std::uint32_t ip = 0);
    std::uint32_t getIp() const { return ip; }
    QLabel *displayLabel() const { return m_displayLabel; }

signals:
    void sendip(std::uint32_t);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLabelGeometry();

    std::uint32_t ip = 0;
    QLabel *m_displayLabel = nullptr;
    int w = 40;
};

#endif // PARTNER_H
