# 会议窗口（MeetingWindow）职责拆分

> 目标：窗口类只做「创建子对象 + 接线 + 生命周期」，业务与状态各自有归属。
> 本文记录拆分结果、每个类的边界，以及后续还可以继续拆的部分。

## 1. 拆分前后的规模

| 文件 | 拆分前 | 拆分后 |
|---|---|---|
| `client/src/ui/pages/meeting_window.cpp` | 1481 行 | **约 1220 行** |
| `client/include/ui/pages/meeting_window.h` | 283 行 | **241 行** |
| 新增 `features/meeting/connect_coordinator.*` | — | 155 行 |
| 新增 `features/meeting/device_menu_controller.*` | — | 186 行 |

头文件里的成员函数从约 70 个降到约 55 个，状态字段收敛进 `MeetingState`。

## 2. 现在的职责分布

| 类 / 文件 | 职责 | 归属层 |
|---|---|---|
| `ui::MeetingWindow` | 持有子视图（`MeetingWindowUi`）、把信号接到控制器、窗口生命周期、弹窗/提示 | `ui/pages` |
| `MeetingState`（头文件内 struct） | 会话状态：是否创建/加入、本端音视频/共享、房间号、服务端地址、会议与启动计时 | `ui/pages` |
| `app::MeetingSession` | 装配层：网络、工作线程、控制器、协议分发的唯一创建者与析构者 | `app` |
| `ConnectCoordinator`（新增） | 「连接服务器 → 建会/入会」的**串行化与挂起**：正在连接/正在结束时暂存参数，结束后补发 | `features/meeting` |
| `DeviceMenuController`（新增） | 设备选择菜单：设备枚举、勾选状态、选择后调用媒体控制器、小箭头朝向 | `features/meeting` |
| `ParticipantsController` | 成员卡片、主画面选择、`@` 补全标签、人数变化 | `features/meeting` |
| `ChatController` | 气泡渲染、时间分隔、@ 提示音、待确认发送状态 | `features/meeting` |
| `RoomSettingsController` | 房间开关面板、房主判定、录制按钮与面板计时 | `features/meeting` |
| `MeetingMediaController` | xrtc 引擎生命周期、本端音视频/屏幕共享、设备切换 | `features/meeting` |
| `MeetingProtocolDispatcher` | 网络消息 → 语义信号（"发生了什么"） | `features/meeting` |
| `MeetingRtcObserver` | xrtc 回调 → Qt 信号 | `features/meeting` |
| `MeetingRecorder` | 主画面录制与 MP4 封装 | `media/recording` |

## 3. 本次拆出的两个类

### 3.1 ConnectCoordinator

**拆分原因**：窗口原先自己维护 `_connecting`、`_hasPendingConnect`、`_pendingConnectIp/Port/Action/RoomNo`、
`_pendingMaxParticipants`、`_pendingDurationMinutes` 共 8 个字段，并在两个槽里重复
「正在连接就暂存、结束后补发」的逻辑；任何新入口（首页、设置弹窗、断线重连）都要复制这段。

**现在的接口**：

```cpp
coordinator->request(ip, port, action, room_no, max_participants, duration_minutes);
coordinator->on_connected(ok, ip, port, action);   // 由窗口在控制器信号里转调
coordinator->set_session_ending(true/false);       // 会话结束时禁止补发
coordinator->flush_pending();                      // 断线恢复后补发
coordinator->cancel_pending();                     // 关会时丢弃
```

信号：`connect_requested`、`connect_finished`、`create_requested`、`join_requested`。
窗口只处理 `connect_finished` 的界面副作用（失败关会、更新状态）。

### 3.2 DeviceMenuController

**拆分原因**：`show_audio_device_menu()` + `show_video_device_menu()` 两个方法合计约 105 行，
是纯界面拼装 + 设备枚举，与窗口业务无关。

**现在的接口**：

```cpp
DeviceMenuController *menus = new DeviceMenuController(media(), this);
menus->bind_buttons(ui.audioDeviceBtn, ui.videoDeviceBtn);
menus->show_audio_menu();   // 扬声器 + 麦克风
menus->show_video_menu();   // 摄像头
```

按钮槽 `on_audio_device_btn_clicked_slot()` / `on_video_device_btn_clicked_slot()` 只做转发。

## 4. 改造原则（后续继续拆时沿用）

1. **状态只有一个归属**：数据要么在 `MeetingState`，要么在对应控制器内部；不允许两处各存一份。
2. **判据：不碰 `ui.` 控件的方法就该搬走**。剩下在窗口里的私有槽，绝大多数只是转调。
3. **控制器不反向依赖窗口**：需要界面反馈时发信号（如 `MeetingMediaController::notice`），由窗口决定怎么弹。
4. **窗口只做接线**：`init_controllers_wiring()` 集中所有 `connect`，构造函数只负责创建顺序。
5. **一次搬一块 + 截图/自测回归**：每步都能编译、能跑 `UiPreview` 自测。

## 5. 还可以继续拆的部分（按收益排序）

| 待拆内容 | 现在位置 | 建议去往 | 预计减少 |
|---|---|---|---|
| 顶栏与信息面板（时长、会议详情弹窗、成员数、讲话者） | `update_meeting_info/refresh_duration_chip/show_meeting_info_popup/update_speaker_label/apply_tab_bar_visibility` | 新增 `MeetingTopBarController` | 约 200 行 |
| 建会/入会结果处理（状态机跳转、计时启动、房间设置激活） | `on_create_meeting_result/on_join_meeting_result` | 新增 `MeetingSessionController` | 约 80 行 |
| 媒体包装方法（start/stop、set_local_*、帧回调、rtc 观察者转调） | 窗口内 | 收敛进 `MeetingMediaController`（帧渲染保留在窗口） | 约 150 行 |
| 聊天发送适配（network→chat 的收发协调） | `on_text_received/on_send_msg_clicked_slot` | `ChatController` + 发送回调 | 约 60 行 |

## 6. 验证方式

```powershell
# 编译
cmake --build build/client -j

# 关键自测（本次回归覆盖）
build\client\client\UiPreview.exe --device-menu-selftest
build\client\client\UiPreview.exe --title-button-selftest
build\client\client\UiPreview.exe --duration-selftest
build\client\client\UiPreview.exe --main-hide-selftest
build\client\client\UiPreview.exe --popup-rounding-selftest

# 截图回归
build\client\client\UiPreview.exe --theme dark  <输出目录>
build\client\client\UiPreview.exe --theme light <输出目录>
```

> 已确认与本次改动无关的既有失败用例：`--general-settings-selftest`、
> `--login-store-selftest`、`--meeting-history-selftest`（在改动前的基线上同样 FAIL）。
