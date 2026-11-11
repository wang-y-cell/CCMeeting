# client/src/ui —— UI 层目录约定

UI 代码集中在这里，按"依赖单向"分层。新增界面时按下面的规则放文件，
不要往 `src/widget/` 里再加东西（那是旧目录，正在逐步迁出）。

## 目录结构

```
include/ui/                      源文件镜像（保持项目 include/ + src/ 分离的习惯）
├── foundation/                  ① 设计底座（不依赖任何上层）
│   ├── design_tokens.h          颜色/圆角/间距/控件尺寸/字号（唯一常量来源）
│   └── theme_manager.h          令牌替换 + QSS 加载（@primary 这类占位符）
├── pages/                       ③ 页面（有业务语义）
│   ├── page.h                   页面接口：page_id/title/icon/nav_order/on_show/on_hide
│   └── join_meeting_page.h      已迁移示例
└── shell/                       ④ 外壳（窗口与导航）
    └── page_registry.h          页面注册表：元数据 + 懒创建 + 排序

src/ui/
├── foundation/theme_manager.cpp
├── pages/join_meeting_page.cpp
├── shell/page_registry.cpp
└── theme/*.qss                  样式源码（通过 resource.qrc 的 alias 暴露为
                                 :/Style/source/xxx.qss，调用点无需改路径）
```

依赖方向（违反即视为设计错误）：

```
foundation  →  primitives  →  composites  →  pages  →  shell
```

- `foundation` 不 include 任何上层头文件
- `pages` 可以依赖 `foundation`（令牌）与 `composites`，但**不许**直接 new 网络/媒体对象
- `shell` 只通过 `PageRegistry` 认识页面，不 include 具体页面实现（业务装配除外）

## 新增一个页面（三步）

1. 写页面类，继承 `ui::Page`：

```cpp
// include/ui/pages/history_page.h
#include "ui/pages/page.h"

namespace ui {
class HistoryPage : public Page {
    Q_OBJECT
public:
    explicit HistoryPage(QWidget *parent = nullptr);
    QString page_id() const override { return QStringLiteral("history"); }
    QString title() const override { return tr("会议记录"); }
    QString icon_name() const override { return QStringLiteral("calendar"); }
    int nav_order() const override { return 20; }
    void on_show() override;   // 进页面才拉数据
signals:
    void openMeetingRequested(int room_no);
};
}  // namespace ui
```

2. 在 `main_window::register_pages()` 里登记一行（按 `nav_order` 自动排序）：

```cpp
m_pages.add({QStringLiteral("history"), tr("会议记录"),
             QStringLiteral("calendar"), 20, true},
            [this](QWidget *) { return new ui::HistoryPage(this); });
```

3. 需要独立样式时，在 `src/ui/theme/pages/history.qss` 写，并用
   `ui::ThemeManager::instance().apply_to(this, ":/Style/ui/theme/pages/history.qss")` 应用；
   小页面直接复用 `main_window.qss` 的全局选择器即可。

## 样式写法（必须）

- 颜色/圆角/间距**不要写死**，QSS 里用令牌占位符：

```css
QPushButton#joinMeeting_btn {
    background: @primary;
    border-radius: @radiusSm;
    color: @text;
    min-height: @controlH;
}
```

- 加载时由 `ThemeManager` 替换成当前主题的值（`ui/theme/dark`、`ui/theme/light`）。
- C++ 自绘控件直接用 `ui::tokens::kDark.primary` 等字段。

## 从旧目录迁移一个页面的检查清单

- [ ] 控件 objectName 与旧实现保持一致（QSS 选择器依赖它）
- [ ] 信号改名后更新 `main_window` 的连接
- [ ] `main_window` 里不再保留该页的类型指针（除非需要业务装配）
- [ ] 删除旧文件：`include/widget/...`、`src/widget/...`、`src/widget/ui/` 下的 `*_ui.cpp`
- [ ] `cmake --build` + `UiPreview.exe` 截图对比（同名场景，例如 `03-main-join`）

## 已完成 / 待迁

**页面（全部完成）**：`src/ui/pages/` 下 6 个页面全部继承 `ui::Page`，由
`PageRegistry` 登记，外壳只通过 `on_show()` 驱动刷新。

| 页面 | 类 | on_show 行为 |
|---|---|---|
| 创建会议 | `ui::CreateMeetingPage` | 刷新「最近创建的会议」 |
| 加入会议 | `ui::JoinMeetingPage` | 刷新「最近加入的会议」 |
| 会议记录 | `ui::MeetingHistoryPage` | `refresh()` |
| 录制 | `ui::RecordingsPage` | `refresh()` |
| 设置 | `ui::SettingsPage` | 设备枚举（延后 50ms）+ 预览同步 |
| 个人资料 | `ui::UserProfilePage` | 无（数据在刷新时拉取） |

**下一批（尚未迁移）**：

| 对象 | 现状 |
|---|---|
| 外壳 | `main_window` + `main_window_ui.*`（侧栏仍是手写按钮，未按注册表生成） |
| 登录窗口 | `login` + `login_ui.*` |
| 会议页 | `meeting_widget` + `meeting_widget_ui.*`（+ 6 个 controller 待移到 `src/features/`） |
| 对话框 | `edit_profile_dialog_ui.*`、`avatar_crop_dialog`、`recording_playback_dialog` |
## 主题（深色 / 浅色）

三件事配合工作，缺一不可：

1. **令牌层** `ui/foundation/design_tokens.h`
   两套调色板 `kDark` / `kLight`，字段顺序必须一致。
2. **样式层** `src/ui/theme/*.qss`
   只允许写 `@token`，禁止 hex 字面量（`--theme-selftest` 会校验）。
3. **应用层** `ui::ThemeManager`
   - `apply_global(path)`：全局样式（`style.qss`）
   - `apply_to(widget, path)`：窗口样式，同时**登记**到注册表
   - `set_theme(Theme)`：重刷全局 + 所有已登记窗口，并发出 `theme_changed()`

### 新增一个颜色

1. `design_tokens.h` 的 `Palette` 加字段，并在 `kDark`、`kLight` 的**相同位置**填值
2. `theme_manager.cpp` 的 `build_tokens()` 里加一行映射
3. QSS 里用 `@新字段名`

### 用户怎么切换

设置 → 通用 → 主题：`跟随系统 / 浅色 / 深色`，存在 `Storage` 的 `ui/theme`
（`system` / `light` / `dark`）。`system` 时监听 `QStyleHints::colorSchemeChanged` 自动跟随。

### 新增窗口时

用 `ui::ThemeManager::instance().apply_to(this, ":/Style/...")` 而不是
`setStyleSheet()`，否则切主题时不会重刷（`style_loader.h` 的老接口已改为委托，可继续用）。