<#
.SYNOPSIS
    编译 CloudMeeting 客户端（MSVC ABI x64 + Ninja，默认 clang-cl，可切回 cl.exe）。

.DESCRIPTION
    脚本会自己把 Visual Studio 的 x64 编译环境导入当前 PowerShell 会话
    （vcvars64.bat：link.exe / INCLUDE / LIB / Windows SDK），然后用 CMake + Ninja
    构建 client：

        .\build.ps1                       # Debug，clang-cl（默认）
        .\build.ps1 -Config release       # Release
        .\build.ps1 -Compiler msvc        # 换回 cl.exe（旧行为）
        .\build.ps1 -Config debug -Clean

    为什么默认 clang-cl：third_party/webrtcSDK/webrtc/lib_*/webrtc.lib 是 Chromium
    自带 clang（LLVM 23）以 MSVC ABI 编译的，clang-cl 与它同源；cl.exe 也能链接
    （两者都产出 MSVC ABI 的 COFF）。Qt 仍是 msvc2022_64 那套，不需要换。

.PARAMETER Config
    debug（默认）或 release。

.PARAMETER Compiler
    clang-cl（默认，用 Visual Studio 自带的 LLVM）或 msvc（cl.exe）。

.PARAMETER Clean
    编译前删掉 build/client_<Config> 整个目录。

.PARAMETER SkipVsEnv
    跳过 MSVC 环境导入（已经在 vcvars 环境里、或只想看 -List 时用）。
    注意：clang-cl 也依赖这份环境（link.exe / INCLUDE / LIB）。

.PARAMETER NinjaPath
    ninja.exe 路径。

.PARAMETER QtMsvcLibPath
    Qt msvc 版安装目录（CMAKE_PREFIX_PATH）。clang-cl 同样用它（MSVC ABI）。
#>

[CmdletBinding()]
param(
    [ValidateSet('release','debug')]
    [string]$Config = 'debug',
    [ValidateSet('clang-cl','msvc')]
    [string]$Compiler = 'clang-cl',
    [switch]$List,
    [switch]$Clean,
    [switch]$Help,
    [switch]$SkipVsEnv,
    [string]$NinjaPath = 'F:/ninja/ninja.exe',
    [string]$QtMsvcLibPath = 'F:/Qt/6.8.3/msvc2022_64'
)


if($Help) {
    Write-Host "
    args:
        -Config:         [debug(default) , release] 选择构建方式
        -Compiler:       [clang-cl(default) , msvc] 选择编译器
        -List:           列出可用vs
        -Clean:          清空旧的构建文件夹
        -SkipVsEnv:      跳过 MSVC 环境导入（已经在 vcvars 环境里、或只想看 -List 时用）。
                        注意：clang-cl 也依赖这份环境（link.exe / INCLUDE / LIB）。
        -NinjaPath:      ninja.exe 路径。
        -QtMsvcLibPath:  Qt msvc 版安装目录（CMAKE_PREFIX_PATH）。clang-cl 同样用它（MSVC ABI）。
    
    "
    return
}

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
      Write-Host ""
      $i++
  }
  Write-Host "（build.ps1 会自动选最新一个装了 MSVC x64 的实例，无需手动进入环境）" -ForegroundColor DarkGray
  return
}

# ---------------------------------------------------------------------------
# 1. 导入 MSVC x64 编译环境
# ---------------------------------------------------------------------------
function Import-MsvcEnvironment {
    param([string]$VsPath)

    $vcvars = Join-Path $VsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "找不到 vcvars64.bat：$vcvars（该 VS 实例可能没装 C++ 桌面开发组件）"
    }

    # 把 vcvars64.bat 设置的环境变量全部导进当前进程
    # （cl.exe / clang-cl / link.exe / INCLUDE / LIB / VCINSTALLDIR 都由它准备好）
    $lines = cmd /c "`"$vcvars`" >nul && set"
    foreach ($line in ($lines -split "`r?`n")) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2] -ErrorAction SilentlyContinue
        }
    }
}

if ($SkipVsEnv) {
    Write-Host "[env] -SkipVsEnv：沿用当前进程的编译环境" -ForegroundColor DarkGray
} elseif ((Get-Command cl.exe -ErrorAction SilentlyContinue) -and $env:VCToolsInstallDir) {
    Write-Host "[env] 已在 MSVC 环境中: $env:VCToolsInstallDir" -ForegroundColor DarkGray
} else {
    $vsPath = & $VsWhere -latest -prerelease -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath

    if (-not $vsPath) {
        Write-Error @"
没有找到安装了 MSVC x64 工具链的 Visual Studio 实例。
请用 Visual Studio Installer 勾选「使用 C++ 的桌面开发」。
（本机已安装的实例可以用 .\build.ps1 -List 查看）
"@
        return
    }

    Write-Host "[env] 正在导入 MSVC x64 编译环境: $vsPath" -ForegroundColor Cyan
    try {
        Import-MsvcEnvironment -VsPath $vsPath
    } catch {
        Write-Error "[env] 导入失败：$($_.Exception.Message)"
        return
    }

    $clPath = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
    if (-not $clPath) {
        Write-Error "[env] 导入后仍找不到 cl.exe，请检查 Visual Studio 是否完整安装"
        return
    }
    Write-Host "[env] MSVC $env:VCToolsVersion = $clPath" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 2. 定位编译器
#
# clang-cl 在 vcvars 环境里通常已经在 PATH 上（<VS>\VC\Tools\Llvm\x64\bin），
# 找不到就按 VS 安装目录兜底；两者都要求 MSVC ABI（不能是 MinGW 的 g++）。
# ---------------------------------------------------------------------------
function Resolve-ClangClPath {
    $cmd = Get-Command clang-cl.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @()
    foreach ($base in @($env:VCINSTALLDIR, $env:VSINSTALLDIR)) {
        if (-not $base) { continue }
        $candidates += (Join-Path $base 'Tools\Llvm\x64\bin\clang-cl.exe')
        $candidates += (Join-Path $base 'Tools\Llvm\bin\clang-cl.exe')
        $candidates += (Join-Path $base 'VC\Tools\Llvm\x64\bin\clang-cl.exe')
        $candidates += (Join-Path $base 'VC\Tools\Llvm\bin\clang-cl.exe')
    }
    foreach ($p in $candidates) {
        if ($p -and (Test-Path $p)) { return $p }
    }
    return $null
}

$CompilerExe = $null
if ($Compiler -eq 'clang-cl') {
    $CompilerExe = Resolve-ClangClPath
    if (-not $CompilerExe) {
        Write-Error @"
找不到 clang-cl.exe。
请在 Visual Studio Installer 里给该实例勾选「使用 C++ 的桌面开发」下的
    C++ Clang tools for Windows
（默认装到 <VS>\VC\Tools\Llvm\x64\bin\clang-cl.exe）
或者把它所在的目录放进 PATH 后再运行。

想沿用旧的 cl.exe 行为：.\build.ps1 -Compiler msvc
"@
        return
    }
    $clangVersion = (& $CompilerExe --version | Select-Object -First 1)
    Write-Host "[env] clang-cl = $CompilerExe" -ForegroundColor Green
    Write-Host "[env] $clangVersion" -ForegroundColor DarkGray
} else {
    $clCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $clCommand) {
        Write-Error "[env] 找不到 cl.exe，请检查 Visual Studio 是否完整安装"
        return
    }
    $CompilerExe = $clCommand.Source
    Write-Host "[env] cl.exe = $CompilerExe (MSVC $env:VCToolsVersion)" -ForegroundColor Green
}

if (-not (Test-Path $NinjaPath)) {
    Write-Error "找不到 Ninja 构建工具: $NinjaPath"
    return
}
if (-not (Test-Path $QtMsvcLibPath)) {
    Write-Error "找不到 Qt 库路径: $QtMsvcLibPath"
    return
}

# ---------------------------------------------------------------------------
# 3. 清理旧构建目录 / 失效的 CMake 缓存
# ---------------------------------------------------------------------------
$BuildDir = "build/client_$Config"
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host " 正在清理旧构建目录: $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
}

# 缓存里记着的编译器可能已经不存在（VS 升级换了工具集）、或者和本次要求的
# 编译器不是同一族（比如上次是 cl.exe，这次要 clang-cl）：这两种情况都直接
# 清掉缓存，让 CMake 重新按当前环境探测。
$NeedCompilerArgs = -not (Test-Path (Join-Path $BuildDir 'CMakeCache.txt'))
$CacheFile = Join-Path $BuildDir 'CMakeCache.txt'
if (Test-Path $CacheFile) {
    $cachedLine = Select-String -Path $CacheFile -Pattern '^CMAKE_CXX_COMPILER:[A-Z]+=(.+)$' |
        Select-Object -First 1
    if ($cachedLine) {
        $cachedCompiler = $cachedLine.Matches[0].Groups[1].Value.Trim().Trim('"')
        $cachedLeaf = Split-Path $cachedCompiler -Leaf
        $wantedLeaf = Split-Path $CompilerExe -Leaf
        if (-not (Test-Path $cachedCompiler)) {
            Write-Host " 缓存里的编译器已不存在（$cachedCompiler），清理构建目录" -ForegroundColor Yellow
            Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
            $NeedCompilerArgs = $true
        } elseif ($cachedLeaf -ne $wantedLeaf) {
            Write-Host " 缓存里的编译器是 $cachedLeaf，本次要求 $wantedLeaf，清理后重新探测" -ForegroundColor Yellow
            Remove-Item -Recurse -Force $BuildDir -ErrorAction SilentlyContinue
            $NeedCompilerArgs = $true
        }
    }
}

# ---------------------------------------------------------------------------
# 4. 执行 CMake 配置与编译
# ---------------------------------------------------------------------------
Write-Host "`n开始配置 CMake ($Config 模式，编译器 $Compiler)..." -ForegroundColor Cyan

# 注意：只在「全新目录」时显式传 -DCMAKE_C/CXX_COMPILER。
# 已存在的缓存再次传这两个变量，会把缓存项改成 UNINITIALIZED 类型，
# 重新配置时 project() 会报 "unknown error"。缓存里的编译器已经和本次一致时
# 不需要再传，不一致的情况上面的清理逻辑已经把构建目录删掉了。
$cmakeArgs = @(
    '-S', '.'
    '-B', $BuildDir
    '-G', 'Ninja'
    "-DCMAKE_BUILD_TYPE=$(if ($Config -eq 'release') { 'Release' } else { 'Debug' })"
    "-DCMAKE_MAKE_PROGRAM=$NinjaPath"
    '-DBUILD_CLIENT=ON'
    '-DBUILD_SERVER=OFF'
    "-DCMAKE_PREFIX_PATH=$QtMsvcLibPath"
)
if ($NeedCompilerArgs) {
    $cmakeArgs += "-DCMAKE_C_COMPILER=$CompilerExe"
    $cmakeArgs += "-DCMAKE_CXX_COMPILER=$CompilerExe"
}

& cmake @cmakeArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake 配置失败！"
    return
}

Write-Host "`n开始编译..." -ForegroundColor Cyan
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

& cmake --build $BuildDir -j

$stopwatch.Stop()
if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild completed successfully! 编译器: $Compiler  耗时: $($stopwatch.Elapsed.TotalSeconds.ToString('F2')) 秒" -ForegroundColor Green
    Write-Host "可执行文件位置: ./$BuildDir/client/CloudMeeting.exe" -ForegroundColor Green
} else {
    Write-Error "编译失败！"
}
