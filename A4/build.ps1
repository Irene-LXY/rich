param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$RunDemo
)

$ErrorActionPreference = "Stop"

function Find-VsWhere {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }
    throw "vswhere.exe was not found. Install the Visual Studio 2022 Desktop development with C++ workload."
}

function Find-VisualStudio {
    $vswhere = Find-VsWhere
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $vswhere
    $startInfo.Arguments = "-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8"
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    $startInfo.StandardOutputEncoding = $utf8
    $startInfo.StandardErrorEncoding = $utf8

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "vswhere could not be started."
    }

    $json = $process.StandardOutput.ReadToEnd()
    $errorText = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "vswhere failed with exit code $($process.ExitCode): $errorText"
    }

    $instances = $json | ConvertFrom-Json
    if (-not $instances -or -not $instances[0].installationPath) {
        throw "Visual Studio with the MSVC toolchain was not found. Install the Desktop development with C++ workload."
    }
    return [string]$instances[0].installationPath
}

$projectDir = $PSScriptRoot
$buildDir = Join-Path $projectDir "build"
$vsDir = Find-VisualStudio
$vcvars = Join-Path $vsDir "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vsDir "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$ctest = Join-Path $vsDir "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
$ninja = Join-Path $vsDir "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"

foreach ($requiredFile in @($vcvars, $cmake, $ctest, $ninja)) {
    if (-not (Test-Path -LiteralPath $requiredFile)) {
        throw "Required build tool was not found: $requiredFile"
    }
}

Write-Host "Visual Studio: $vsDir"
Write-Host "CMake:        $cmake"
Write-Host "Configuration: $Configuration"

$commands = @(
    ('call "{0}" >nul' -f $vcvars),
    ('"{0}" -S "{1}" -B "{2}" -G Ninja -DCMAKE_MAKE_PROGRAM="{3}" -DCMAKE_BUILD_TYPE={4}' -f $cmake, $projectDir, $buildDir, $ninja, $Configuration),
    ('"{0}" --build "{1}"' -f $cmake, $buildDir),
    ('"{0}" --test-dir "{1}" --output-on-failure' -f $ctest, $buildDir)
)

if ($RunDemo) {
    $commands += ('"{0}"' -f (Join-Path $buildDir "a4_demo.exe"))
}

$commandLine = $commands -join " && "
& $env:ComSpec /d /s /c $commandLine
if ($LASTEXITCODE -ne 0) {
    throw "Build or test failed with exit code $LASTEXITCODE."
}

Write-Host ""
Write-Host "Build and tests completed."
Write-Host "Test executable: $(Join-Path $buildDir 'a4_turn_manager_tests.exe')"
Write-Host "Demo executable: $(Join-Path $buildDir 'a4_demo.exe')"
