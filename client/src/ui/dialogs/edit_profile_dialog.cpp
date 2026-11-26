#include "ui/dialogs/edit_profile_dialog.h"
#include "config/global_config.h"

#include "ui/foundation/auth_http.h"
#include "ui/foundation/theme_manager.h"

#include "ui/dialogs/avatar_crop_dialog.h"
#include "ui/composites/calendar_popup.h"
#include "media/avatar_service.h"
#include "config/client_config.h"
#include "config/user_session.h"

#include <QBuffer>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTextEdit>
#include <QToolButton>
#include <QUrl>

namespace {

UserProfileExtras extrasFromJson(const QJsonObject &data) {
    UserProfileExtras extras;
    extras.gender = data.value(QStringLiteral("gender")).toString();
    extras.birthday = data.value(QStringLiteral("birthday")).toString();
    extras.address = data.value(QStringLiteral("address")).toString();
    extras.phone = data.value(QStringLiteral("phone")).toString();
    extras.email = data.value(QStringLiteral("email")).toString();
    extras.extraJson = data.value(QStringLiteral("extra_json")).toString();
    return extras;
}

}  // namespace

ui::EditProfileDialog::EditProfileDialog(QWidget *parent)
    : FramelessWindow<QDialog>(parent) {
    setWindowTitle(tr("修改资料"));
    setTitleBarHeight(40);
    setMaximizable(false);
    setResizable(true);
    setModal(true);

    ui.setupUi(this);
    ui::ThemeManager::instance().apply_to(
        this, QString::fromLatin1(config::paths::style::edit_profile));
    loadFromSession();

    connect(ui.changeAvatarBtn, &QPushButton::clicked, this,
            &ui::EditProfileDialog::onChangeAvatarClicked);
    connect(ui.saveBtn, &QPushButton::clicked, this,
            &ui::EditProfileDialog::onSaveClicked);
    connect(ui.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui.birthdayEdit, &QDateEdit::dateChanged, this,
            &ui::EditProfileDialog::refreshAgeLabel);
    connect(ui.birthdayPickBtn, &QToolButton::clicked, this, [this]() {
        auto *popup = new ui::CalendarPopup(this);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        const QDate minimum = ui.birthdayEdit->minimumDate();
        const QDate current = ui.birthdayEdit->date();
        popup->setDate(current > minimum ? current : QDate(2000, 1, 1));
        connect(popup, &ui::CalendarPopup::date_selected, this,
                [this](const QDate &date) { ui.birthdayEdit->setDate(date); });
        connect(popup, &ui::CalendarPopup::cleared, this,
                [this]() { ui.birthdayEdit->setDate(ui.birthdayEdit->minimumDate()); });
        const QPoint anchor = ui.birthdayPickBtn->mapToGlobal(
            QPoint(0, ui.birthdayPickBtn->height() + 6));
        popup->move(anchor);
        popup->show();
    });
}

void ui::EditProfileDialog::refreshAgeLabel() {
    if (!ui.ageValue || !ui.birthdayEdit) {
        return;
    }
    const QDate date = ui.birthdayEdit->date();
    if (date <= ui.birthdayEdit->minimumDate()) {
        ui.ageValue->setText(QStringLiteral("-"));
        return;
    }
    const int age =
        UserSession::ageFromBirthday(date.toString(Qt::ISODate));
    ui.ageValue->setText(age >= 0 ? QString::number(age) : QStringLiteral("-"));
}

void ui::EditProfileDialog::loadFromSession() {
    const auto &session = UserSession::instance();
    ui.nicknameEdit->setText(session.name());
    ui.infoEdit->setPlainText(session.info());
    ui.addressEdit->setText(session.address());
    ui.phoneEdit->setText(session.phone());
    ui.emailEdit->setText(session.email());

    const int genderIndex =
        ui.genderCombo->findData(session.gender());
    ui.genderCombo->setCurrentIndex(genderIndex >= 0 ? genderIndex : 0);

    const QDate born = QDate::fromString(session.birthday(), Qt::ISODate);
    if (born.isValid()) {
        ui.birthdayEdit->setDate(born);
    } else {
        ui.birthdayEdit->setDate(ui.birthdayEdit->minimumDate());
    }
    refreshAgeLabel();

    AvatarService::instance().load(
        session.avatar(), ui.avatarPreview->size(), this,
        [this](const QPixmap &pixmap) { ui.avatarPreview->setPixmap(pixmap); });
}

UserProfileExtras ui::EditProfileDialog::collectExtras() const {
    UserProfileExtras extras = UserSession::instance().extras();
    extras.gender = ui.genderCombo->currentData().toString();
    const QDate date = ui.birthdayEdit->date();
    if (date > ui.birthdayEdit->minimumDate()) {
        extras.birthday = date.toString(Qt::ISODate);
    } else {
        extras.birthday.clear();
    }
    extras.address = ui.addressEdit->text().trimmed();
    extras.phone = ui.phoneEdit->text().trimmed();
    extras.email = ui.emailEdit->text().trimmed();
    return extras;
}

void ui::EditProfileDialog::setBusy(bool busy) {
    m_busy = busy;
    ui.saveBtn->setEnabled(!busy);
    ui.cancelBtn->setEnabled(!busy);
    ui.changeAvatarBtn->setEnabled(!busy);
    ui.nicknameEdit->setEnabled(!busy);
    ui.genderCombo->setEnabled(!busy);
    ui.birthdayEdit->setEnabled(!busy);
    ui.addressEdit->setEnabled(!busy);
    ui.phoneEdit->setEnabled(!busy);
    ui.emailEdit->setEnabled(!busy);
    ui.infoEdit->setEnabled(!busy);
}

void ui::EditProfileDialog::onChangeAvatarClicked() {
    if (m_busy) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("选择头像"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.webp)"));
    if (filePath.isEmpty()) {
        return;
    }

    const auto cropped = ui::AvatarCropDialog::cropFromFile(this, filePath);
    if (!cropped.has_value() || cropped->isNull()) {
        return;
    }

    m_pendingAvatar = cropped.value();
    ui.avatarPreview->setPixmap(
        QPixmap::fromImage(m_pendingAvatar)
            .scaled(ui.avatarPreview->size(), Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
}

void ui::EditProfileDialog::onSaveClicked() {
    if (m_busy) {
        return;
    }
    if (!UserSession::instance().isLoggedIn()) {
        QMessageBox::warning(this, tr("修改资料"), tr("请先登录"));
        return;
    }

    const QString nickname = ui.nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        QMessageBox::warning(this, tr("修改资料"), tr("昵称不能为空"));
        return;
    }

    setBusy(true);
    if (!m_pendingAvatar.isNull()) {
        uploadPendingAvatar();
    } else {
        submitProfileUpdate();
    }
}

void ui::EditProfileDialog::uploadPendingAvatar() {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!m_pendingAvatar.save(&buffer, "PNG")) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("无法编码头像"));
        return;
    }

    auto *nam = AvatarService::instance().networkManager();
    if (!nam) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("网络模块未初始化"));
        return;
    }

    // v2 规范：{"data":{"mime","data_base64"}}；身份取自 token，不再传 user_id
    QJsonObject avatar_data;
    avatar_data.insert(QStringLiteral("mime"), QStringLiteral("image/png"));
    avatar_data.insert(QStringLiteral("data_base64"),
                       QString::fromLatin1(bytes.toBase64()));
    QJsonObject body;
    body.insert(QStringLiteral("data"), avatar_data);

    const QUrl url =
        ui::auth_url(ClientConfig::instance().auth().upload_avatar_path);
    m_reply = nam->post(ui::auth_request(url),
                        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this,
            &ui::EditProfileDialog::onAvatarUploadFinished);
}

void ui::EditProfileDialog::onAvatarUploadFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("头像上传失败"));
        return;
    }

    const QByteArray payload = reply->readAll();
    const auto netErr = reply->error();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("头像上传请求失败"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("头像上传响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("code")).toInt(-1) != 0) {
        setBusy(false);
        const QString message = root.value(QStringLiteral("message")).toString();
        QMessageBox::warning(this, tr("修改资料"),
                             message.isEmpty() ? tr("头像上传失败") : message);
        return;
    }

    // v2：data = { user_id, avatar }
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QString newAvatar = data.value(QStringLiteral("avatar")).toString();
    if (newAvatar.isEmpty()) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("服务器未返回头像地址"));
        return;
    }

    AvatarService::instance().invalidate(UserSession::instance().avatar());
    UserSession::instance().setAvatar(newAvatar);
    m_pendingAvatar = QImage();
    submitProfileUpdate();
}

void ui::EditProfileDialog::submitProfileUpdate() {
    auto *nam = AvatarService::instance().networkManager();
    if (!nam) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("网络模块未初始化"));
        return;
    }

    const UserProfileExtras extras = collectExtras();
    // v2 规范：请求体包进 data —— {"data":{"nickname","info",...}}
    // 身份取自 token，不再传 user_id（服务端拒绝多余的 user_id 语义）
    QJsonObject profile;
    profile.insert(QStringLiteral("nickname"),
                   ui.nicknameEdit->text().trimmed());
    profile.insert(QStringLiteral("info"), ui.infoEdit->toPlainText().trimmed());
    profile.insert(QStringLiteral("gender"), extras.gender);
    profile.insert(QStringLiteral("birthday"), extras.birthday);
    profile.insert(QStringLiteral("address"), extras.address);
    profile.insert(QStringLiteral("phone"), extras.phone);
    profile.insert(QStringLiteral("email"), extras.email);
    profile.insert(QStringLiteral("extra_json"), extras.extraJson);
    QJsonObject body;
    body.insert(QStringLiteral("data"), profile);

    const QUrl url =
        ui::auth_url(ClientConfig::instance().auth().update_profile_path);
    m_reply = nam->post(ui::auth_request(url),
                        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this,
            &ui::EditProfileDialog::onProfileUpdateFinished);
}

void ui::EditProfileDialog::onProfileUpdateFinished() {
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("保存失败"));
        return;
    }

    const QByteArray payload = reply->readAll();
    const auto netErr = reply->error();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("保存请求失败"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("服务器响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("code")).toInt(-1) != 0) {
        setBusy(false);
        const QString message = root.value(QStringLiteral("message")).toString();
        QMessageBox::warning(this, tr("修改资料"),
                             message.isEmpty() ? tr("保存失败") : message);
        return;
    }

    // v2：data = { user:{user_id,username,nickname,avatar,info,...} }
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject user = data.value(QStringLiteral("user")).toObject();
    const QString name = user.value(QStringLiteral("nickname")).toString(
        ui.nicknameEdit->text().trimmed());
    const QString info = user.value(QStringLiteral("info")).toString(
        ui.infoEdit->toPlainText().trimmed());
    UserProfileExtras extras = extrasFromJson(user);
    if (extras.gender.isEmpty() && extras.birthday.isEmpty() &&
        extras.address.isEmpty()) {
        extras = collectExtras();
    }

    // data.user 是服务端权威快照；头像为空时不覆盖本地缓存（QString() 为 null）
    const QString avatar = user.value(QStringLiteral("avatar")).toString();
    UserSession::instance().updateProfile(name,
                                          avatar.isEmpty() ? QString() : avatar,
                                          info, extras);
    setBusy(false);
    emit profileSaved();
    accept();
}
