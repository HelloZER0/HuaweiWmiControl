@echo off
REM =============================================================================
REM  build_test_proto.bat — 单独构建并运行 proto.h 纯逻辑测试
REM
REM  这一组测试不需要华为硬件、不需要 COM、不需要管理员权限。
REM  只用 Windows SDK 的 C 运行时即可（或 MinGW 的 gcc）。
REM
REM  用法：在「x64 Native Tools Command Prompt」中运行本脚本。
REM =============================================================================
setlocal

where cl.exe >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 cl.exe，请使用 Visual Studio 的开发者命令提示符。
    exit /b 1
)

if not exist build mkdir build

echo [1/2] 编译测试...
cl.exe /nologo /utf-8 /W4 /O2 /MT /Isrc ^
    tests\test_proto.c src\proto.c ^
    /Fe:build\test_proto.exe
if errorlevel 1 (
    echo [错误] 编译失败。
    exit /b 1
)

echo [2/2] 运行测试...
echo.
build\test_proto.exe
set RC=%errorlevel%
echo.
if not "%RC%"=="0" (
    echo 测试失败，退出码 %RC%
    exit /b %RC%
)
echo 测试全部通过。
endlocal
exit /b 0
