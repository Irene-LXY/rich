@echo off
rem One-click build for the whole project (dev program + automation framework).
setlocal
cd /d "%~dp0"

where powershell >nul 2>nul
if errorlevel 1 (
    echo [ERROR] PowerShell not found.
    pause
    exit /b 1
)

echo.
echo ===== [1/2] Building dev program (monopoly / rich / monopoly_test) =====
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0rich\scripts\build_and_test_windows.ps1" -NoPause -SkipTest
if errorlevel 1 (
    echo.
    echo [ERROR] Dev program build failed. See errors above.
    pause
    exit /b 1
)

echo.
echo ===== [2/2] Building automation framework (run_tests / run_interactive_tests) =====
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0automation\scripts\build.ps1" -NoPause
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
