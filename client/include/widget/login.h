#ifndef LOGIN_H
#define LOGIN_H

#include "frameless_window.h"

#include <QNetworkAccessManager>

class QNetworkReply;

namespace Ui {
class login;
}

/**
 * @brief 登录对话框：通过 HTTP 调用认证服务校验账号
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

    /** @brief 发起登录请求 */
    void Login();
    /** @brief 应用样式 */
    void set_style();

private slots:
    /**
     * @brief 处理登录 HTTP 响应
     * @param reply 网络响应
     */
    void onLoginFinished(QNetworkReply* reply);

private:
    Ui::login *ui; ///< UI
    QNetworkAccessManager m_nam; ///< HTTP 客户端
    bool m_requestInFlight = false; ///< 防止重复提交
};

#endif // LOGIN_H
