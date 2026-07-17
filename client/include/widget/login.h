#ifndef LOGIN_H
#define LOGIN_H

#include "frameless_window.h"

namespace Ui {
class login;
}

/**
 * @brief 登录对话框
 */
class login : public FramelessWindow<QDialog>
{
    Q_OBJECT
public:
    /**
     * @brief 构造登录对话框
     * @param parent 父控件
     */
    explicit login(QWidget *parent = nullptr);
    ~login();
    /** @brief 执行登录逻辑 */
    void Login();
    /** @brief 应用样式 */
    void set_style();

private:
    Ui::login *ui; ///< UI
};

#endif // LOGIN_H
