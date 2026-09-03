@echo off
rem The only build entry point for the whole project.
rem Double-click this file to build both the game and automation framework.
setlocal
set "PROJECT_ROOT=%~dp0"
set "PROJECT_ROOT_ARG=%PROJECT_ROOT:~0,-1%"
cd /d "%PROJECT_ROOT%"

where powershell >nul 2>nul
if errorlevel 1 (
    echo [ERROR] PowerShell not found.
    pause
    exit /b 1
)

echo.
echo ===== [0/2] Cleaning existing build artifacts =====
if not exist "%PROJECT_ROOT%automation\scripts\clean_build_artifacts.ps1" (
    echo [ERROR] Missing automation\scripts\clean_build_artifacts.ps1
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%automation\scripts\clean_build_artifacts.ps1" -ProjectRoot "%PROJECT_ROOT_ARG%"
if errorlevel 1 (
    echo.
    echo [ERROR] Failed to clean existing build artifacts. See errors above.
    pause
    exit /b 1
)

echo.
echo ===== [1/2] Building dev program (monopoly / rich / monopoly_test) =====
if not exist "%PROJECT_ROOT%rich\scripts\build_and_test_windows.ps1" (
    echo [ERROR] Missing rich\scripts\build_and_test_windows.ps1
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%rich\scripts\build_and_test_windows.ps1" -NoPause -SkipTest
if errorlevel 1 (
    echo.
    echo [ERROR] Dev program build failed. See errors above.
    pause
    exit /b 1
)

echo.
echo ===== [2/2] Building automation framework (run_tests / run_interactive_tests) =====
if not exist "%PROJECT_ROOT%automation\scripts\build.ps1" (
    echo [ERROR] Missing automation\scripts\build.ps1
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%PROJECT_ROOT%automation\scripts\build.ps1" -NoPause
if errorlevel 1 (
    echo.
    echo [ERROR] Automation framework build failed. See errors above.
    pause
    exit /b 1
)

echo.
echo ===== All builds completed =====
pause
exit /b 0
