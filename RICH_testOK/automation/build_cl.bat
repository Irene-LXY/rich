@echo off
setlocal

cd /d "%~dp0"

set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSINSTALL="
set "VCVARS="

if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if defined VSINSTALL (
    if exist "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
    if exist "D:\Visual Studio\Vs 2022\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=D:\Visual Studio\Vs 2022\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
    echo Cannot find Visual Studio C++ compiler.
    echo Please install Visual Studio 2022 with the "Desktop development with C++" workload.
    exit /b 1
)

call "%VCVARS%"

cl /nologo /W4 /std:c11 /utf-8 ^
  /I third_party\cjson ^
  /I src ^
  src\test_runner.c ^
  src\game_engine_stub.c ^
  third_party\cjson\cJSON.c ^
  /Fe:rich_test.exe

if %ERRORLEVEL% NEQ 0 (
    echo Build failed. See the error messages above.
    exit /b %ERRORLEVEL%
)

echo Build succeeded: rich_test.exe
endlocal
