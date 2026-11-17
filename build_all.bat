@echo off
REM ===========================================================================
REM  build_all.bat -- one-shot build: locates VS, builds tests + app, runs tests
REM
REM  Recommended entry point. It does NOT require a pre-configured developer
REM  prompt: it finds the Visual Studio installation via vswhere and calls
REM  vcvars64.bat itself.
REM
REM  ASCII only on purpose: cmd.exe parses .bat files using the OEM code page
REM  (936 on zh-CN), so UTF-8 Chinese text inside a .bat gets mangled and can
REM  break tokenization. Chinese documentation lives in README.md instead.
REM
REM  Requirements: Visual Studio 2022/2026 with the "Desktop development with
REM  C++" workload (MSVC toolset + Windows SDK).
REM
REM  Output: build\HuaweiWmiControl.exe  (static CRT, no runtime dependency)
REM ===========================================================================
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo [ERROR] vswhere.exe not found. Is Visual Studio installed?
    echo         Expected at: !VSWHERE!
    exit /b 1
)

set "VSDIR="
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"

if "!VSDIR!"=="" (
    echo [ERROR] No Visual Studio installation with the MSVC C++ toolset was found.
    echo         Open Visual Studio Installer, choose "Modify", and tick
    echo         "Desktop development with C++" ^(which brings MSVC + Windows SDK^).
    exit /b 1
)
echo [info] Visual Studio: !VSDIR!

if not exist "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" (
    echo [ERROR] vcvars64.bat missing under !VSDIR!\VC\Auxiliary\Build
    exit /b 1
)

call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] vcvars64.bat failed
    exit /b 1
)

cd /d "%~dp0"
if not exist build mkdir build

echo.
echo === [1/3] build proto logic tests ===
cl.exe /nologo /utf-8 /W4 /O2 /MT /Isrc /Fo:build\\ tests\test_proto.c src\proto.c /Fe:build\test_proto.exe
if errorlevel 1 (
    echo [FAILED] test program does not compile
    exit /b 1
)

echo.
echo === [2/3] run proto logic tests ===
build\test_proto.exe
if errorlevel 1 (
    echo [FAILED] proto logic tests failed
    exit /b 1
)

echo.
echo === [3/3] build tray application ===

REM A running instance keeps build\HuaweiWmiControl.exe locked, which surfaces as
REM a bare "LNK1104: cannot open file". Detect it first and say what to do.
tasklist /FI "IMAGENAME eq HuaweiWmiControl.exe" 2>nul | find /I "HuaweiWmiControl.exe" >nul
if not errorlevel 1 (
    echo [BLOCKED] HuaweiWmiControl.exe is still running.
    echo           It holds a lock on build\HuaweiWmiControl.exe, so linking would fail.
    echo           Quit it first: right-click the tray icon, then choose "Exit".
    exit /b 1
)

rc.exe /nologo /fo build\app.res app.rc
if errorlevel 1 (
    echo [FAILED] rc.exe
    exit /b 1
)

REM /MANIFESTUAC:NO is required. Without it the linker still emits its own
REM UAC fragment with level="asInvoker", which collides with the
REM requireAdministrator in app.manifest and fails with:
REM   manifest authoring error c1010001: Values of attribute level not equal
cl.exe /nologo /utf-8 /W4 /EHsc /O2 /MT ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
    /Isrc /Fo:build\\ ^
    src\proto.c src\log.cpp src\wmi.cpp src\services.cpp ^
    src\icon.cpp src\tray.cpp src\menu.cpp src\main.cpp ^
    build\app.res ^
    /Fe:build\HuaweiWmiControl.exe ^
    /link /SUBSYSTEM:WINDOWS /MANIFEST:EMBED /MANIFESTINPUT:app.manifest /MANIFESTUAC:NO ^
    wbemuuid.lib ole32.lib oleaut32.lib user32.lib shell32.lib shlwapi.lib ^
    comctl32.lib gdi32.lib advapi32.lib
if errorlevel 1 (
    echo [FAILED] compile or link
    exit /b 1
)

echo.
echo [OK] build\HuaweiWmiControl.exe
echo      Double-click it (UAC prompt is expected: the manifest requires admin).
exit /b 0
