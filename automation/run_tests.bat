@echo off
rem One-click automated test. Auto-detects the program under test.
rem To override, create program.txt next to this script with the full path
rem to monopoly_test.exe (one line).
setlocal EnableExtensions
cd /d "%~dp0"

set "RUNNER=%~dp0build\run_tests.exe"
if not exist "%RUNNER%" if exist "%~dp0build\Release\run_tests.exe" set "RUNNER=%~dp0build\Release\run_tests.exe"
if not exist "%RUNNER%" if exist "%~dp0build\Debug\run_tests.exe" set "RUNNER=%~dp0build\Debug\run_tests.exe"
if not exist "%RUNNER%" (
    echo Building test environment...
    where powershell >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] PowerShell not found.
        echo.
        pause
        exit /b 1
    )
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" -NoPause
    if errorlevel 1 goto :buildfail
)

rem --- locate monopoly_test.exe ---
set "TARGET="
if exist "%~dp0program.txt" set /p TARGET=<"%~dp0program.txt"

if not defined TARGET if exist "%~dp0bin\monopoly_test.exe" set "TARGET=%~dp0bin\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build-local\monopoly_test.exe" set "TARGET=%~dp0..\rich\build-local\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build-local\Release\monopoly_test.exe" set "TARGET=%~dp0..\rich\build-local\Release\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build-local\Debug\monopoly_test.exe" set "TARGET=%~dp0..\rich\build-local\Debug\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build\monopoly_test.exe" set "TARGET=%~dp0..\rich\build\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build\Release\monopoly_test.exe" set "TARGET=%~dp0..\rich\build\Release\monopoly_test.exe"
if not defined TARGET if exist "%~dp0..\rich\build\Debug\monopoly_test.exe" set "TARGET=%~dp0..\rich\build\Debug\monopoly_test.exe"

if not defined TARGET (
    echo [ERROR] monopoly_test.exe not found.
    echo.
    echo Options:
    echo   Option 1 - copy monopoly_test.exe into:  %~dp0bin\
    echo   Option 2 - create program.txt in this folder containing the full path.
    echo.
    pause
    exit /b 1
)

> "%~dp0program.txt" echo %TARGET%

echo.
echo Using program: %TARGET%
echo.
echo ===== Positive cases =====
"%RUNNER%" --program "%TARGET%" --cases "%~dp0testcases" --map "%~dp0spec\map.json"
echo.
echo Done.
echo.
pause
exit /b 0

:buildfail
echo.
echo Build failed - see errors above.
echo.
pause
exit /b 1
