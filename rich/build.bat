@echo off
rem Portable build: uses CMake (CMakeLists.txt) and lets it auto-detect the C compiler.
rem Does not hardcode any specific compiler or compiler path.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] cmake not found. Install CMake and a C11 compiler, then add them to PATH.
    exit /b 1
)

cmake -S . -B build
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo.
echo BUILD SUCCEEDED
echo   build\monopoly.exe       interactive game
echo   build\monopoly_test.exe  automation test entry
echo   (exact paths may be under build\Debug or build\Release, depending on the generator)
exit /b 0
