@echo off
REM ===========================================================================
REM  build_clean.bat -- remove compiler intermediates, keep the built binaries
REM
REM  Deletes: *.obj *.res *.pdb *.ilk *.exp *.iobj *.ipdb
REM  Keeps  : build\*.exe  (the actual deliverables)
REM
REM  Pass "all" to wipe the whole build\ directory including the binaries.
REM  ASCII only (cmd.exe parses .bat with the OEM code page).
REM ===========================================================================
setlocal
cd /d "%~dp0"

if /I "%~1"=="all" (
    echo Removing build\ entirely...
    if exist build rmdir /S /Q build
    echo [OK] build\ removed
    exit /b 0
)

set COUNT=0
for %%E in (obj res pdb ilk exp iobj ipdb) do (
    for %%F in (build\*.%%E) do (
        if exist "%%F" (
            del /Q "%%F" >nul 2>&1
            set /a COUNT+=1
        )
    )
    REM also catch strays that landed next to the sources
    for %%F in (*.%%E) do (
        if exist "%%F" (
            del /Q "%%F" >nul 2>&1
            set /a COUNT+=1
        )
    )
)

echo [OK] removed %COUNT% intermediate file^(s^)
echo      kept: build\*.exe
exit /b 0
