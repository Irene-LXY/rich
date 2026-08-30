@echo off
rem One-click build (Windows) - test-side automation framework.
rem Delegates to the universal PowerShell script, which auto-detects the
rem toolchain and picks a working CMake generator (avoids NMake+cl.exe,
rem which needs a vcvars environment and fails when double-clicked).
setlocal
cd /d "%~dp0"

where powershell >nul 2>nul
if errorlevel 1 (
    echo [ERROR] PowerShell not found.
    pause
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" %*
exit /b %ERRORLEVEL%
