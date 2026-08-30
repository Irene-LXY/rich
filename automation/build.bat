@echo off
rem Portable build: uses CMake to auto-detect the installed C compiler.
rem Does not hardcode any specific compiler or compiler path.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found. Install CMake and any C11 compiler, then add them to PATH.
    exit /b 1
)

cmake -S . -B build
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo.
echo BUILD SUCCEEDED - look for run_tests and run_interactive_tests under build\.
exit /b 0
