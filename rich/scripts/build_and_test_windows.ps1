$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$gccBin = 'C:\msys64\ucrt64\bin'
$gcc = Join-Path $gccBin 'gcc.exe'
$requiredDll = Join-Path $gccBin 'libisl-23.dll'

if (-not (Test-Path -LiteralPath $gcc)) {
    throw "GCC not found: $gcc. Install MSYS2 UCRT64 GCC or use CMake."
}
if (-not (Test-Path -LiteralPath $requiredDll)) {
    throw "Required GCC DLL not found: $requiredDll. Repair the MSYS2 UCRT64 toolchain."
}

# gcc starts cc1.exe; cc1 must find libisl and other DLLs in this directory.
$env:Path = "$gccBin;$env:Path"
$compileRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant())
    $hashBytes = [System.Security.Cryptography.SHA256]::Create().ComputeHash($pathBytes)
    $pathKey = -join ($hashBytes[0..5] | ForEach-Object { $_.ToString('x2') })
    $asciiRoot = Join-Path $env:TEMP ("monopoly_c_project_" + $pathKey)
    if (-not (Test-Path -LiteralPath $asciiRoot)) {
        New-Item -ItemType Junction -Path $asciiRoot -Target $projectRoot | Out-Null
    }
    $linkItem = Get-Item -LiteralPath $asciiRoot
    $linkTarget = $linkItem.Target
    if ($linkTarget -ne $projectRoot) {
        if ($linkItem.LinkType -ne 'Junction') {
            throw "Temporary path exists and is not a junction: $asciiRoot"
        }
        Remove-Item -LiteralPath $asciiRoot -Force
        New-Item -ItemType Junction -Path $asciiRoot -Target $projectRoot | Out-Null
    }
    $compileRoot = $asciiRoot
}
$buildDir = Join-Path $compileRoot 'build-local'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$commonSources = @(
    (Join-Path $compileRoot 'src\core\game.c'),
    (Join-Path $compileRoot 'src\startup\startup.c'),
    (Join-Path $compileRoot 'src\commands\command_dispatcher.c'),
    (Join-Path $compileRoot 'src\commands\quit_command.c')
)
$flags = @(
    '-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
    '-I', (Join-Path $compileRoot 'include')
)

& $gcc @flags @commonSources (Join-Path $compileRoot 'tests\test_a1.c') '-o' (Join-Path $buildDir 'test_a1.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $buildDir 'test_a1.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gcc @flags @commonSources (Join-Path $compileRoot 'tests\test_a20.c') '-o' (Join-Path $buildDir 'test_a20.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $buildDir 'test_a20.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $gcc @flags @commonSources (Join-Path $compileRoot 'src\main.c') '-o' (Join-Path $buildDir 'monopoly.exe')
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host '[PASS] Build completed:' (Join-Path $buildDir 'monopoly.exe')
