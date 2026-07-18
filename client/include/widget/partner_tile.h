#ifndef PARTNER_TILE_H
#define PARTNER_TILE_H

#include <QWidget>

class QLabel;
class Partner;

/**
 * @brief 会议成员左侧小窗 UI
 *
 * 负责布局、边框样式、点击/尺寸等交互事件；
 * 图像显示区域通过 Partner::displayLabel() 暴露给 CameraVideo。
 */
class PartnerTile : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief 构造小窗并关联成员
     * @param partner 用户信息对象（不可为空）
     * @param parent 父控件，一般为 scrollAreaWidgetContents
     */
    explicit PartnerTile(Partner *partner, QWidget *parent = nullptr);

    /** @brief 关联的成员 */
    Partner *partner() const { return m_partner; }
    /** @brief 视频/头像显示标签 */
    QLabel *displayLabel() const { return m_displayLabel; }

    /**
     * @brief 设置是否为主画面选中态
     * @param selected 选中时用红色边框
     */
    void setSelected(bool selected);
    /** @brief 恢复默认蓝色边框 */
    void resetBorder();

signals:
    /**
     * @brief 用户点击小窗
     * @param ip 成员 IP
     */
    void clicked(std::uint32_t ip);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLabelGeometry();
    void applyBorder(bool selected);

    Partner *m_partner = nullptr;
    QLabel *m_displayLabel = nullptr;
    int m_side = 40; ///< 正方形边长参考（宽高联动）
};

#endif // PARTNER_TILE_H
