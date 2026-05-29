#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QWidget>
#include "widget.h"

namespace Ui {
class main_window;
}

class main_window : public QWidget
{
    Q_OBJECT

public:
    explicit main_window(QWidget *parent = nullptr);
    ~main_window();

private slots:
    void CreateMeeting_button_clicked();

private:
    Ui::main_window *ui;
    Widget *widget;
};

#endif // MAIN_WINDOW_H
