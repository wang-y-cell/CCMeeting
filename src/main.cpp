#include <QApplication>
#include "widget.h"
#include "screen.h"
#include "login.h"
#include "logger/Logger.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Logger::instance().setFile("log.txt", Logger::FileMode::Truncate);
    Logger::instance().setLevel(Logger::Level::Debug);

    Screen::init();

    login loginDialog;
    Widget w;
    loginDialog.show();
    if(loginDialog.exec() == QDialog::Accepted) {
        w.show();
        const int ret = app.exec();
        Logger::instance().shutdown();
        return ret;
    }
    return 0;
}
