@echo off
rem One-click interactive black-box test. Auto-detects monopoly.exe.
rem To override, create program_interactive.txt next to this script with the
rem full path to monopoly.exe (one line).
setlocal EnableExtensions
cd /d "%~dp0"

set "RUNNER=%~dp0build\run_interactive_tests.exe"
if not exist "%RUNNER%" (
    echo Building test environment...
    where cmake >nul 2>nul
    if errorlevel 1 (
        echo [ERROR] cmake not found. Install CMake and a C11 compiler first.
        echo.
        pause
        exit /b 1
    )
    cmake -S . -B build
    if errorlevel 1 goto :buildfail
    cmake --build build
    if errorlevel 1 goto :buildfail
)

rem --- locate monopoly.exe ---
set "TARGET="
if exist "%~dp0program_interactive.txt" set /p TARGET=<"%~dp0program_interactive.txt"

if not defined TARGET if exist "%~dp0bin\monopoly.exe" set "TARGET=%~dp0bin\monopoly.exe"
if not defined TARGET if exist "%~dp0..\rich\build\monopoly.exe" set "TARGET=%~dp0..\rich\build\monopoly.exe"
if not defined TARGET if exist "%~dp0..\rich\build\Release\monopoly.exe" set "TARGET=%~dp0..\rich\build\Release\monopoly.exe"
if not defined TARGET if exist "%~dp0..\rich\build\Debug\monopoly.exe" set "TARGET=%~dp0..\rich\build\Debug\monopoly.exe"

if not defined TARGET (
    echo [ERROR] monopoly.exe not found.
    echo.
    echo Options:
    echo   Option 1 - copy monopoly.exe into:  %~dp0bin\
    echo   Option 2 - create program_interactive.txt in this folder containing the full path.
    echo.
    pause
    exit /b 1
)

> "%~dp0program_interactive.txt" echo %TARGET%

echo.
echo Using program: %TARGET%
echo.
echo ===== Interactive tests =====
"%RUNNER%" --program "%TARGET%" --cases "%~dp0interactive\cases"
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
