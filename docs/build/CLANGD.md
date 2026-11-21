# clangd 配置（client + message_server）

`.clangd` 只能指向**一个**编译数据库。client 与 message_server 是两套 CMake 工程，
所以这里用一个 `gen_compile_commands.ps1` 把四份数据库合并到
`build/clangd/compile_commands.json`，`.clangd` 指向它：

```yaml
CompileFlags:
    CompilationDatabase: build/clangd
```

## 生成 / 更新

仓库根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\gen_compile_commands.ps1
```

首次或改了 CMake 配置：

```powershell
powershell -ExecutionPolicy Bypass -File .\gen_compile_commands.ps1 -Force
```

脚本会：

1. 复用或重新 configure `build/client`（client + `client/tests`，BUILD_TESTS=ON）；
2. configure `build/server-clangd`（`BUILD_SERVER=ON`，Boost 用
   `third_party/webrtcSDK/third_party/boost-1.92.0`，不需要联网下载）；
3. configure `build/tests-clangd`（`client/tests` 独立工程）；
4. configure `build/server-tests-clangd`（`message_server/tests`）；
5. 合并去重后写入 `build/clangd/compile_commands.json`。

完成后在 VS Code 里执行 **clangd: Restart language server**（或重开窗口）。

## 什么时候需要重跑

| 变更 | 是否需要 |
|---|---|
| 新增/删除 `.cpp` / `.h`（GLOB 目标） | ✅ 需要（configure 才能发现新文件） |
| 修改 CMakeLists（依赖、include、宏） | ✅ 需要 |
| 修改普通源码内容 | ❌ 不需要 |
| 切换 Build 类型 / Qt 版本 | ✅ 需要（`-Force`） |

## 常见问题

- **服务器代码仍然无补全**：确认 `build/server-clangd/compile_commands.json` 存在，
  且合并文件里能搜到 `message_server/src/...`；然后重启 clangd。
- **clangd 报 MSVC 参数不识别**：本项目数据库由 Ninja + MSVC 生成，clangd 默认
  可以读取 `cl.exe` 风格参数；若你的 clangd 版本较旧，可在 VS Code 的
  `clangd.arguments` 加 `--query-driver=F:/vs/VC/Tools/MSVC/**/cl.exe`。
- **Windows PowerShell 下脚本乱码/解析失败**：脚本必须保存为 **UTF-8 BOM**
  （PowerShell 5.1 需要），仓库里的版本已经带 BOM。
