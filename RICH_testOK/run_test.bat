@echo off
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_json_tests.ps1" -ExePath "%~dp0rich\build\monopoly.exe"
pause
