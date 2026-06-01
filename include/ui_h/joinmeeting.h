#ifndef JOINMEETING_H
#define JOINMEETING_H

#include <QDialog>

namespace Ui {
class JoinMeeting;
}

class JoinMeeting : public QDialog
{
    Q_OBJECT

public:
    explicit JoinMeeting(QWidget *parent = nullptr);
    ~JoinMeeting();
    QString getRoomNo();

private slots:
    void JoinMeeting_btn_clicked();
private:
    Ui::JoinMeeting *ui;
    QString roomNo;
};

#endif // JOINMEETING_H
