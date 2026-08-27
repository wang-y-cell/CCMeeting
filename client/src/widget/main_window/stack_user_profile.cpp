#include "stack_user_profile.h"

#include "avatar_crop_dialog.h"
#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/user_session.h"

#include <QBuffer>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>

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

stack_user_profile::stack_user_profile(QWidget *parent)
    : QWidget(parent), ui(new Ui::stack_user_profile) {
    ui->setupUi(this);
    connect(ui->changeAvatarBtn, &QPushButton::clicked, this,
            &stack_user_profile::on_change_avatar_clicked);
    connect(ui->backHomeBtn, &QPushButton::clicked, this,
            &stack_user_profile::backHomeRequested);
}

stack_user_profile::~stack_user_profile() { delete ui; }

void stack_user_profile::refreshFromSession() {
    const auto &session = UserSession::instance();
    ui->valueNickname->setText(session.name().isEmpty() ? QStringLiteral("-")
                                                        : session.name());
    ui->valueUsername->setText(session.username().isEmpty()
                                   ? QStringLiteral("-")
                                   : session.username());
    ui->valueUserId->setText(session.userId() > 0
                                 ? QString::number(session.userId())
                                 : QStringLiteral("-"));
    ui->valueInfo->setText(session.info().isEmpty() ? QStringLiteral("-")
                                                    : session.info());

    AvatarImageLoader::instance().load(
        session.avatar(), ui->profileAvatarLarge->size(), this,
        [this](const QPixmap &pixmap) {
            ui->profileAvatarLarge->setPixmap(pixmap);
        });
}

void stack_user_profile::on_change_avatar_clicked() {
    if (m_uploadInFlight) {
        return;
    }
    if (!UserSession::instance().isLoggedIn()) {
        QMessageBox::warning(this, tr("更换头像"), tr("请先登录"));
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

    uploadCroppedAvatar(cropped.value());
}

void stack_user_profile::uploadCroppedAvatar(const QImage &cropped) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!cropped.save(&buffer, "PNG")) {
        QMessageBox::warning(this, tr("更换头像"), tr("无法编码裁剪结果"));
        return;
    }

    auto *nam = AvatarImageLoader::instance().networkManager();
    if (!nam) {
        QMessageBox::warning(this, tr("更换头像"), tr("网络模块未初始化"));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("user_id"), UserSession::instance().userId());
    body.insert(QStringLiteral("mime"), QStringLiteral("image/png"));
    body.insert(QStringLiteral("data_base64"),
                QString::fromLatin1(bytes.toBase64()));

    m_pendingOldAvatar = UserSession::instance().avatar();
    m_uploadInFlight = true;
    ui->changeAvatarBtn->setEnabled(false);

    const QUrl url =
        authUrl(ClientConfig::instance().auth().upload_avatar_path);
    auto *reply =
        nam->post(makeJsonRequest(url),
                  QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_uploadReply = reply;
    connect(reply, &QNetworkReply::finished, this,
            &stack_user_profile::on_upload_finished);
}

void stack_user_profile::on_upload_finished() {
    m_uploadInFlight = false;
    ui->changeAvatarBtn->setEnabled(true);

    QNetworkReply *reply = m_uploadReply;
    m_uploadReply = nullptr;
    if (!reply) {
        QMessageBox::warning(this, tr("更换头像"), tr("上传失败"));
        return;
    }

    const QByteArray payload = reply->readAll();
    const auto netErr = reply->error();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("更换头像"), tr("上传请求失败"));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("更换头像"), tr("服务器响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    if (code != 0) {
        const QString message = root.value(QStringLiteral("message")).toString();
        QMessageBox::warning(this, tr("更换头像"),
                             message.isEmpty() ? tr("上传失败") : message);
        return;
    }

    const QString newAvatar =
        root.value(QStringLiteral("data"))
            .toObject()
            .value(QStringLiteral("avatar"))
            .toString();
    if (newAvatar.isEmpty()) {
        QMessageBox::warning(this, tr("更换头像"), tr("服务器未返回头像地址"));
        return;
    }

    AvatarImageLoader::instance().invalidate(m_pendingOldAvatar);
    UserSession::instance().setAvatar(newAvatar);
    m_pendingOldAvatar.clear();
    refreshFromSession();
    emit avatarUpdated();
    QMessageBox::information(this, tr("更换头像"), tr("头像已更新"));
}
