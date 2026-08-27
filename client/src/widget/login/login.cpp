#include "login.h"
#include "ui_login.h"

#include "configure/client_config.h"
#include "configure/user_session.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include <spdlog/spdlog.h>

namespace {

std::string qutf8(const QString &s) {
    const QByteArray bytes = s.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

void flush_log() {
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
}

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

login::login(QWidget *parent)
    : FramelessWindow<QDialog>(parent), ui(new Ui::login) {
    ui->setupUi(this);
    setWindowTitle(tr("登录"));
    setResizable(false);
    setMaximizable(false);
    set_style();

    connect(ui->login_button, &QPushButton::clicked, this, &login::Login);
    connect(ui->register_button, &QPushButton::clicked, this, &login::Register);
    connect(ui->createUserLink, &QLabel::linkActivated, this,
            [this](const QString &) { showRegisterPage(); });
    connect(ui->backToLoginLink, &QLabel::linkActivated, this,
            [this](const QString &) { showLoginPage(); });
    connect(&m_nam, &QNetworkAccessManager::finished, this,
            [this](QNetworkReply *reply) {
                if (m_pendingRequest == PendingRequest::Register) {
                    onRegisterFinished(reply);
                } else {
                    onLoginFinished(reply);
                }
            });
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

login::~login() { delete ui; }

void login::showRegisterPage() {
    ui->stackedWidget->setCurrentWidget(ui->registerPage);
    setWindowTitle(tr("创建账号"));
}

void login::showLoginPage() {
    ui->stackedWidget->setCurrentWidget(ui->loginPage);
    setWindowTitle(tr("登录"));
}

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

    const QUrl url = authUrl(ClientConfig::instance().auth().login_path);

    QJsonObject body;
    body.insert(QStringLiteral("username"), username);
    body.insert(QStringLiteral("password"), password);

    m_pendingRequest = PendingRequest::Login;
    m_requestInFlight = true;
    ui->login_button->setEnabled(false);
    spdlog::info("[login] POST {}", qutf8(url.toString()));
    flush_log();
    m_nam.post(makeJsonRequest(url),
               QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void login::Register() {
    if (m_requestInFlight) {
        return;
    }

    const QString username = ui->register_username_line->text().trimmed();
    const QString password = ui->register_password_line->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Register Error"), tr("请输入用户名和密码"));
        return;
    }

    const QUrl url = authUrl(ClientConfig::instance().auth().register_path);

    QJsonObject body;
    body.insert(QStringLiteral("username"), username);
    body.insert(QStringLiteral("password"), password);

    m_pendingRequest = PendingRequest::Register;
    m_requestInFlight = true;
    ui->register_button->setEnabled(false);
    spdlog::info("[register] POST {}", qutf8(url.toString()));
    flush_log();
    m_nam.post(makeJsonRequest(url),
               QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void login::onLoginFinished(QNetworkReply *reply) {
    if (m_pendingRequest == PendingRequest::Register) {
        return;
    }

    m_requestInFlight = false;
    m_pendingRequest = PendingRequest::None;
    ui->login_button->setEnabled(true);

    spdlog::info("[login] onLoginFinished enter");
    flush_log();

    if (!reply) {
        spdlog::error("[login] reply is null");
        flush_log();
        return;
    }

    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto netErr = reply->error();
    spdlog::info("[login] httpStatus={} qNetworkError={}", httpStatus,
                 static_cast<int>(netErr));
    flush_log();

    if (netErr != QNetworkReply::NoError && httpStatus == 0) {
        spdlog::error("[login] network error");
        flush_log();
        reply->deleteLater();
        QMessageBox::warning(this, tr("Login Error"),
                             tr("无法连接登录服务器，请确认认证服务已启动"));
        return;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();
    spdlog::info("[login] response bytes={}", payload.size());
    flush_log();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        spdlog::error("[login] invalid response parseError={}",
                      static_cast<int>(parseError.error));
        flush_log();
        QMessageBox::warning(this, tr("Login Error"), tr("登录响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    const QString message = root.value(QStringLiteral("message")).toString();

    if (code != 0 || !root.contains(QStringLiteral("data"))) {
        spdlog::warn("[login] failed code={}", code);
        flush_log();
        QMessageBox::warning(this, tr("Login Error"),
                             message.isEmpty() ? tr("账号或密码错误")
                                               : message);
        return;
    }

    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const qint64 userId =
        data.value(QStringLiteral("id")).toVariant().toLongLong();
    const QString name = data.value(QStringLiteral("name")).toString();
    const QString avatar = data.value(QStringLiteral("avatar")).toString();
    const QString info = data.value(QStringLiteral("info")).toString();

    UserSession::instance().setUser(userId, name, avatar, info);
    spdlog::info("[login] success id={} name_bytes={}", userId,
                 name.toUtf8().size());
    spdlog::info("[login] calling accept()");
    flush_log();
    accept();
    spdlog::info("[login] accept() returned");
    flush_log();
}

void login::onRegisterFinished(QNetworkReply *reply) {
    if (m_pendingRequest != PendingRequest::Register) {
        return;
    }

    m_requestInFlight = false;
    m_pendingRequest = PendingRequest::None;
    ui->register_button->setEnabled(true);

    const QString username = ui->register_username_line->text().trimmed();

    if (!reply) {
        QMessageBox::warning(this, tr("Register Error"), tr("注册请求失败"));
        return;
    }

    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto netErr = reply->error();

    if (netErr != QNetworkReply::NoError && httpStatus == 0) {
        reply->deleteLater();
        QMessageBox::warning(this, tr("Register Error"),
                             tr("无法连接登录服务器，请确认认证服务已启动"));
        return;
    }

    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Register Error"), tr("注册响应格式错误"));
        return;
    }

    const QJsonObject root = doc.object();
    const int code = root.value(QStringLiteral("code")).toInt(-1);
    const QString message = root.value(QStringLiteral("message")).toString();

    if (code != 0) {
        QMessageBox::warning(this, tr("Register Error"),
                             message.isEmpty() ? tr("注册失败") : message);
        return;
    }

    ui->register_password_line->clear();
    ui->account_line->setText(username);
    ui->password_line->clear();
    showLoginPage();
    QMessageBox::information(this, tr("Register Success"),
                             tr("注册成功，请使用新账号登录"));
}
