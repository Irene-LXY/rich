@echo off
rem ============================================================
rem  RichMan A6 - MSVC build script (Windows)
rem  Usage:  double-click or run  build.bat  in cmd
rem  Output: bin\A6.exe   (objects in obj\)
rem ============================================================
setlocal EnableDelayedExpansion
chcp 65001 >nul

rem --- locate Visual Studio via vswhere ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere not found. Please install Visual Studio 2019/2022 with C++ workload.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VS=%%i"
if not defined VS (
    echo [ERROR] No Visual Studio installation found.
    exit /b 1
)

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment.
    exit /b 1
)

if not exist obj mkdir obj
if not exist bin mkdir bin

rem /utf-8      : source files are UTF-8 (Chinese strings)
rem /W4         : high warning level
rem /std:c11    : C11 standard
rem /Iinclude   : header search path
cl /nologo /W4 /utf-8 /std:c11 /Iinclude /Fo:obj\ /Fe:bin\A6.exe src\*.c
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo [OK] Build succeeded: bin\A6.exe
endlocal
