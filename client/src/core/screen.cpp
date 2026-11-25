#include "core/screen.h"
#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <spdlog/spdlog.h>


int Screen::width = -1;
int Screen::height = -1;

void Screen::init() {
    /// 获得主显示器对象,也就是我的电脑频幕
    QScreen *s = QGuiApplication::primaryScreen();
    if (s == nullptr) {
        // 无头/远程会话下没有主屏；保持 -1，避免解引用空指针
        spdlog::warn("[Screen] 无主显示器，保持默认 {}x{}", Screen::width,
                     Screen::height);
        return;
    }

    Screen::width = s->geometry().width();
    Screen::height = s->geometry().height();
    spdlog::info("[Screen] 屏幕宽度: {} 屏幕高度: {}", Screen::width,
                 Screen::height);
}
