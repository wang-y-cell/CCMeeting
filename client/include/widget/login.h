#ifndef LOGIN_H
#define LOGIN_H

#include "frameless_window.h"

namespace Ui {
class login;
}

class login : public FramelessWindow<QDialog>
{
    Q_OBJECT
public:
    explicit login(QWidget *parent = nullptr);
    ~login();
    void Login();
    void set_style();

private:
    Ui::login *ui;
};

#endif // LOGIN_H
