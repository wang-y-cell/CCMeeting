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

Write-Host "[deploy] target=$Target  host=$Remote" -ForegroundColor Cyan

# 1) 创建远程目录
& ssh -o StrictHostKeyChecking=no $Remote "mkdir -p $RepoPath/deploy/units"
if ($LASTEXITCODE -ne 0) { throw "mkdir failed" }

# 2) 把 deploy.sh 转成 LF 后上传（避免 CRLF 破坏 bash）
$srcSh = Join-Path $deployDir "deploy.sh"
$tmpSh = Join-Path $env:TEMP "deploy_sh_lf.sh"
$text = [System.IO.File]::ReadAllText($srcSh).Replace("`r`n", "`n")
[System.IO.File]::WriteAllText($tmpSh, $text, (New-Object System.Text.UTF8Encoding($false)))
& scp -o StrictHostKeyChecking=no $tmpSh "${Remote}:$RepoPath/deploy/deploy.sh"
if ($LASTEXITCODE -ne 0) { throw "upload deploy.sh failed" }

# 3) 上传 systemd unit
foreach ($unit in (Get-ChildItem (Join-Path $deployDir "units") -Filter *.service)) {
    & scp -o StrictHostKeyChecking=no $unit.FullName "${Remote}:$RepoPath/deploy/units/$($unit.Name)"
    if ($LASTEXITCODE -ne 0) { throw "upload $($unit.Name) failed" }
}

# 4) 执行部署（失败由服务器脚本自动回滚）
Write-Host "[deploy] running deploy.sh $Target ..." -ForegroundColor Cyan
& ssh -o StrictHostKeyChecking=no $Remote "cd $RepoPath && bash deploy/deploy.sh $Target"
$exit = $LASTEXITCODE

if ($exit -eq 0) {
    Write-Host "[deploy] OK - deployment succeeded" -ForegroundColor Green
} else {
    Write-Host "[deploy] FAILED - server rolled back, see /root/deploy.log" -ForegroundColor Red
}
exit $exit
