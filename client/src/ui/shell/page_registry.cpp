#include "ui/shell/page_registry.h"

#include <algorithm>

#include <spdlog/spdlog.h>

namespace ui {

void PageRegistry::add(const PageMeta &meta, Factory factory) {
    if (meta.id.isEmpty() || !factory) {
        spdlog::warn("[PageRegistry] 忽略无效登记: id='{}' factory={}",
                     meta.id.toUtf8().constData(), static_cast<bool>(factory));
        return;
    }
    Entry entry;
    entry.meta = meta;
    entry.factory = std::move(factory);
    entry.index = m_next_index++;
    m_entries.insert(meta.id, std::move(entry));
}

QWidget *PageRegistry::create(const QString &id, QWidget *parent) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) {
        spdlog::warn("[PageRegistry] 未登记的页面: {}", id.toUtf8().constData());
        return nullptr;
    }
    if (it->instance != nullptr) {
        return it->instance;
    }
    it->instance = it->factory(parent);
    if (it->instance == nullptr) {
        spdlog::error("[PageRegistry] 页面构造失败: {}", id.toUtf8().constData());
        return nullptr;
    }
    // 页面自己没设置对象名时，用 id 兜底，方便 QSS 用 #id 作用域
    if (it->instance->objectName().isEmpty()) {
        it->instance->setObjectName(id);
    }
    spdlog::debug("[PageRegistry] 已创建页面: {}", id.toUtf8().constData());
    return it->instance;
}

QWidget *PageRegistry::find(const QString &id) const {
    const auto it = m_entries.find(id);
    return (it == m_entries.end()) ? nullptr : it->instance;
}

QList<PageMeta> PageRegistry::all() const {
    QList<Entry> entries = m_entries.values();
    std::sort(entries.begin(), entries.end(),
              [](const Entry &lhs, const Entry &rhs) {
                  if (lhs.meta.order != rhs.meta.order) {
                      return lhs.meta.order < rhs.meta.order;
                  }
                  return lhs.index < rhs.index;
              });
    QList<PageMeta> metas;
    metas.reserve(entries.size());
    for (const Entry &entry : entries) {
        metas.append(entry.meta);
    }
    return metas;
}

QList<PageMeta> PageRegistry::nav() const {
    QList<PageMeta> metas;
    for (const PageMeta &meta : all()) {
        if (meta.in_nav) {
            metas.append(meta);
        }
    }
    return metas;
}

void PageRegistry::clear() {
    m_entries.clear();
    m_next_index = 0;
}

}  // namespace ui
