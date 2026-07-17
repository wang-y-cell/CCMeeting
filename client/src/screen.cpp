#include "screen.h"
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include <spdlog/spdlog.h>

int Screen::width = -1;
int Screen::height = -1;

void Screen::init()
{
    ///获得主显示器对象,也就是我的电脑频幕
    QScreen *s = QGuiApplication::primaryScreen();

    Screen::width = s->geometry().width();
    Screen::height = s->geometry().height();
    spdlog::info("[Screen] 屏幕宽度: {} 屏幕高度: {}", Screen::width, Screen::height);
}
