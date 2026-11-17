#ifndef CONFIGURE_PREFERENCES_H
#define CONFIGURE_PREFERENCES_H

#include <QString>
#include <QVariant>

/**
 * @brief 偏好读写接口（界面层唯一可见的偏好入口）
 *
 * 背景：界面层原先直接调 `Storage::instance()`，等于让 UI 知道
 * SQLite 连接、打开失败时的降级策略、GUI 线程约束这些实现细节；
 * 换存储、加缓存、写测试都无从下手。
 *
 * 现在界面只依赖本接口：
 *   - 生产实现 `StoragePreferences` 委托给 `Storage`（SQLite 单一后端）
 *   - 测试/预览工具可 `set_preferences()` 注入内存实现，不碰真实磁盘
 *
 * 线程约束：与 `Storage` 相同，只允许在 GUI 线程访问。
 */
class IPreferences {
public:
    virtual ~IPreferences() = default;

    /** @brief 读取偏好（不存在返回 def） */
    virtual QVariant value(const QString &key,
                           const QVariant &def = QVariant()) const = 0;
    /** @brief 写入偏好（立即提交） */
    virtual void set_value(const QString &key, const QVariant &value) = 0;
    /** @brief 删除某个偏好 */
    virtual void remove(const QString &key) = 0;
};

/** @brief 全局偏好入口 */
IPreferences &preferences();

/**
 * @brief 替换全局实现（测试/预览工具用）
 *
 * @param impl 新实现；传 nullptr 恢复默认的 Storage 实现。
 *             调用方负责保证 impl 的生命周期覆盖使用期。
 */
void set_preferences(IPreferences *impl);

#endif  // CONFIGURE_PREFERENCES_H
