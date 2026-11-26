#include "ui/pages/login_window.h"
#include "config/global_config.h"

#include "ui/foundation/auth_http.h"
#include "ui/foundation/theme_manager.h"

#include "config/login_store.h"

#include "config/client_config.h"
#include "config/user_session.h"

#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QTimer>
#include <QNetworkRequest>
#include <QStackedWidget>
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

}  // namespace

ui::LoginWindow::LoginWindow(QWidget *parent)
    : FramelessWindow<QDialog>(parent) {
    ui.setupUi(this);
    setWindowTitle(tr("登录"));
    setResizable(false);
    setMaximizable(false);
    set_style();

    connect(ui.login_button, &QPushButton::clicked, this, &ui::LoginWindow::Login);
    connect(ui.register_button, &QPushButton::clicked, this, &ui::LoginWindow::Register);
    connect(ui.createUserLink, &QLabel::linkActivated, this,
            [this](const QString &) { showRegisterPage(); });
    connect(ui.backToLoginLink, &QLabel::linkActivated, this,
            [this](const QString &) { showLoginPage(); });
    // 上次登录成功过就预填账号并自动登录；失败时停留在此界面让用户改
    if (const LoginStore::Credentials creds = LoginStore::load();
        creds.autoLogin) {
        ui.account_line->setText(creds.username);
        ui.password_line->setText(creds.password);
        if (ui.hintLabel) {
            ui.hintLabel->setText(tr("正在自动登录…"));
        }
        QTimer::singleShot(config::ui::timing::login_auto_click_ms, this, &ui::LoginWindow::Login);
    } else if (!creds.username.isEmpty()) {
        ui.account_line->setText(creds.username);
    }

    connect(&m_nam, &QNetworkAccessManager::finished, this,
            [this](QNetworkReply *reply) {
                if (m_pendingRequest == PendingRequest::Register) {
                    onRegisterFinished(reply);
                } else {
                    onLoginFinished(reply);
                }
            });
}

void ui::LoginWindow::set_style() {
    ui::ThemeManager::instance().apply_to(this, QString::fromLatin1(config::paths::style::login));
}

void ui::LoginWindow::showRegisterPage() {
    ui.stackedWidget->setCurrentWidget(ui.registerPage);
    setWindowTitle(tr("创建账号"));
}

void ui::LoginWindow::showLoginPage() {
    ui.stackedWidget->setCurrentWidget(ui.loginPage);
    setWindowTitle(tr("登录"));
}

void ui::LoginWindow::Login() {
    if (m_requestInFlight) { //正在登录
        return;
    }

    const QString username = ui.account_line->text().trimmed();
    const QString password = ui.password_line->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Login Error"), tr("请输入账号和密码"));
        return;
    }

    const QUrl url = ui::auth_url(ClientConfig::instance().auth().login_path);

    // v2 规范（docs/schema/api.json）：请求体只带 data —— {"data":{"username","password"}}
    QJsonObject credentials;
    credentials.insert(QStringLiteral("username"), username);
    credentials.insert(QStringLiteral("password"), password);
    QJsonObject body;
    body.insert(QStringLiteral("data"), credentials);

    m_pendingRequest = PendingRequest::Login;
    m_requestInFlight = true;
    ui.login_button->setEnabled(false);
    spdlog::debug("[login] POST {}", qutf8(url.toString()));
    flush_log();
    m_nam.post(ui::auth_request(url),
               QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void ui::LoginWindow::Register() {
    if (m_requestInFlight) {
        return;
    }

    const QString username = ui.register_username_line->text().trimmed();
    const QString password = ui.register_password_line->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, tr("Register Error"), tr("请输入用户名和密码"));
        return;
    }

    const QUrl url = ui::auth_url(ClientConfig::instance().auth().register_path);

    // v2 规范：{"data":{"username","password"}}；成功同样返回 DataSession
    QJsonObject credentials;
    credentials.insert(QStringLiteral("username"), username);
    credentials.insert(QStringLiteral("password"), password);
    QJsonObject body;
    body.insert(QStringLiteral("data"), credentials);

    m_pendingRequest = PendingRequest::Register;
    m_requestInFlight = true;
    ui.register_button->setEnabled(false);
    spdlog::debug("[register] POST {}", qutf8(url.toString()));
    flush_log();
    m_nam.post(ui::auth_request(url),
               QJsonDocument(body).toJson(QJsonDocument::Compact));
}

void ui::LoginWindow::onLoginFinished(QNetworkReply *reply) {
    if (m_pendingRequest == PendingRequest::Register) {
        return;
    }

    m_requestInFlight = false;
    m_pendingRequest = PendingRequest::None;
    ui.login_button->setEnabled(true);

    spdlog::debug("[login] onLoginFinished enter");
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
    spdlog::debug("[login] response bytes={}", payload.size());
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

    // v2：data = { user:{user_id,username,nickname,avatar,info,gender,birthday,
    //                    address,phone,email,extra_json}, token, turn }
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    const QJsonObject user = data.value(QStringLiteral("user")).toObject();
    if (user.isEmpty()) {
        spdlog::warn("[login] missing data.user in response");
        flush_log();
    }
    const qint64 userId = user.value(QStringLiteral("user_id"))
                              .toVariant()
                              .toLongLong();
    const QString username = user.value(QStringLiteral("username")).toString();
    const QString name = user.value(QStringLiteral("nickname")).toString();
    const QString avatar = user.value(QStringLiteral("avatar")).toString();
    const QString info = user.value(QStringLiteral("info")).toString();
    UserProfileExtras extras;
    extras.gender = user.value(QStringLiteral("gender")).toString();
    extras.birthday = user.value(QStringLiteral("birthday")).toString();
    extras.address = user.value(QStringLiteral("address")).toString();
    extras.phone = user.value(QStringLiteral("phone")).toString();
    extras.email = user.value(QStringLiteral("email")).toString();
    extras.extraJson = user.value(QStringLiteral("extra_json")).toString();

    UserSession::instance().setToken(
        data.value(QStringLiteral("token")).toString());
    UserSession::instance().setUser(
        userId,
        username.isEmpty() ? ui.account_line->text().trimmed() : username,
        name, avatar, info, extras);

    const QJsonObject turn = data.value(QStringLiteral("turn")).toObject();
    if (!turn.isEmpty()) {
        TurnCredentials creds;
        creds.username = turn.value(QStringLiteral("username")).toString();
        creds.password = turn.value(QStringLiteral("password")).toString();
        creds.ttlSeconds = turn.value(QStringLiteral("ttl")).toInt();
        const QJsonArray uris = turn.value(QStringLiteral("uris")).toArray();
        for (const QJsonValue &uri : uris) {
            if (uri.isString()) {
                creds.uris.push_back(uri.toString());
            }
        }
        UserSession::instance().setTurnCredentials(creds);
        spdlog::info("[login] turn credentials ttl={} uris={}", creds.ttlSeconds,
                     creds.uris.size());
    }
    spdlog::info("[login] success id={} name_bytes={}", userId,
                 name.toUtf8().size());

    // 记住本机：下次启动直接自动登录（「退出登录」会清除）
    LoginStore::save(ui.account_line->text().trimmed(),
                     ui.password_line->text());
    spdlog::info("[login] calling accept()");
    flush_log();
    accept();
    spdlog::info("[login] accept() returned");
    flush_log();
}

void ui::LoginWindow::onRegisterFinished(QNetworkReply *reply) {
    if (m_pendingRequest != PendingRequest::Register) {
        return;
    }

    m_requestInFlight = false;
    m_pendingRequest = PendingRequest::None;
    ui.register_button->setEnabled(true);

    const QString username = ui.register_username_line->text().trimmed();

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

    ///获取响应数据
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    ///解析响应数据
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        ///解析响应数据失败
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

    ui.register_password_line->clear();
    ui.account_line->setText(username);
    ui.password_line->clear();
    showLoginPage();
    QMessageBox::information(this, tr("Register Success"),
                             tr("注册成功，请使用新账号登录"));
}
