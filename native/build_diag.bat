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
set COMMON=src\proto.c src\log.cpp src\wmi.cpp src\services.cpp
set LIBS=wbemuuid.lib ole32.lib oleaut32.lib advapi32.lib shell32.lib shlwapi.lib user32.lib
set CFLAGS=/nologo /utf-8 /W4 /EHsc /O2 /MT /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /Isrc

echo [1/3] diag_live   -- all getters + write-back
cl.exe %CFLAGS% %COMMON% tests\diag_live.cpp /Fe:build\diag_live.exe /link %LIBS%
if errorlevel 1 ( echo [FAILED] & exit /b 1 )

echo [2/3] diag_fnlock -- Fn-Lock raw bytes + value mapping (0..3)
cl.exe %CFLAGS% %COMMON% tests\diag_fnlock.cpp /Fe:build\diag_fnlock.exe /link %LIBS%
if errorlevel 1 ( echo [FAILED] & exit /b 1 )

echo [3/3] diag_fnwatch -- poll Fn-Lock value, detects physical Fn+Esc
cl.exe %CFLAGS% %COMMON% tests\diag_fnwatch.cpp /Fe:build\diag_fnwatch.exe /link %LIBS%
if errorlevel 1 ( echo [FAILED] & exit /b 1 )

echo.
echo [OK] build\diag_live.exe / diag_fnlock.exe / diag_fnwatch.exe
echo      Run elevated. Outputs:
echo        %%TEMP%%\hwc_diag.txt     (all getters)
echo        %%TEMP%%\hwc_fnlock.txt   (Fn-Lock raw + value mapping)
echo        %%TEMP%%\hwc_fnwatch.txt  (Fn-Lock value changes; press Fn+Esc to correlate)
