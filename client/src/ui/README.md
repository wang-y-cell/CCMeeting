# client/src/ui —— UI 层目录约定

UI 代码集中在这里，按"依赖单向"分层。新增界面时按下面的规则放文件；
旧的 `src/widget/` 已并入 `ui/`（`foundation` / `composites` / `shell`），不要再建。

## 客户端整体分层

客户端按「基础设施 → 配置/数据 → 能力 → 装配 → 界面」单向分层：

```
ui（foundation → primitives → composites → pages → shell）
                        │
                        ▼
                  app/MeetingSession            ← 唯一装配点
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
  features/meeting   media/*        protocol/*
        │               │               │
        └───────────────┴───────────────┘
                        │
                 data/*  config/*  core/*
```

各层职责：

| 目录 | 放什么 | 不允许 |
|---|---|---|
| `core/` | 平台与存储基础设施：`screen`、`storage`（SQLite 单一后端） | 认识任何业务概念 |
| `config/` | 配置与会话：`client_config`、`auth_config`、`user_session`、`login_store`、`preferences`、`stdtypes` | include `ui/` |
| `data/` | 业务数据：`meeting_record`、`meeting_repository`、`meeting_history`（实现）、`recording_repository`、`recording_library`（实现） | 弹窗、控件 |
| `domain/` | 纯业务模型：`partner` | 依赖 UI |
| `protocol/` | 线上协议与传输：`connection`、`message(_hub/_codec)`、`net_header`、`network_manager`、`room_settings_flags` | include `ui/`、`media/` |
| `media/` | 图像/视频资源：`avatar_service`、`media/video/*`（样式/渲染/路由）、`media/recording/*` | 依赖 `ui/` 或 `protocol/` |
| `features/meeting/` | 8 个 controller（会议、聊天、成员、媒体、协议分发、RTC 观察、房间设置） | 自己 new 线程/网络 |
| `app/` | `MeetingSession`：创建并持有网络、工作线程、控制器、协议分发；`stop_workers()` 收尾 | 依赖具体页面 |
| `ui/` | 界面：foundation → primitives → composites → pages → shell / dialogs | include `core/storage.h`、`protocol/*`，或自己 new `NetworkManager`/`QThread` |

- 界面读写偏好只走 `config/preferences.h` 的 `preferences()`，不直接 `Storage::instance()`；
- 页面读写会议记录 / 录制库只走 `data/meeting_repository.h`、`data/recording_repository.h`；
- 三个接口都提供 `set_xxx(nullptr)` 形式的注入点，测试可替换成内存实现。

**头文件一律写全路径**（只有一个 include 根 `client/include/`）：

```cpp
#include "media/video/meeting_video.h"
#include "protocol/network_manager.h"
```

## 目录结构

```
include/ui/                      源文件镜像（保持项目 include/ + src/ 分离的习惯）
├── foundation/                  ① 设计底座（不依赖任何上层）
│   ├── design_tokens.h          颜色/圆角/间距/控件尺寸/字号（唯一常量来源）
│   ├── theme_manager.h          令牌替换 + QSS 加载（@primary 这类占位符）
│   ├── frameless_window.h       无边框窗口模板（原 widget/）
│   ├── window_effects.h         Win11 DWM 圆角 / Win10 遮罩回退
│   └── resource_paths.h         资源路径常量（Source::at_sound 等，原 configure.h）
├── primitives/                  ② 原子控件（PageHeader / Card / field_label）
├── composites/                  ③ 组合控件（无独立页面语义）
│   ├── partner_tile.h           成员卡片
│   ├── recent_meeting_list.h    最近会议列表
│   ├── chat_message.h           聊天气泡（原 message/text）
│   └── my_text_edit.h           带 @ 补全的输入框（原 message/text）
├── pages/                       ④ 页面（有业务语义）
│   ├── page.h                   页面接口：page_id/title/icon/nav_order/on_show/on_hide
│   └── join_meeting_page.h      示例页面
├── dialogs/                     对话框（编辑资料 / 头像裁剪 / 录制回放）
└── shell/                       ⑤ 外壳（窗口与导航）
    ├── main_window.h            主窗口（原 widget/main_window）
    └── page_registry.h          页面注册表：元数据 + 懒创建 + 排序

src/ui/                          与 include/ui/ 完全镜像，另有：
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
- [x] 旧目录已并入 `ui/{foundation,composites,shell}`（2026-11 完成）
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

**外壳 / 窗口 / 对话框（已完成）**：

| 对象 | 类 / 文件 | 说明 |
|---|---|---|
| 外壳 UI | `Ui_main_window` → `ui/shell/main_window_ui.*` | 侧栏条目由 `PageRegistry::nav()` 生成，UI 层不再知道有哪些页面 |
| 登录窗口 | `ui::LoginWindow` → `ui/pages/login_window.*` | 含 `LoginWindowUi` |
| 修改资料 | `ui::EditProfileDialog` → `ui/dialogs/edit_profile_dialog.*` | 含 `edit_profile_dialog_ui` |
| 头像裁剪 | `ui::AvatarCropDialog` / `ui::AvatarCropCanvas` → `ui/dialogs/` | |
| 录制回放 | `ui::RecordingPlaybackDialog` → `ui/dialogs/recording_playback_dialog.*` | |

**会议窗口 / 逻辑 / 基础层（已完成）**：

| 对象 | 现状 |
|---|---|
| 会议窗口 | `ui/pages/meeting_window.*`（`ui::MeetingWindow`）+ `meeting_window_ui.*` |
| 会议设置弹窗 | `ui/pages/meeting_settings_popup.*`（`ui::MeetingSettingsPopup`） |
| 会话逻辑 | 8 个 controller → `src/features/meeting/`，include 为 `features/meeting/*.h` |
| 基础控件 | `ui/primitives/`：`PageHeader`、`Card`、`make_field_label`（加入/创建页已采用） |
| 图标 | `resources/icons/*.svg`（13 个）+ `ui/foundation/icon_provider`（着色+缓存） |
| 字体 | `ui/foundation/font_registry`（扫描 `:/ui/fonts` 注册，目录空时安全跳过） |
| 截图回归 | `client/tools/image_diff.py` + UiPreview 固定时间种子（同版本连拍 0 差异） |

**仍可继续优化（非阻塞）**：

| 项 | 说明 |
|---|---|
| 其余页面改用语 primitives | settings / history / recordings / profile 的 `_ui.cpp` 仍手写布局 |
| 图标着色实时刷新 | 会议工具栏图标在窗口创建时着色，切换主题后需重建窗口才更新 |
| 字体文件 | `resources/fonts/` 仍为空，放入 ttf/otf 并在 qrc 登记后自动生效 |
| 像素基线 | 建议把 `ui-preview-dark` 作为基线目录纳入评审流程（目前未提交到仓库） |
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

## 重构记录（2026-11）

已完成：

- 会议窗口不再自建网络 / 线程 / 控制器，统一由 `app/MeetingSession` 装配与收尾；
- 界面层不再直接依赖 `Storage` 单例，改走 `preferences()`；
- 页面不再直接调 `MeetingHistory` / `RecordingLibrary`，改走
  `meeting_repository()` / `recording_repository()`；
- 删除 `NetworkManager` 的 snake_case ↔ camelCase 双份别名（每增加一个发送接口只需改一处）；
- UI 类命名统一为 `XxxUi`（`MainWindowUi` / `EditProfileDialogUi`），移除空转的 `AUTOUIC`；
- 全局 `struct state` 收进 `ui::MeetingState`；
- 新增 `client/tests/`（发送队列 / 编解码 / 协议分发）并接入 `ctest` 与 CI。

### 画面渲染（2026-11 第二轮回）

- 样式收敛到 `media/video/video_style.h`：`DrawMode` 唯一定义、`VideoStyle` 值对象、
  具名预设 `VideoStyles::video() / memberAvatar() / mainAvatar()`（原来枚举被定义两遍，
  靠 `static_cast<int>` 互转）；
- `ImgDisplay` 改为**无状态**入口：`ImgDisplay::show(widget, image, style)` /
  `show_video(widget, frame, style)` / `clear(widget)`，不再持控件、不再存样式，
  因此没有「先 setDrawMode 再 setFrame」的顺序依赖，也没有悬空指针；
- `VideoGLWidget` 只保留 `setStyle()` 一个样式入口（原 5 个 setter 合并），
  样式与帧互相独立，`setStyle` 有变化才重绘；
- `MeetingVideo`（当时叫 `CameraVideo`）用 `QPointer<VideoGLWidget>` 记录控件，删除 `detachFromWidgets()`
  手工解绑契约；同时删掉没人读的 `setLocalUserId()`；
- 成员小窗头像「72×72 居中」现在只有 `VideoStyles::memberAvatar()` 一个来源，
  消除了 `PartnerTile` 与本类两套样式互相覆盖的问题。

### 画面路由 API 收敛（2026-11 第四轮回）

- `CameraVideo` 改名 `MeetingVideo`（`media/video/meeting_video.h/.cpp`），
  公开方法 **13 → 7**：`bindMain` / `bind` / `show` / `setAvatar` /
  `clearVideo` / `setMainUser` / `clear`；
- 删除 `setAvatarUrlForUser` / `hasActiveVideo` / `refreshMainForUser` /
  `showAvatarForUser` / `showMainAvatar` / `showImageForUser` 等重复入口：
  「有帧重播、否则头像、否则清屏」的规则收进 `setMainUser` / `clearVideo`；
- 头像下载移到 `ParticipantsController`：一次请求（256px）、裁圆后同时交给
  卡片与 `MeetingVideo`，消除 `PartnerTile` 再下载一遍的重复请求；
- `PartnerTile` 不再依赖 `AvatarImageLoader`，新增 `showAvatar(image)`，
  `updateProfile()` 只改昵称；
- 删除未使用的 `ParticipantsController::main_user_id()` / `set_main_user_id()`；
- 行为微调：主画面用户离开时清空主画面（旧实现会继续显示他的默认头像）。

### 兼容层清理（2026-11 第三轮回）

删除所有「只为兼容历史而存在」的代码，只留一套实现：

- `widget/style_loader.h` 兼容包装删除，7 处调用点直接
  `ui::ThemeManager::instance().apply_to(...)`；
- `Storage` 只保留 SQLite：删除 QSettings 降级、注册表历史键迁移、
  `login/*` 残留清理、空实现 `sync()`；打开失败时本次运行不持久化；
- 删除旧数据目录迁移（`%LOCALAPPDATA%\CloudMeeting` → `CCMeeting`）；
- `MeetingHistory` 删除 JSON 存储路径与一次性导入，只剩 SQLite；
  记录类型拆到 `data/meeting_record.h`，工具/页面统一走 `meeting_repository()`；
- 协议删除 0–3 号旧媒体类型（`img_send` / `img_recv` / `audio_send` /
  `audio_recv`），客户端与服务端同步删除；
- `PageRegistry` 只接受 `ui::Page`，不再接受任意 QWidget；
- 加入会议页的 `objectName` 由 `stack_join_meet` 改为 `join`；
- 会议记录 SQL 增加 `id DESC` 次级排序，同一秒内的多条记录顺序稳定。

刻意保留（属于运行环境兼容，不是历史兼容）：Win10 圆角遮罩回退、
H.264 不可用时回退 MJPEG/Matroska、OpenGL 用 R8 单通道纹理。

仍待处理：

- 设置 / 会议记录 / 录制 / 资料四个页面仍是手写布局，尚未改用 `ui/primitives`；
- `ui/foundation` 里仍有少量历史命名（如 `frameless_window` 模板参数风格），可后续统一。