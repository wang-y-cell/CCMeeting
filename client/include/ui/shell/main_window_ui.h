#ifndef MAIN_WINDOW_UI_H
#define MAIN_WINDOW_UI_H

class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QWidget;

/** 主窗口纯 UI，不含业务逻辑 */
class MainWindowUi {
public:
    QWidget *userCard = nullptr;
    QLabel *avatarLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QPushButton *btnLogout = nullptr;
    QListWidget *sideNav = nullptr;
    QStackedWidget *contentStack = nullptr;

    void setupUi(QWidget *parent);
};

#endif // MAIN_WINDOW_UI_H
