#include "screen.h"
#include <QGuiApplication>
#include <QApplication>
#include <QScreen>
#include <QDebug>
#include "logger/Logger.h"

int Screen::width = -1;
int Screen::height = -1;

void Screen::init()
{
    //获得主显示器对象,也就是我的电脑频幕
    QScreen *s = QGuiApplication::primaryScreen();

    Screen::width = s->geometry().width();
    Screen::height = s->geometry().height();
    LOG_INFO("Screen", "屏幕宽度: " << Screen::width << " 屏幕高度: " << Screen::height);
}
