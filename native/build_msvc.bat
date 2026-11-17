@echo off
REM =============================================================================
REM  build_msvc.bat — 直接用 cl.exe 构建，不依赖 CMake
REM
REM  用法：
REM    1) 打开「x64 Native Tools Command Prompt for VS 2022」
REM       或先执行："C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
REM    2) 在本目录运行 build_msvc.bat
REM
REM  产物：build\HuaweiWmiControl.exe（静态 CRT，无运行时依赖）
REM =============================================================================
setlocal

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 cl.exe。
    echo         请使用 Visual Studio 的「x64 Native Tools 命令提示符」运行，
    echo         或先调用 vcvars64.bat 初始化环境。
    exit /b 1
)

if not exist build mkdir build

echo [1/2] 编译资源...
rc.exe /nologo /fo build\app.res app.rc
if errorlevel 1 (
    echo [错误] 资源编译失败。
    exit /b 1
)

echo [2/2] 编译并链接...
cl.exe /nologo /utf-8 /W4 /EHsc /O2 /MT ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
    /Isrc ^
    src\proto.c src\log.cpp src\wmi.cpp src\services.cpp ^
    src\icon.cpp src\tray.cpp src\menu.cpp src\main.cpp ^
    build\app.res ^
    /Fe:build\HuaweiWmiControl.exe ^
    /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest ^
    wbemuuid.lib ole32.lib oleaut32.lib user32.lib shell32.lib shlwapi.lib ^
    comctl32.lib gdi32.lib advapi32.lib
if errorlevel 1 (
    echo [错误] 编译或链接失败。
    exit /b 1
)

echo.
echo 构建成功：build\HuaweiWmiControl.exe
echo 运行时需右键「以管理员身份运行」（清单已要求提权，双击即可触发 UAC）。
endlocal
exit /b 0
