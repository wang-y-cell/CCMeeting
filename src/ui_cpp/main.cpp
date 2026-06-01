#include <QApplication>
#include "screen.h"
#include "login.h"
#include "logger/Logger.h"
#include "main_window.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Logger::instance().setFile("log.txt", Logger::FileMode::Truncate);
    Logger::instance().setLevel(Logger::Level::Debug);

    Screen::init();

    login loginDialog;
    main_window main_windowDialog;
    loginDialog.show();
    if(loginDialog.exec() == QDialog::Accepted) {
        main_windowDialog.show();
        const int ret = app.exec();
        Logger::instance().shutdown();
        return ret;
    }
    return 0;
}
