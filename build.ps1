$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"

Remove-Item -Recurse -Force build\client -ErrorAction SilentlyContinue

cmd /c "`"$vcvars`" && cmake -S . -B build/client -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=F:/ninja/ninja.exe -DBUILD_CLIENT=ON -DBUILD_SERVER=OFF -DBUILD_SERVER2=OFF -DQT_INSTALL_PATH=F:/Qt/6.8.3/msvc2022_64 && cmake --build build/client -j"

Write-Host "Build completed"

./build/client/client/CloudMeeting.exe