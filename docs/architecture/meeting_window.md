# 会议窗口（MeetingWindow）职责拆分

> 目标：窗口只做「持有模块指针 + 接线 + 生命周期」，业务与界面逻辑各自有归属。
> 本文记录拆分结果、模块边界、状态归属与验证方式。

## 1. 拆分结果（规模）

| 文件 | 最初 | 现在 |
|---|---|---|
| `client/src/ui/pages/meeting_window.cpp` | **1481 行** | **572 行** |
| `client/include/ui/pages/meeting_window.h` | **283 行** | **168 行** |
| 头文件成员函数 | 约 70 个 | 约 30 个 |

拆出去的模块：

| 模块 | 文件（.h + .cpp） | 行数 |
|---|---|---|
| `ConnectCoordinator` | `features/meeting/connect_coordinator.*` | 82 + 73 |
| `DeviceMenuController` | `features/meeting/device_menu_controller.*` | 39 + 147 |
| `MeetingTopBarController` | `features/meeting/meeting_top_bar_controller.*` | 47 + 231 |
| `MeetingMediaView` | `features/meeting/meeting_media_view.*` | 51 + 266 |
| `MeetingSessionController` | `features/meeting/meeting_session_controller.*` | 45 + 315 |
| `meeting::MeetingState` | `features/meeting/meeting_state.h` | 32 |
| `IMeetingWindowHost` | `features/meeting/meeting_window_host.h` | 66 |
| `ui::apply_toggle_icon` | `ui/composites/meeting_toggle_icon.*` | 18 + 22 |

## 2. 现在的职责分布

```
MeetingWindow（装配根，572 行）
├── 持有 MeetingWindowUi（视图控件）
├── 持有 app::MeetingSession（网络/线程/控制器装配）
├── 持有各功能模块指针：
│   ├── ConnectCoordinator        连接挂起/串行化
│   ├── DeviceMenuController      设备选择菜单
│   ├── MeetingTopBarController   顶栏状态/时长/详情弹窗/标签栏
│   ├── MeetingMediaView          媒体按钮/媒体生命周期/帧路由
│   └── MeetingSessionController  建会入会结果/结束/复位
├── 实现 IMeetingWindowHost（给控制器读状态与装配对象）
└── 负责：init_ui 布局、init_connect 接线、少量转发槽、窗口生命周期
```

## 3. 各模块边界

### 3.1 ConnectCoordinator

- 持有：`_connecting`、`_hasPendingConnect`、挂起参数（ip/port/action/room/人数/时长）。
- 行为：请求连接时若正忙则暂存；`on_connected()` 后再补发；结束后按动作发出建会/入会。
- 信号：`connect_requested / connect_finished / create_requested / join_requested`。
- 窗口只转发 `connect_finished` 的界面副作用。

### 3.2 DeviceMenuController

- 设备枚举、勾选状态、选择后调用 `MeetingMediaController::select_*_device`。
- 负责小箭头朝向（菜单开启向下、关闭向上）。
- 窗口两个按钮槽只是转发。

### 3.3 MeetingTopBarController

- `refresh_meeting_info()`：状态胶囊、房间号、人数、服务端地址。
- `refresh_duration()/start_duration()/stop_duration()`：每秒刷新的会议时长。
- `refresh_speaker()`：讲话者标签。
- `show_info_popup()`：「会议详情」弹层（懒创建）。
- `apply_tab_bar_visibility()`、`set_main_screen_title()`。
- `format_duration_hms()` 静态纯函数（单测入口）。

### 3.4 MeetingMediaView

- `refresh_buttons()`：工具栏可用性/文案/图标/共享权限。
- `set_video_on/set_audio_on/set_screen_on`、`start_media()/stop_media()`。
- xrtc 回调：进房成功/失败、信令错误、连接丢失、远端离开。
- 帧路由：本地预览/远端帧的渲染与录制推帧、启动耗时结算。
- `send_local_user_profile()`。

### 3.5 MeetingSessionController

- `on_create_result/on_join_result`：建会/入会成功后的成员初始化、计时启动、房间设置激活。
- `end_session(was_connecting)`：结算时长（发 `meeting_closed`）、停媒体、复位、断网。
- `reset_ui()`：工具栏/面板复位。
- `wire_settings_panel()`：房间设置面板 + 录制器信号接线。
- 信号：`meeting_confirmed / meeting_closed / session_ending_finished`。

### 3.6 窗口剩余逻辑（装配根应当保留的）

| 内容 | 说明 |
|---|---|
| `init_ui()` | 视图布局、QSS 应用、几何尺寸、初始禁用态 |
| `init_connect()` | 网络/控制器/连接协调器/工具栏按钮的 `connect` |
| `init_controllers_wiring()` | 成员/协议事件 → 各模块的接线 |
| 转发槽 | 设备菜单、媒体开关、聊天发送、侧栏切换、房间号校验 |
| `close_meeting_window()` | 协调器取消挂起 + 会话结束 + 隐藏 |
| 宿主访问器 | `network()/participants()/media()/...` |

## 4. 设计规则

1. **状态单一归属**：`meeting::MeetingState` 由窗口持有；控制器只读写必要字段。
2. **判据**："不碰 `ui_` 控件的方法就搬走"；窗口里的私有槽基本只剩转发。
3. **控制器不反向依赖窗口**：需要界面反馈时发信号（如 `MeetingMediaController::notice`）。
4. **接线集中**：`init_connect()` 与 `init_controllers_wiring()` 两处。
5. **模块之间不直接互调**：由窗口接线（例如媒体视图不直接调用会话控制器）。

## 5. 验证

```powershell
# 编译
cmake --build build/client -j

# UI 自测（全部与实际改动相关）
UiPreview.exe --device-menu-selftest      # 设备菜单
UiPreview.exe --title-button-selftest     # 标题栏按钮
UiPreview.exe --duration-selftest         # 时长格式化
UiPreview.exe --main-hide-selftest        # 主窗口隐藏
UiPreview.exe --popup-rounding-selftest   # 弹窗圆角
UiPreview.exe --theme-selftest            # 主题
UiPreview.exe --icon-selftest             # 图标渲染
UiPreview.exe --video-backdrop-selftest   # 视频底色
UiPreview.exe --avatar-alpha-selftest     # 头像透明
UiPreview.exe --join-codec-selftest       # 房间号编解码
UiPreview.exe --resource-selftest         # 资源路径
UiPreview.exe --screen-preview-selftest   # 屏幕预览尺寸
UiPreview.exe --storage-selftest          # 存储
UiPreview.exe --unified-storage-selftest  # 统一存储
UiPreview.exe --resize-cost-selftest      # 缩放开销
UiPreview.exe --window-theme-selftest     # 窗口主题

# 截图回归
UiPreview.exe --theme dark  <输出目录>
UiPreview.exe --theme light <输出目录>
```

> 本次拆分后 `05-meeting.png` 的 sha256 与拆分前完全一致（`8fa42fb3…`），说明界面无任何变化。

### 已知的既有失败（与拆分无关，基线上同样失败）

- `--general-settings-selftest`（录制保存目录解析）
- `--login-store-selftest`（凭据保存）
- `--meeting-history-selftest`（历史库在无 QCoreApplication 场景下记录数为 0）
