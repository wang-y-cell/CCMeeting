@echo off
setlocal

echo ==========================================
echo    CloudMeeting Client build and run
echo ==========================================

set "QT_DIR=F:\qt515\6.11.1\mingw_64"
set "MINGW_BIN=F:\qt515\Tools\mingw1310_64\bin"
set "EXE_PATH=build\client\CloudMeeting.exe"
set "PATH=%MINGW_BIN%;%QT_DIR%\bin;%PATH%"

:: 1. Configure
echo [1/4] cmake configure...
cmake ^
  -S . ^
  -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_PREFIX_PATH="%QT_DIR:\=/%" ^
  -DQT_INSTALL_PATH="%QT_DIR:\=/%" ^
  -DCMAKE_CXX_COMPILER="%MINGW_BIN:\=/%/g++.exe"
if errorlevel 1 (
  echo [error] cmake configure failed
  exit /b 1
)

:: 2. Build
echo [2/4] cmake build...
cmake --build build --parallel
if errorlevel 1 (
  echo [error] cmake build failed
  exit /b 1
)
echo [success] build completed

:: 3. Deploy Qt DLLs
if not exist "%EXE_PATH%" (
  echo [error] can't find: %EXE_PATH%
  exit /b 1
)

echo [3/4] deploy Qt DLLs...
call "%QT_DIR%\bin\windeployqt.exe" --debug --no-translations --no-opengl-sw "%EXE_PATH%"
if errorlevel 1 (
  echo [warning] windeployqt had issues, continuing...
) else (
  echo [success] DLL deployment completed
)

:: 4. Run
echo [4/4] starting CloudMeeting.exe ...
start "" "%EXE_PATH%"

endlocal
