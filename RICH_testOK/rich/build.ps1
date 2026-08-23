$ErrorActionPreference = "Stop"

$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null

if (Test-Path -LiteralPath $vsWhere) {
    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path -LiteralPath $candidate) {
            $vcvars = $candidate
        }
    }
}

if (-not $vcvars) {
    $fallback = "D:\Visual Studio\Vs 2022\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path -LiteralPath $fallback) {
        $vcvars = $fallback
    }
}

if (-not $vcvars) {
    Write-Error "Visual Studio C++ compiler not found."
    exit 1
}

New-Item -ItemType Directory -Force -Path "build" | Out-Null

$sources = @(
    "src\main.c",
    "src\core\game.c",
    "src\startup\startup.c",
    "src\commands\command_dispatcher.c",
    "src\commands\quit_command.c",
    "src\runtime\runtime.c",
    "src\a4\a4_turn_manager.c",
    "src\map\map.c",
    "src\map\game_interfaces.c"
) -join " "

$command = "call `"$vcvars`" >nul && cl /nologo /W3 /std:c11 /utf-8 /I include $sources /Fe:build\monopoly.exe"

cmd.exe /d /c $command

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    exit $LASTEXITCODE
}

Write-Host "Build succeeded: build\monopoly.exe"
