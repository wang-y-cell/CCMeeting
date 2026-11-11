# client/resources —— 内容资源

除 QSS 之外的所有"内容资源"都放这里，并编译进 `resource.qrc`
（`client/resource.qrc`）。代码里统一用 `:/ui/...` 访问。

## 目录与访问路径

| 目录 | 类型 | 资源前缀 | 例子 |
|---|---|---|---|
| `resources/images/` | 位图/动画 | `/ui/images` | `:/ui/images/loading.gif` |
| `resources/audio/` | 音频 | `/ui/audio` | `qrc:/ui/audio/message.wav` |
| `resources/fonts/` | 字体 | `/ui/fonts` | `:/ui/fonts/inter-regular.ttf` |
| `src/ui/theme/` | QSS（代码性资源，不在本目录） | `/Style` | `:/Style/source/main_window.qss` |

## 新增一个资源（三步）

1. 把文件放进对应子目录，用**语义化 kebab-case 命名**
   （`loading.gif`、`message.wav`、`icon-user.svg`，不要 `1.png`、`新图片.png`）
2. 在 `client/resource.qrc` 里加一行：

```xml
<qresource prefix="/ui/images">
    <file alias="icon-user.svg">resources/images/icon-user.svg</file>
</qresource>
```

3. 代码里用 `:/ui/images/icon-user.svg`

## 规则

- **禁止相对路径**（`"./source/x.png"` 依赖当前工作目录，换启动方式即失效）
- **大文件不要进 qrc**（qrc 内容会进 exe 只读段，直接撑大安装包且更新要重发整包）：
  视频、帮助文档、可热更主题包 → 放安装目录 `assets/`，运行时用
  `QCoreApplication::applicationDirPath()` 拼路径
- **图标优先 SVG**（需要 `Qt6::Svg`，已在 CMake 链接；打包时确认
  `imageformats/qsvg.dll` 存在）
- **位图提供 @2x**：`logo.png` + `logo@2x.png`，避免高分屏发虚
- **字体**：放 `resources/fonts/`，并在 `main()` 早期注册后打印真实 family 名：

```cpp
const int id = QFontDatabase::addApplicationFont(
    QStringLiteral(":/ui/fonts/inter-regular.ttf"));
if (id >= 0) {
    qDebug() << QFontDatabase::applicationFontFamilies(id);
}
```

  真实 family 名可能与文件名不同，QSS 里的 `font-family` 必须用打印出来的名字。

## 运行时数据（不在这里）

| 数据 | 位置 |
|---|---|
| 数据库 / 日志 / 头像缓存 / 凭据 | `%LOCALAPPDATA%\CCMeeting\`（`QStandardPaths::AppLocalDataLocation`） |
| 录制视频 | `%USERPROFILE%\Videos\CCMeeting\` |
| 用户配置覆盖 | `<exe>/config/client.json`（不存在时回退到内置 `:/config/client.json.default`） |

安装目录（如 `C:\Program Files\...`）**只读**，任何写入都会失败，因此用户数据一律走
`QStandardPaths`。

## 当前资源清单

| 文件 | 用途 | 引用点 |
|---|---|---|
| `images/loading.gif` | 聊天气泡加载动画 | `Source::wait_gif` → `chatmessage.cpp` |
| `images/demo-avatar.png` | 演示头像（当前无代码引用，保留备查） | — |
| `audio/message.wav` | 收到 @ 消息的提示音 | `chat_controller.cpp`、`Source::at_sound` |
