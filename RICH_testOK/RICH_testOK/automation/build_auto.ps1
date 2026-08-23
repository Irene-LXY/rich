$ErrorActionPreference = "Stop"

$Root = $PSScriptRoot
Push-Location $Root

$SourceFiles = @(
    "src\test_runner.c",
    "src\game_engine_stub.c",
    "third_party\cjson\cJSON.c"
)

$IncludeDirs = @(
    "third_party\cjson",
    "src"
)

$Target = "rich_test.exe"

function Try-Gcc {
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    if (-not $gcc) {
        return $false
    }

    Write-Host "Detected compiler: gcc"
    & $gcc.Source -std=c11 -Wall -Wextra `
        -I "third_party\cjson" `
        -I "src" `
        $SourceFiles `
        -o $Target

    return ($LASTEXITCODE -eq 0)
}

function Try-Clang {
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if (-not $clang) {
        return $false
    }

    Write-Host "Detected compiler: clang"
    & $clang.Source -std=c11 -Wall -Wextra `
        -I "third_party\cjson" `
        -I "src" `
        $SourceFiles `
        -o $Target

    return ($LASTEXITCODE -eq 0)
}

function Try-Msvc {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $false
    }

    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        return $false
    }

    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        return $false
    }

    Write-Host "Detected compiler: MSVC"
    $command = "call `"$vcvars`" >nul && cl /nologo /W4 /std:c11 /utf-8 /I third_party\cjson /I src src\test_runner.c src\game_engine_stub.c third_party\cjson\cJSON.c /Fe:rich_test.exe"
    cmd.exe /d /c $command

    return ($LASTEXITCODE -eq 0)
}

$ok = $false

if (-not $ok) {
    $ok = Try-Gcc
}
if (-not $ok) {
    $ok = Try-Clang
}
if (-not $ok) {
    $ok = Try-Msvc
}

Pop-Location

if ($ok) {
    Write-Host "Build succeeded: rich_test.exe"
    exit 0
} else {
    Write-Error "No usable C compiler was found. Install gcc, clang, or Visual Studio with the Desktop development with C++ workload."
    exit 1
}
