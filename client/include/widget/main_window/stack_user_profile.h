#ifndef STACK_USER_PROFILE_H
#define STACK_USER_PROFILE_H

#include "ui_stack_user_profile.h"
#include <QImage>
#include <QWidget>

class QNetworkReply;

class stack_user_profile : public QWidget {
    Q_OBJECT

public:
    explicit stack_user_profile(QWidget *parent = nullptr);
    ~stack_user_profile();

    void refreshFromSession();

signals:
    void backHomeRequested();
    void avatarUpdated();

private slots:
    void on_change_avatar_clicked();
    void on_upload_finished();

private:
    void uploadCroppedAvatar(const QImage &cropped);

    Ui::stack_user_profile *ui = nullptr;
    QString m_pendingOldAvatar;
    bool m_uploadInFlight = false;
    QNetworkReply *m_uploadReply = nullptr;
};

#endif // STACK_USER_PROFILE_H
