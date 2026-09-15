#ifndef EDIT_PROFILE_DIALOG_H
#define EDIT_PROFILE_DIALOG_H

#include "frameless_window.h"
#include "edit_profile_dialog_ui.h"

#include <QDialog>
#include <QImage>

class QNetworkReply;

/** 修改昵称 / 简介 / 头像 */
class EditProfileDialog : public FramelessWindow<QDialog> {
    Q_OBJECT

public:
    explicit EditProfileDialog(QWidget *parent = nullptr);

signals:
    void profileSaved();

private slots:
    void onChangeAvatarClicked();
    void onSaveClicked();
    void onAvatarUploadFinished();
    void onProfileUpdateFinished();

private:
    void loadFromSession();
    void setBusy(bool busy);
    void uploadPendingAvatar();
    void submitProfileUpdate();

    Ui_edit_profile_dialog ui;
    QImage m_pendingAvatar;
    bool m_busy = false;
    QNetworkReply *m_reply = nullptr;
};

#endif // EDIT_PROFILE_DIALOG_H
