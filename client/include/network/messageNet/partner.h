#ifndef PARTNER_H
#define PARTNER_H

#include <QWidget>
#include <cstdint>

class QLabel;

/**
 * @brief 会议成员小窗控件：显示远端视频/头像，点击可切换主画面
 */
class Partner : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造成员控件
     * @param parent 父控件
     * @param ip 成员 IP（主机序）
     */
    Partner(QWidget *parent = nullptr, std::uint32_t ip = 0);
    /**
     * @brief 获取成员 IP
     * @return IP（主机序）
     */
    std::uint32_t getIp() const { return ip; }
    /**
     * @brief 获取用于显示图像的标签
     * @return 显示用 QLabel
     */
    QLabel *displayLabel() const { return m_displayLabel; }

signals:
    /**
     * @brief 用户点击该成员时发出
     * @param ip 成员 IP
     */
    void sendip(std::uint32_t);

protected:
    /**
     * @brief 鼠标按下：发出 sendip
     * @param ev 鼠标事件
     */
    void mousePressEvent(QMouseEvent *ev) override;
    /**
     * @brief 尺寸变化时更新标签几何
     * @param event 尺寸事件
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    /** @brief 按当前控件尺寸更新显示标签几何 */
    void updateLabelGeometry();

    std::uint32_t ip = 0; ///< 成员 IP
    QLabel *m_displayLabel = nullptr; ///< 视频/头像显示标签
    int w = 40; ///< 默认宽度参考
};

#endif // PARTNER_H
