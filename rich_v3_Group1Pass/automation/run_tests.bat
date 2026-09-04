@echo off
rem One-click iteration 3 automated test: STATE JSON + interactive CLI JSON.
setlocal EnableExtensions
cd /d "%~dp0"

rem Double-click mode pauses before closing. Pass -NoPause for CI/terminal use.
set "NO_PAUSE="
if /I "%~1"=="-NoPause" set "NO_PAUSE=1"

rem Always perform an incremental build and asset validation. CMake only rebuilds
rem changed sources, so this prevents stale runner binaries without a full rebuild.
echo Checking test environment and incremental build...
where powershell >nul 2>nul
if errorlevel 1 (
    echo [ERROR] PowerShell not found.
    set "FINAL_EXIT=1"
    goto :finish
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build.ps1" -NoPause
if errorlevel 1 goto :buildfail
set "STATE_RUNNER=%~dp0build\run_tests.exe"
set "CLI_RUNNER=%~dp0build\run_interactive_tests.exe"
if not exist "%STATE_RUNNER%" if exist "%~dp0build\Release\run_tests.exe" set "STATE_RUNNER=%~dp0build\Release\run_tests.exe"
if not exist "%STATE_RUNNER%" if exist "%~dp0build\Debug\run_tests.exe" set "STATE_RUNNER=%~dp0build\Debug\run_tests.exe"
if not exist "%CLI_RUNNER%" if exist "%~dp0build\Release\run_interactive_tests.exe" set "CLI_RUNNER=%~dp0build\Release\run_interactive_tests.exe"
if not exist "%CLI_RUNNER%" if exist "%~dp0build\Debug\run_interactive_tests.exe" set "CLI_RUNNER=%~dp0build\Debug\run_interactive_tests.exe"
if not exist "%STATE_RUNNER%" goto :buildfail
if not exist "%CLI_RUNNER%" goto :buildfail

:locate
rem program.txt and interactive_program.txt are optional explicit overrides.
set "STATE_TARGET="
set "CLI_TARGET="
if exist "%~dp0program.txt" set /p STATE_TARGET=<"%~dp0program.txt"
if exist "%~dp0interactive_program.txt" set /p CLI_TARGET=<"%~dp0interactive_program.txt"

rem Explicit override files have priority; otherwise use stable relative locations.
if not defined STATE_TARGET if exist "%~dp0..\rich\build-local\monopoly_test.exe" set "STATE_TARGET=%~dp0..\rich\build-local\monopoly_test.exe"
if not defined CLI_TARGET if exist "%~dp0..\rich\build-local\monopoly.exe" set "CLI_TARGET=%~dp0..\rich\build-local\monopoly.exe"
if not defined CLI_TARGET if exist "%~dp0..\rich\build-local\rich.exe" set "CLI_TARGET=%~dp0..\rich\build-local\rich.exe"

if not defined STATE_TARGET (
    echo [ERROR] monopoly_test.exe not found. Put its full path in program.txt.
    set "FINAL_EXIT=1"
    goto :finish
)
if not exist "%STATE_TARGET%" (
    echo [ERROR] STATE program not found: %STATE_TARGET%
    set "FINAL_EXIT=1"
    goto :finish
)
if not defined CLI_TARGET (
    echo [ERROR] monopoly.exe/rich.exe not found. Put its full path in interactive_program.txt.
    set "FINAL_EXIT=1"
    goto :finish
)
if not exist "%CLI_TARGET%" (
    echo [ERROR] interactive program not found: %CLI_TARGET%
    set "FINAL_EXIT=1"
    goto :finish
)

set "CASE_DIR=%~dp0testcases"
if not exist "%CASE_DIR%\" (
    echo [ERROR] Test-case directory not found: %CASE_DIR%
    set "FINAL_EXIT=1"
    goto :finish
)
set /a "JSON_COUNT=0"
for /r "%CASE_DIR%" %%F in (*.json) do set /a "JSON_COUNT+=1" >nul
if "%JSON_COUNT%"=="0" (
    echo [ERROR] No JSON files found under: %CASE_DIR%
    set "FINAL_EXIT=1"
    goto :finish
)

echo.
echo ===== STATE JSON cases: all files under testcases =====
echo JSON files: %JSON_COUNT% ^(recursive^)
echo Program: %STATE_TARGET%
"%STATE_RUNNER%" --program "%STATE_TARGET%" --cases "%CASE_DIR%" --map "%~dp0spec\map.json" --out "%~dp0results_state_v2.json" --junit "%~dp0junit_state_v2.xml"
set "STATE_EXIT=%ERRORLEVEL%"

echo.
echo ===== INTERACTIVE positive cases: 30 =====
echo Program: %CLI_TARGET%
"%CLI_RUNNER%" --program "%CLI_TARGET%" --cases "%~dp0interactive\cases_v2" --out "%~dp0results_interactive_v2.json" --quiet
set "CLI_EXIT=%ERRORLEVEL%"

echo.
if "%STATE_EXIT%"=="0" if "%CLI_EXIT%"=="0" (
    echo All discovered STATE JSON cases and INTERACTIVE cases passed.
    set "FINAL_EXIT=0"
    goto :finish
)
echo [FAILED] Genuine test result: STATE exit=%STATE_EXIT%, INTERACTIVE exit=%CLI_EXIT%.
echo Reports: results_state_v2.json, results_interactive_v2.json, junit_state_v2.xml
set "FINAL_EXIT=1"
goto :finish

:buildfail
echo [ERROR] Build or test-asset validation failed.
set "FINAL_EXIT=1"

:finish
if not defined NO_PAUSE (
    echo.
    echo Press any key to close this window...
    pause >nul
)
exit /b %FINAL_EXIT%
