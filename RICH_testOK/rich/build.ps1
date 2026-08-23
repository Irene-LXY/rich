$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
Push-Location $Root

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
)

function Try-Gcc {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if (-not $gcc) { return $false }
    Write-Host "Detected compiler: gcc"
    & $gcc.Source -std=c11 -Wall -Wextra -I include @sources -o build\monopoly.exe
    return ($LASTEXITCODE -eq 0)
}

function Try-Clang {
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if (-not $clang) { return $false }
    Write-Host "Detected compiler: clang"
    & $clang.Source -std=c11 -Wall -Wextra -I include @sources -o build\monopoly.exe
    return ($LASTEXITCODE -eq 0)
}

function Try-Msvc {
    $vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vsWhere)) { return $false }

    $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { return $false }

    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path -LiteralPath $vcvars)) { return $false }

    Write-Host "Detected compiler: MSVC"
    $src = $sources -join " "
    $command = "call `"$vcvars`" >nul && cl /nologo /W3 /std:c11 /utf-8 /I include $src /Fe:build\monopoly.exe"
    cmd.exe /d /c $command
    return ($LASTEXITCODE -eq 0)
}

$ok = $false
if (-not $ok) { $ok = Try-Gcc }
if (-not $ok) { $ok = Try-Clang }
if (-not $ok) { $ok = Try-Msvc }

Pop-Location

if ($ok) {
    Write-Host "Build succeeded: build\monopoly.exe"
    exit 0
}

Write-Error "No C compiler found. Install gcc, clang, or Visual Studio C++ build tools."
exit 1
