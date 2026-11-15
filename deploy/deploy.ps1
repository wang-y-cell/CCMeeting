# 本地一键部署包装器：把 deploy/ 目录送到服务器并执行 deploy.sh
#
# 用法：powershell -File deploy\deploy.ps1 [all|auth|signal|janus|coturn|units|verify|backup]
param(
    [string]$Target = "all",
    [string]$Remote = "root@8.153.155.18",
    [string]$RepoPath = "/root/code/CCMeeting"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$deployDir = Join-Path $repoRoot "deploy"

# SSH 免交互（有 askpass 就用；否则依赖 SSH key）
$askpass = Join-Path $env:TEMP "cc_askpass.cmd"
if (Test-Path $askpass) {
    $env:SSH_ASKPASS = $askpass
    $env:SSH_ASKPASS_REQUIRE = "force"
    $env:DISPLAY = "localhost:0"
}

Write-Host "[deploy] 目标: $Target  服务器: $Remote" -ForegroundColor Cyan

# 1) 把 deploy 目录同步到服务器（脚本与 unit 定义以本地为准）
Write-Host "[deploy] 同步 deploy/ 到服务器..." -ForegroundColor Cyan
& ssh -o StrictHostKeyChecking=no $Remote "mkdir -p $RepoPath/deploy/units"
if ($LASTEXITCODE -ne 0) { throw "创建远程目录失败" }
& scp -o StrictHostKeyChecking=no -r (Join-Path $deployDir "deploy.sh") "${Remote}:$RepoPath/deploy/deploy.sh"
if ($LASTEXITCODE -ne 0) { throw "上传 deploy.sh 失败" }
foreach ($unit in (Get-ChildItem (Join-Path $deployDir "units") -Filter *.service)) {
    & scp -o StrictHostKeyChecking=no $unit.FullName "${Remote}:$RepoPath/deploy/units/$($unit.Name)"
    if ($LASTEXITCODE -ne 0) { throw "上传 $($unit.Name) 失败" }
}

# 2) 执行部署（失败会由服务器脚本自动回滚）
Write-Host "[deploy] 在服务器执行 deploy.sh $Target ..." -ForegroundColor Cyan
& ssh -o StrictHostKeyChecking=no $Remote "cd $RepoPath && sed -i 's/\r`$//' deploy/deploy.sh && bash deploy/deploy.sh $Target"
$exit = $LASTEXITCODE

if ($exit -eq 0) {
    Write-Host "[deploy] ✅ 部署成功" -ForegroundColor Green
} else {
    Write-Host "[deploy] ❌ 部署失败（服务器已尝试回滚，详见 /root/deploy.log）" -ForegroundColor Red
}
exit $exit
