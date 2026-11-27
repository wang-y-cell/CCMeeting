
[CmdletBinding()]
param(
    [ValidateSet('release','debug')]
    [string]$Config = 'debug',
    [switch]$List,
    [switch]$Clean,
    [string]$NinjaPath = 'F:/ninja/ninja.exe',
    [string]$QtMsvcLibPath = 'F:/Qt/6.8.3/msvc2022_64'
)

$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if( -not (Test-Path $VsWhere)) {
    Write-Error @"
vswhere 没有找到$VsWhere
请安装 Visual Studio Installer
"@
    return
}

if($List) {
  Write-Host "`n=== 本机 Visual Studio 实例 ===" -ForegroundColor Cyan
  $instances = & $VsWhere -all -prerelease -products * -format json | ConvertFrom-Json
  if (-not $instances) {
      Write-Host "未发现任何 VS 实例" -ForegroundColor Yellow
      return
  }
  $i = 1
  foreach ($inst in $instances) {
      $ver   = $inst.installationVersion
      $name  = $inst.displayName
      $path  = $inst.installationPath
      $pre   = if ($inst.isPrerelease) { ' [Preview]' } else { '' }
      $major = ($ver -split '\.')[0]

      Write-Host "[$i] " -NoNewline -ForegroundColor Green
      Write-Host "$name$pre" -ForegroundColor White
      Write-Host "    版本：$ver  (主版本 $major)"
      Write-Host "    路径：$path"
      Write-Host "    进入：.\Enter-VsDevShell.ps1 -Major $major"
      Write-Host ""
      $i++
  }
  return
}

if (-not (Test-Path $NinjaPath)) {
    Write-Error "找不到 Ninja 构建工具: $NinjaPath"
    return
}
if (-not (Test-Path $QtMsvcLibPath)) {
    Write-Error "找不到 Qt 库路径: $QtMsvcLibPath"
    return
}

# 3. 清理旧的构建目录 (修复了路径不匹配的问题)
$BuildDir = "build/client_$Config"
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host " 正在清理旧构建目录: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

# 4. 执行 CMake 配置与编译
Write-Host "`n开始配置 CMake ($Config 模式)..." -ForegroundColor Cyan

# 注意：这里去掉了 & 和双引号，直接传参
cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$(if ($Config -eq 'release') { 'Release' } else { 'Debug' })" `
    "-DCMAKE_MAKE_PROGRAM=$NinjaPath" `
    "-DBUILD_CLIENT=ON" `
    "-DBUILD_SERVER=OFF" `
    "-DBUILD_SERVER2=OFF" `
    "-DCMAKE_PREFIX_PATH=$QtMsvcLibPath"

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake 配置失败！"
    return
}

Write-Host "`n开始编译..." -ForegroundColor Cyan
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

cmake --build $BuildDir -j

$stopwatch.Stop()
if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild completed successfully! 耗时: $($stopwatch.Elapsed.TotalSeconds.ToString('F2')) 秒" -ForegroundColor Green
    Write-Host "可执行文件位置: ./$BuildDir/client/CloudMeeting.exe" -ForegroundColor Green
} else {
    Write-Error "编译失败！"
}
