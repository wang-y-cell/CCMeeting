#include "login.h"
#include "ui_login.h"

#include "configure/auth_config.h"
#include "configure/user_session.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QUrl>

#include <spdlog/spdlog.h>

login::login(QWidget *parent)
    : FramelessWindow<QDialog>(parent)
    , ui(new Ui::login)
{
    ui->setupUi(this);
    setWindowTitle(tr("登录")); ///< 设置窗口标题为登录
    setResizable(false);   ///< 禁止拖边缩放
    setMaximizable(false); ///< 禁止最大化按钮与双击标题栏
    set_style();

    connect(ui->login_button, &QPushButton::clicked, this, &login::Login);
    connect(&m_nam, &QNetworkAccessManager::finished, this, &login::onLoginFinished);
}

void login::set_style() {
    QFile file(":/Style/source/login.qss");
    if (file.open(QFile::ReadOnly)) {
        spdlog::info("login.qss loaded");
        QString styleSheet = file.readAll();
        this->setStyleSheet(styleSheet);
        file.close();
    } else {
        spdlog::warn("login.qss not found");
    }
}

login::~login()
{
    delete ui;
}

/**
 * @details 读取账号密码，向认证服务 POST /api/login；成功则写入 UserSession 并 accept()
 */
void login::Login() {
    if (m_requestInFlight) {
        return;
    }

    const QString username = ui->account_line->text().trimmed();
    const QString password = ui->password_line->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Login Error"), tr("请输入账号和密码"));
        return;
    }

    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QString::fromUtf8(AuthConfig::host));
    url.setPort(AuthConfig::port);
    url.setPath(QString::fromUtf8(AuthConfig::login_path));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json; charset=utf-8"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("CloudMeetingClient/1.0"));

    QJsonObject body;
    body.insert(QStringLiteral("username"), username);
    body.insert(QStringLiteral("password"), password);

    m_requestInFlight = true;
    ui->login_button->setEnabled(false);
    spdlog::info("[login] POST {}", url.toString().toStdString());
    m_nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void login::onLoginFinished(QNetworkReply* reply) {
    m_requestInFlight = false;
    ui->login_button->setEnabled(true);

    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError &&
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0) {
        spdlog::error("[login] network error: {}", reply->errorString().toStdString());
        QMessageBox::warning(this,
                             tr("Login Error"),
                             tr("无法连接登录服务器，请确认认证服务已启动"));
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        spdlog::error("[login] invalid response: {}", payload.toStdString());
        QMessageBox::warning(this, tr("Login Error"), tr("登录响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    const QString message = root.value(QStringLiteral("message")).toString();

    if (code != 0 || !root.contains(QStringLiteral("data"))) {
        spdlog::warn("[login] failed code={} msg={}", code, message.toStdString());
        QMessageBox::warning(this,
                             tr("Login Error"),
                             message.isEmpty() ? tr("账号或密码错误") : message);
        return;
    }

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const qint64 userId = data.value(QStringLiteral("id")).toVariant().toLongLong();
    const QString name = data.value(QStringLiteral("name")).toString();
    const QString avatar = data.value(QStringLiteral("avatar")).toString();
    const QString info = data.value(QStringLiteral("info")).toString();

    UserSession::instance().setUser(userId, name, avatar, info);
    spdlog::info("[login] success id={} name={}", userId, name.toStdString());
    accept();
}
