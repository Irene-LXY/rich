[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build-a21'

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($null -ne $cmakeCommand) {
    $cmakePath = $cmakeCommand.Source
} else {
    $knownCMakePaths = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )
    $cmakePath = $knownCMakePaths |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($cmakePath)) {
    throw 'CMake was not found. Install CMake or Visual Studio 2022 with C++ CMake tools.'
}

$ctestName = if ($IsLinux -or $IsMacOS) { 'ctest' } else { 'ctest.exe' }
$ctestPath = Join-Path (Split-Path -Parent $cmakePath) $ctestName
if (-not (Test-Path -LiteralPath $ctestPath)) {
    $ctestCommand = Get-Command ctest -ErrorAction SilentlyContinue
    if ($null -eq $ctestCommand) {
        throw 'CTest was not found beside CMake or on PATH.'
    }
    $ctestPath = $ctestCommand.Source
}

Write-Host '[A21] Configuring project...'
& $cmakePath -S $projectRoot -B $buildDirectory
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host '[A21] Building test_a21...'
& $cmakePath --build $buildDirectory --config Release --target test_a21
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host '[A21] Running all 33 Excel cases...'
& $ctestPath --test-dir $buildDirectory -C Release `
    -R '^A21_bankruptcy$' --output-on-failure -V
exit $LASTEXITCODE
