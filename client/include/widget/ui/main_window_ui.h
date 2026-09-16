#ifndef MAIN_WINDOW_UI_H
#define MAIN_WINDOW_UI_H

class QLabel;
class QListWidget;
class QStackedWidget;
class QWidget;

/** 主窗口纯 UI，不含业务逻辑 */
class Ui_main_window {
public:
    QWidget *userCard = nullptr;
    QLabel *avatarLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QListWidget *sideNav = nullptr;
    QStackedWidget *contentStack = nullptr;

    void setupUi(QWidget *parent);
};

#endif // MAIN_WINDOW_UI_H
