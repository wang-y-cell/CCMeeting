#ifndef PARTNER_H
#define PARTNER_H

#include <QObject>
#include <QString>
#include <cstdint>

class QLabel;
class PartnerTile;

/**
 * @brief 会议成员（用户信息与显示相关槽）
 *
 * 不负责 UI 布局与鼠标事件；左侧小窗由 PartnerTile 承担。
 * PartnerTile 通过本类提供的 displayLabel() / 选中样式槽来刷新画面外观。
 */
class Partner : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造成员数据对象
     * @param ip 成员 IP（主机序）
     * @param parent 父对象（一般挂在 Widget 上）
     */
    explicit Partner(std::uint32_t ip, QObject *parent = nullptr);

    /** @brief 成员 IP（主机序） */
    std::uint32_t ip() const { return m_ip; }
    /** @brief 兼容旧接口 */
    std::uint32_t getIp() const { return m_ip; }
    /** @brief IP 字符串，如 "192.168.1.1" */
    QString ipString() const;

    /**
     * @brief 绑定左侧小窗
     * @param tile 小窗控件，可为 nullptr（解绑）
     */
    void setTile(PartnerTile *tile);
    /** @brief 当前绑定的小窗 */
    PartnerTile *tile() const { return m_tile; }

    /**
     * @brief 供 CameraVideo 绑定的显示标签（来自 PartnerTile）
     * @return 无绑定时返回 nullptr
     */
    QLabel *displayLabel() const;

public slots:
    /**
     * @brief 设为当前主画面选中样式（高亮边框）
     * @param selected true 红色高亮，false 默认蓝边
     */
    void setSelected(bool selected);
    /** @brief 恢复默认边框样式 */
    void resetBorder();

signals:
    /**
     * @brief 用户点击对应小窗时转发（由 PartnerTile 触发）
     * @param ip 成员 IP
     */
    void clicked(std::uint32_t ip);

private:
    std::uint32_t m_ip = 0;
    PartnerTile *m_tile = nullptr; ///< 不拥有，由布局/父控件管理生命周期
};

#endif // PARTNER_H
