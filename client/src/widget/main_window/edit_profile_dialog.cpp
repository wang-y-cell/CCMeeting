#include "edit_profile_dialog.h"

#include "avatar_crop_dialog.h"
#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/user_session.h"
#include "style_loader.h"

#include <QBuffer>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
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
#include <QUrl>
#include <QVBoxLayout>

namespace {

QUrl authUrl(const QString &path) {
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    const auto &auth = ClientConfig::instance().auth();
    url.setHost(auth.host);
    url.setPort(auth.port);
    url.setPath(path);
    return url;
}

QNetworkRequest makeJsonRequest(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CloudMeetingClient/1.0"));
    return request;
}

}  // namespace

EditProfileDialog::EditProfileDialog(QWidget *parent)
    : FramelessWindow<QDialog>(parent) {
    setWindowTitle(tr("修改资料"));
    setTitleBarHeight(40);
    setMaximizable(false);
    setResizable(false);
    setModal(true);

    ui.setupUi(this);
    loadWidgetStyleSheet(
        this, QStringLiteral(":/Style/source/edit_profile_dialog.qss"));
    loadFromSession();

    connect(ui.changeAvatarBtn, &QPushButton::clicked, this,
            &EditProfileDialog::onChangeAvatarClicked);
    connect(ui.saveBtn, &QPushButton::clicked, this,
            &EditProfileDialog::onSaveClicked);
    connect(ui.cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void EditProfileDialog::loadFromSession() {
    const auto &session = UserSession::instance();
    ui.nicknameEdit->setText(session.name());
    ui.usernameValue->setText(session.username().isEmpty()
                                  ? QStringLiteral("-")
                                  : session.username());
    ui.userIdValue->setText(session.userId() > 0
                                ? QString::number(session.userId())
                                : QStringLiteral("-"));
    ui.infoEdit->setPlainText(session.info());

    AvatarImageLoader::instance().load(
        session.avatar(), ui.avatarPreview->size(), this,
        [this](const QPixmap &pixmap) { ui.avatarPreview->setPixmap(pixmap); });
}

void EditProfileDialog::setBusy(bool busy) {
    m_busy = busy;
    ui.saveBtn->setEnabled(!busy);
    ui.cancelBtn->setEnabled(!busy);
    ui.changeAvatarBtn->setEnabled(!busy);
    ui.nicknameEdit->setEnabled(!busy);
    ui.infoEdit->setEnabled(!busy);
}

void EditProfileDialog::onChangeAvatarClicked() {
    if (m_busy) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("选择头像"), QString(),
        tr("Images (*.png *.jpg *.jpeg *.webp)"));
    if (filePath.isEmpty()) {
        return;
    }

    const auto cropped = AvatarCropDialog::cropFromFile(this, filePath);
    if (!cropped.has_value() || cropped->isNull()) {
        return;
    }

    m_pendingAvatar = cropped.value();
    ui.avatarPreview->setPixmap(
        QPixmap::fromImage(m_pendingAvatar)
            .scaled(ui.avatarPreview->size(), Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
}

void EditProfileDialog::onSaveClicked() {
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

void EditProfileDialog::uploadPendingAvatar() {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!m_pendingAvatar.save(&buffer, "PNG")) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("无法编码头像"));
        return;
    }

    auto *nam = AvatarImageLoader::instance().networkManager();
    if (!nam) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("网络模块未初始化"));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("user_id"), UserSession::instance().userId());
    body.insert(QStringLiteral("mime"), QStringLiteral("image/png"));
    body.insert(QStringLiteral("data_base64"),
                QString::fromLatin1(bytes.toBase64()));

    const QUrl url =
        authUrl(ClientConfig::instance().auth().upload_avatar_path);
    m_reply = nam->post(makeJsonRequest(url),
                        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this,
            &EditProfileDialog::onAvatarUploadFinished);
}

void EditProfileDialog::onAvatarUploadFinished() {
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

    const QString newAvatar =
        root.value(QStringLiteral("data"))
            .toObject()
            .value(QStringLiteral("avatar"))
            .toString();
    if (newAvatar.isEmpty()) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("服务器未返回头像地址"));
        return;
    }

    AvatarImageLoader::instance().invalidate(UserSession::instance().avatar());
    UserSession::instance().setAvatar(newAvatar);
    m_pendingAvatar = QImage();
    submitProfileUpdate();
}

void EditProfileDialog::submitProfileUpdate() {
    auto *nam = AvatarImageLoader::instance().networkManager();
    if (!nam) {
        setBusy(false);
        QMessageBox::warning(this, tr("修改资料"), tr("网络模块未初始化"));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("user_id"), UserSession::instance().userId());
    body.insert(QStringLiteral("nickname"),
                ui.nicknameEdit->text().trimmed());
    body.insert(QStringLiteral("info"), ui.infoEdit->toPlainText().trimmed());

    const QUrl url =
        authUrl(ClientConfig::instance().auth().update_profile_path);
    m_reply = nam->post(makeJsonRequest(url),
                        QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_reply, &QNetworkReply::finished, this,
            &EditProfileDialog::onProfileUpdateFinished);
}

void EditProfileDialog::onProfileUpdateFinished() {
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

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QString name = data.value(QStringLiteral("name")).toString(
        ui.nicknameEdit->text().trimmed());
    const QString info = data.value(QStringLiteral("info")).toString(
        ui.infoEdit->toPlainText().trimmed());

    UserSession::instance().updateProfile(name, QString(), info);
    setBusy(false);
    emit profileSaved();
    accept();
}
