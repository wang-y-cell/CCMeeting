# 生成 clangd 用的合并 compile_commands.json
#
# 背景：.clangd 只能指向一个编译数据库；client 用 build/client，服务器用
# build/server-clangd，两边各有一份。本脚本把 client / server / 测试的数据库
# 合并到 build/clangd/compile_commands.json，.clangd 指向它。
#
# 用法（在仓库根目录）：
#   powershell -ExecutionPolicy Bypass -File .\gen_compile_commands.ps1
#   powershell -ExecutionPolicy Bypass -File .\gen_compile_commands.ps1 -Force
#
# -Force 会重新 configure client（默认复用已有 build/client 的数据库）。
# 新增 .cpp 文件后需要重跑本脚本（GLOB 变更只有 configure 才能感知）。

param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path # 获得当前执行脚本的绝对路径
Set-Location $root # 切换工作目录

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
if (-not $vs) {
    throw "找不到 Visual Studio（vswhere 返回空）。"
}
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$ninja = "F:/ninja/ninja.exe"
$qt = "F:/Qt/6.8.3/msvc2022_64"
# 服务器只需要 Boost 头文件；优先用 xrtc 子模块自带的那份，避免下载
$boost = Join-Path $root "third_party\webrtcSDK\third_party\boost-1.92.0"

function Invoke-CMakeConfigure {
    param([string]$BuildDir, [string]$ExtraArgs, [string]$SourceDir = ".")
    $cmd = "`"$vcvars`" >nul 2>&1 && cmake -S $SourceDir -B $BuildDir -G Ninja " +
           "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON " +
           "-DCMAKE_MAKE_PROGRAM=$ninja $ExtraArgs"
    Write-Host "[clangd] configure $BuildDir"
    cmd /c $cmd | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "cmake configure 失败: $BuildDir"
    }
}

# 1) client（默认复用）
$clientDb = "build/client/compile_commands.json"
if ($Force -or -not (Test-Path $clientDb)) {
    Invoke-CMakeConfigure "build/client" `
        "-DCMAKE_BUILD_TYPE=Debug -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DQT_INSTALL_PATH=$qt"
}

# 2) message_server
Invoke-CMakeConfigure "build/server-clangd" `
    "-DCMAKE_BUILD_TYPE=Debug -DBUILD_CLIENT=OFF -DBUILD_SERVER=ON -DBoost_ROOT=`"$boost`""

# 3) 两套测试（有就合并）
if (Test-Path "client/tests/CMakeLists.txt") {
    Invoke-CMakeConfigure "build/tests-clangd" `
        "-DCMAKE_BUILD_TYPE=Debug" "client/tests"
}
if (Test-Path "message_server/tests/CMakeLists.txt") {
    Invoke-CMakeConfigure "build/server-tests-clangd" `
        "-DCMAKE_BUILD_TYPE=Debug" "message_server/tests"
}

# 4) 合并（按 file 去重）
$sources = @(
    "build/client/compile_commands.json",
    "build/server-clangd/compile_commands.json",
    "build/tests-clangd/compile_commands.json",
    "build/server-tests-clangd/compile_commands.json"
)
$entries = @()
foreach ($file in $sources) {
    if (-not (Test-Path $file)) {
        Write-Host "[clangd] skip $file"
        continue
    }
    Write-Host "[clangd] read $file"
    $entries += Get-Content $file -Raw | ConvertFrom-Json
}

$seen = @{}
$merged = New-Object System.Collections.Generic.List[object]
foreach ($entry in $entries) {
    if (-not $seen.ContainsKey($entry.file)) {
        $seen[$entry.file] = $true
        $merged.Add($entry)
    }
}

New-Item -ItemType Directory -Force "build/clangd" | Out-Null
$out = "build/clangd/compile_commands.json"
# -Compress 避免几十 MB 的缩进
$merged | ConvertTo-Json -Depth 12 -Compress | Set-Content $out -Encoding UTF8

Write-Host "[clangd] 合并完成：$($merged.Count) 条 -> $out"
Write-Host "[clangd] .clangd 已指向 build/clangd；在 VS Code 里执行 clangd: Restart language server"
