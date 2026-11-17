@echo off
REM ===========================================================================
REM  build_diag.bat -- build the live hardware diagnostic tool (needs admin)
REM
REM  diag_live.exe opens the real WMI device and prints the HwStatus of every
REM  getter, plus a write-back test that writes each current value back
REM  unchanged. Use it to tell "the model really does not support this" apart
REM  from "our code is wrong" -- which is exactly how the inverted-gray menu
REM  bug was found on real hardware.
REM
REM  ASCII only (cmd.exe parses .bat with the OEM code page).
REM ===========================================================================
setlocal enabledelayedexpansion
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
)
where cl.exe >nul 2>nul
if errorlevel 1 (
    if "!VSDIR!"=="" ( echo [ERROR] no MSVC found & exit /b 1 )
    call "!VSDIR!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)
cd /d "%~dp0"
if not exist build mkdir build
cl.exe /nologo /utf-8 /W4 /EHsc /O2 /MT ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
    /Isrc ^
    src\proto.c src\log.cpp src\wmi.cpp src\services.cpp tests\diag_live.cpp ^
    /Fe:build\diag_live.exe ^
    /link wbemuuid.lib ole32.lib oleaut32.lib advapi32.lib shell32.lib shlwapi.lib user32.lib
if errorlevel 1 ( echo [FAILED] & exit /b 1 )
echo [OK] build\diag_live.exe
echo      Run it elevated; results go to %%TEMP%%\hwc_diag.txt
