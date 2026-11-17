@echo off
REM ===========================================================================
REM  build_test_proto.bat -- build and run ONLY the proto.h logic tests
REM
REM  These tests are pure logic: no COM, no WMI, no admin rights, no Huawei
REM  hardware. They are the only part of this project verifiable off-device.
REM
REM  ASCII only on purpose (cmd.exe parses .bat with the OEM code page).
REM ===========================================================================
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)

REM Fall back to the caller's environment if cl is already on PATH
REM (e.g. started from a "x64 Native Tools Command Prompt").
where cl.exe >nul 2>nul
if errorlevel 1 (
    if "!VSDIR!"=="" (
        echo [ERROR] cl.exe not on PATH and no Visual Studio with MSVC found.
        exit /b 1
    )
    call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

cd /d "%~dp0"
if not exist build mkdir build

echo === [1/2] build proto logic tests ===
cl.exe /nologo /utf-8 /W4 /O2 /MT /Isrc tests\test_proto.c src\proto.c /Fe:build\test_proto.exe
if errorlevel 1 (
    echo [FAILED] test program does not compile
    exit /b 1
)

echo.
echo === [2/2] run proto logic tests ===
build\test_proto.exe
if errorlevel 1 (
    echo [FAILED] tests failed
    exit /b 1
)
echo [OK] all proto.h assertions passed
exit /b 0
