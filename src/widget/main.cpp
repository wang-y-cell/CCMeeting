#include <QApplication>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

#include "screen.h"
#include "login.h"
#include "main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("CloudMeeting", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::debug);
    logger->flush_on(spdlog::level::info);
    logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [tid:%t] %v");
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::debug);

    Screen::init();

    login loginDialog;
    main_window main_windowDialog;
    loginDialog.show();
    if(loginDialog.exec() == QDialog::Accepted) {
        main_windowDialog.show();
        const int ret = app.exec();
        spdlog::shutdown();
        return ret;
    }
    return 0;
}
