#include "stack_user_profile.h"

#include "avatar_image_loader.h"
#include "configure/client_config.h"
#include "configure/user_session.h"

#include <QFile>
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
        session.avatar(), ui->profileAvatarLarge->size(),
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

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("更换头像"), tr("无法读取图片文件"));
        return;
    }

    const QByteArray bytes = file.readAll();
    const QString mime = detectMime(filePath);
    if (mime.isEmpty()) {
        QMessageBox::warning(this, tr("更换头像"), tr("不支持的图片格式"));
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("user_id"), UserSession::instance().userId());
    body.insert(QStringLiteral("mime"), mime);
    body.insert(QStringLiteral("data_base64"),
                QString::fromLatin1(bytes.toBase64()));

    auto *nam = AvatarImageLoader::instance().networkManager();
    if (!nam) {
        QMessageBox::warning(this, tr("更换头像"), tr("网络模块未初始化"));
        return;
    }

    m_pendingOldAvatar = UserSession::instance().avatar();
    m_uploadInFlight = true;
    ui->changeAvatarBtn->setEnabled(false);

    const QUrl url =
        authUrl(ClientConfig::instance().auth().upload_avatar_path);
    auto *reply =
        nam->post(makeJsonRequest(url),
                  QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            &stack_user_profile::on_upload_finished);
}

QString stack_user_profile::detectMime(const QString &filePath) const {
    const QString lower = filePath.toLower();
    if (lower.endsWith(QStringLiteral(".png"))) {
        return QStringLiteral("image/png");
    }
    if (lower.endsWith(QStringLiteral(".jpg")) ||
        lower.endsWith(QStringLiteral(".jpeg"))) {
        return QStringLiteral("image/jpeg");
    }
    if (lower.endsWith(QStringLiteral(".webp"))) {
        return QStringLiteral("image/webp");
    }
    return {};
}

void stack_user_profile::on_upload_finished(QNetworkReply *reply) {
    m_uploadInFlight = false;
    ui->changeAvatarBtn->setEnabled(true);

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
