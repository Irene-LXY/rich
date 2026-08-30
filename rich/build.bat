@echo off
rem Portable build: let CMake discover the installed C11 compiler.
setlocal EnableExtensions
cd /d "%~dp0"
set "BUILD_DIR=%~1"
if not defined BUILD_DIR set "BUILD_DIR=build"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found. Install CMake and a C11 compiler first.
    exit /b 1
)

cmake -S . -B "%BUILD_DIR%"
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

echo.
echo BUILD SUCCEEDED
echo   interactive targets: monopoly / rich
echo   automation target:   monopoly_test
echo   build directory:      %BUILD_DIR%
echo   Multi-config generators may place executables under Debug or Release.
exit /b 0
