# 直接运行 Release 客户端。
# 构建目录本身不包含 Qt DLL，必须把 Qt 的 bin 加入 PATH；
# 同时按脚本所在目录解析相对路径，避免从其他目录执行时找不到 exe。
$ErrorActionPreference = "Stop"

$qtBin = "F:\Qt\6.8.3\msvc2022_64\bin"
if (-not (Test-Path (Join-Path $qtBin "Qt6Core.dll"))) {
    throw "Qt bin not found: $qtBin"
}
$env:PATH = "$qtBin;$env:PATH"

$exe = Join-Path $PSScriptRoot "build\client_release\client\CloudMeeting.exe"
if (-not (Test-Path $exe)) {
    throw "CloudMeeting.exe not found: $exe"
}

Set-Location (Split-Path $exe -Parent)
& $exe
exit $LASTEXITCODE
