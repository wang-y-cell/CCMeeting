#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QString>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#include <string>

#include "configure/user_session.h"
#include "configure/client_config.h"
#include "login.h"
#include "main_window.h"
#include "screen.h"

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

void qt_message_handler(QtMsgType type, const QMessageLogContext &ctx,
                        const QString &msg) {
    const std::string text = qutf8(msg);
    const char *file = ctx.file ? ctx.file : "";
    switch (type) {
    case QtDebugMsg:
        spdlog::debug("[Qt] {} ({}:{})", text, file, ctx.line);
        break;
    case QtInfoMsg:
        spdlog::info("[Qt] {} ({}:{})", text, file, ctx.line);
        break;
    case QtWarningMsg:
        spdlog::warn("[Qt] {} ({}:{})", text, file, ctx.line);
        break;
    case QtCriticalMsg:
        spdlog::error("[Qt] {} ({}:{})", text, file, ctx.line);
        break;
    case QtFatalMsg:
        spdlog::critical("[Qt fatal] {} ({}:{})", text, file, ctx.line);
        flush_log();
        break;
    }
    flush_log();
}

void on_terminate() {
    try {
        if (auto ep = std::current_exception()) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception &ex) {
                spdlog::critical("[terminate] uncaught exception: {}", ex.what());
            } catch (...) {
                spdlog::critical("[terminate] uncaught non-std exception");
            }
        } else {
            spdlog::critical("[terminate] std::terminate called");
        }
    } catch (...) {
    }
    flush_log();
    std::abort();
}

}  // namespace

int main(int argc, char *argv[]) {
    std::set_terminate(on_terminate);

    QApplication app(argc, argv);
    qInstallMessageHandler(qt_message_handler);

    // applicationDirPath 在部分启动方式下可能为空，用 argv[0] 兜底
    QString app_dir = QCoreApplication::applicationDirPath();
    if (app_dir.isEmpty() && argc > 0) {
        app_dir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
    }
    if (app_dir.isEmpty()) {
        app_dir = QDir::currentPath();
    }
    const QString log_path = QDir(app_dir).filePath(QStringLiteral("log.txt"));
    try {
        // GUI 子系统下不要挂 stdout_color_sink：Windows 控制台写 UTF-8 中文
        // 易触发 0xC0000005，表现为登录成功后闪退。
        const QByteArray log_path_local = QDir::toNativeSeparators(log_path).toLocal8Bit();
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            std::string(log_path_local.constData(),
                        static_cast<std::size_t>(log_path_local.size())),
            true);
        auto logger = std::make_shared<spdlog::logger>("CloudMeeting", file_sink);
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::debug);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [tid:%t] %v");
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_level(spdlog::level::debug);
    } catch (const std::exception &ex) {
        fprintf(stderr, "spdlog init failed: %s\n", ex.what());
    }

    spdlog::info("[main] ===== CloudMeeting start =====");
    spdlog::info("[main] exe dir={}", qutf8(app_dir));
    spdlog::info("[main] log file={}", qutf8(log_path));
    spdlog::info("[main] cwd={}", qutf8(QDir::currentPath()));
    spdlog::info("[main] quitOnLastWindowClosed(initial)={}",
                 app.quitOnLastWindowClosed());
    flush_log();

    QFile style_file(":/Style/source/style.qss");
    if (style_file.open(QFile::ReadOnly)) {
        spdlog::info("[main] style.qss loaded");
        QString style_sheet = QLatin1String(style_file.readAll());
        app.setStyleSheet(style_sheet);
        style_file.close();
    } else {
        spdlog::warn("[main] style.qss not found");
    }
    flush_log();

    spdlog::info("[main] Screen::init begin");
    flush_log();
    Screen::init();
    spdlog::info("[main] Screen::init done");
    flush_log();

    spdlog::info("[main] ClientConfig::load begin");
    flush_log();
    ClientConfig::instance().load();
    const auto &auth = ClientConfig::instance().auth();
    const auto &meeting = ClientConfig::instance().meeting_server();
    spdlog::info("[main] auth={}:{}{} meeting={}:{}", qutf8(auth.host),
                 auth.port, qutf8(auth.login_path), qutf8(meeting.host),
                 meeting.port);
    flush_log();

    // 登录框关闭时主窗口尚未显示；保持 false，避免 Qt 自动 quit
    app.setQuitOnLastWindowClosed(false);
    spdlog::info("[main] quitOnLastWindowClosed set false");
    flush_log();

    spdlog::info("[main] show login dialog");
    flush_log();
    login loginDialog;
    const int login_result = loginDialog.exec();
    spdlog::info("[main] login.exec returned={} (Accepted={})", login_result,
                 static_cast<int>(QDialog::Accepted));
    flush_log();

    if (login_result != QDialog::Accepted) {
        spdlog::warn("[main] login cancelled/rejected, exit 0");
        flush_log();
        spdlog::shutdown();
        return 0;
    }

    const auto &session = UserSession::instance();
    spdlog::info("[main] session loggedIn={} id={} name={}",
                 session.isLoggedIn(), session.userId(),
                 qutf8(session.name()));
    flush_log();

    spdlog::info("[main] constructing main_window ...");
    flush_log();
    std::unique_ptr<main_window> main_windowDialog;
    try {
        main_windowDialog = std::make_unique<main_window>();
        spdlog::info("[main] main_window constructed ok");
        flush_log();
    } catch (const std::exception &ex) {
        spdlog::critical("[main] main_window ctor exception: {}", ex.what());
        flush_log();
        return 1;
    } catch (...) {
        spdlog::critical("[main] main_window ctor unknown exception");
        flush_log();
        return 1;
    }

    spdlog::info("[main] main_window.show() begin");
    flush_log();
    main_windowDialog->show();
    main_windowDialog->raise();
    main_windowDialog->activateWindow();
    spdlog::info("[main] main_window.show() done visible={} topLevel={}",
                 main_windowDialog->isVisible(),
                 main_windowDialog->isTopLevel());
    flush_log();

    // 主窗口已显示后再允许“最后窗口关闭即退出”
    app.setQuitOnLastWindowClosed(true);
    spdlog::info("[main] quitOnLastWindowClosed set true, enter app.exec()");
    flush_log();

    const int ret = app.exec();
    spdlog::info("[main] app.exec returned {}", ret);
    flush_log();
    spdlog::shutdown();
    return ret;
}
