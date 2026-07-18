#include <QApplication>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>
#include <QFile>

#include "screen.h"
#include "login.h"
#include "main_window.h"
#include "configure/user_session.h"

int main(int argc, char* argv[]) {

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>(); ///< 控制台带颜色输出
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true); ///< 文件输出
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("CloudMeeting", sinks.begin(), sinks.end()); ///< 创建名为CloudMeeting的logger,绑定上面两个sink
    logger->set_level(spdlog::level::debug); ///< 设置日志等级
    logger->flush_on(spdlog::level::info); ///< 设置日志等级为info时刷新日志
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [tid:%t] %v"); ///< 设置日志格式
    spdlog::set_default_logger(std::move(logger)); ///< 将刚刚创建的logger设置为全局默认
    spdlog::set_level(spdlog::level::debug); ///< 设置全局默认日志等级,确保全局默认也是debug

    QApplication app(argc, argv);
    QFile style_file(":/Style/source/style.qss");
    if(style_file.open(QFile::ReadOnly)) {
        spdlog::info("style.qss loaded");
        QString style_sheet = QLatin1String(style_file.readAll());
        app.setStyleSheet(style_sheet);
        style_file.close();
    } else {
        spdlog::warn("style.qss not found");
    }

    Screen::init();

    login loginDialog;
    main_window main_windowDialog;
    loginDialog.show();
    if(loginDialog.exec() == QDialog::Accepted) {
        const auto& session = UserSession::instance();
        spdlog::info("logged in user id={} name={} avatar={} info={}",
                     session.userId(),
                     session.name().toStdString(),
                     session.avatar().toStdString(),
                     session.info().toStdString());
        main_windowDialog.show();
        const int ret = app.exec();
        spdlog::shutdown();
        return ret;
    }
    return 0;
}
