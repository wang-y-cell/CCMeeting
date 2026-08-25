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

//刷新日志
void flush_log() {
    if (spdlog::default_logger()) {
        spdlog::default_logger()->flush();
    }
}

void qt_message_handler(QtMsgType type, const QMessageLogContext &ctx,
                        const QString &msg) {
    //将 Qt 的 QString 转换为标准的 UTF-8 编码的 std::string，确保中文或其他多字节字符在 spdlog 中不会乱码
    const std::string text = qutf8(msg);
    //安全地获取触发日志的源码文件名。如果 ctx.file 为空（某些 Release 构建下 Qt 可能会剥离调试符号），则使用空字符串防止空指针崩溃
    const char *file = ctx.file ? ctx.file : "";
    //根据日志类型，使用不同的 spdlog 级别记录日志
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
    //当 Qt 遇到致命错误（如 qFatal()）时，Qt 默认会直接调用 abort() 终止程序。这里在记录 critical 日志后，额外调用了一次 flush_log()
        spdlog::critical("[Qt fatal] {} ({}:{})", text, file, ctx.line);
        flush_log();
        break;
    }
    //全局 flush_log()：在 switch 语句结束后，无论什么级别的日志，都会调用 flush_log()。这确保了日志被立即写入磁盘。因为程序可能在下一条指令就崩溃了，如果不强制刷盘，日志可能会残留在内存缓冲区中而丢失
    flush_log();
}

void on_terminate() {
    try {
        //当程序触发 std::terminate 时，通常是因为有一个异常没有被任何 catch 块捕获
        //std::current_exception() 会尝试捕获这个“逃逸”的异常，并将其封装为 std::exception_ptr（智能指针）
        if (auto ep = std::current_exception()) {
            try {
                //重新抛出异常
                std::rethrow_exception(ep);
            } catch (const std::exception &ex) {
                //如果抛出的是标准错误,则提取其 what() 错误信息，并使用 spdlog 以最高级别（critical）记录日志
                spdlog::critical("[terminate] uncaught exception: {}", ex.what());
            } catch (...) {
                //如果抛出的是非标准错误,则使用 spdlog 以最高级别（critical）记录日志
                spdlog::critical("[terminate] uncaught non-std exception");
            }
        } else {
            //如果 std::current_exception() 返回空，说明程序崩溃不是因为普通的未捕获异常，而是触发了其他致命错误（如栈溢出、双重异常等）。此时记录一条通用的 terminate 日志
            spdlog::critical("[terminate] std::terminate called");
        }
    } catch (...) {
        //最外层的 catch (...)：这是一个极其重要的安全兜底。在 on_terminate 函数内部，如果 std::rethrow_exception 再次抛出了异常，或者 spdlog 记录日志时发生了崩溃，为了避免无限递归导致栈溢出，最外层的 catch 会默默吞掉这个新异常
    }
    flush_log();
    //最后，强制终止程序，并生成核心转储文件（Core Dump），方便后续通过调试器（如 GDB）分析崩溃现场
    std::abort();
}

}  // namespace

int main(int argc, char *argv[]) {
    //在程序因为未捕获的异常而即将崩溃（调用 std::terminate）时，
    //拦截这个异常，提取出有用的错误信息并记录到日志中，最后再安全地终止程序
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
        //QDir::toNativeSeparators(log_path)将路径中的分隔符转换为当前操作系统使用的“原生分隔符
        //.toLocal8Bit()将 Qt 的 QString（内部是 Unicode/UTF-16 编码）转换为当前操作系统本地编码的 QByteArray
        const QByteArray log_path_local = QDir::toNativeSeparators(log_path).toLocal8Bit();
        //basic_file_sink_mt：mt 代表 Multi-Thread。它内部使用了互斥锁（std::mutex），确保在多线程环境下写入日志是安全的
        //第二个参数 true：表示截断模式（Truncate）。每次程序启动时，会清空旧的日志文件从头写入。如果改为 false，则是追加模式（Append），日志会保留在文件末尾

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            //QByteArray 可能包含 \0 字符，或者为了绝对安全，直接使用 constData() 配合显式的 size() 来构造 std::string
            std::string(log_path_local.constData(),
                        static_cast<std::size_t>(log_path_local.size())),
            true);
        auto logger = std::make_shared<spdlog::logger>("CloudMeeting", file_sink);
        logger->set_level(spdlog::level::debug);
        //只要遇到 debug 或更高级别的日志，就立即将内存缓冲区的数据刷入磁盘。这保证了程序崩溃时日志不丢失，但在高频日志输出时会有一定的 I/O 性能损耗
        logger->flush_on(spdlog::level::debug);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%l] [tid:%t] %v");
        //将局部创建的 logger 所有权转移给 spdlog 的全局默认管理器，避免不必要的智能指针拷贝开销
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
