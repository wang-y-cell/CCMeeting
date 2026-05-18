#include <QApplication>
#include "widget.h"
#include "screen.h"
#include "logger/Logger.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    Logger::instance().setFile("log.txt", Logger::FileMode::Truncate);
    Logger::instance().setLevel(Logger::Level::Debug);

    Screen::init();

    Widget w;
    w.show();
    const int ret = app.exec();
    Logger::instance().shutdown();
    return ret;
}
