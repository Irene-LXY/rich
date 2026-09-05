# Removes only known build-output directories inside this project.
# This script intentionally does not scan or delete arbitrary folders.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProjectRoot
)

$ErrorActionPreference = 'Stop'

try {
    $root = [System.IO.Path]::GetFullPath($ProjectRoot).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )
    if (-not (Test-Path -LiteralPath $root -PathType Container)) {
        throw "Project root does not exist: $root"
    }

    $rootPrefix = $root + [System.IO.Path]::DirectorySeparatorChar
    $targets = @(
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build-local')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build-codex')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build-release')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build_c')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'rich\build_cpp')),
        [System.IO.Path]::GetFullPath((Join-Path $root 'automation\build'))
    )

    foreach ($target in $targets) {
        if (-not $target.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean a path outside the project root: $target"
        }

        if (-not (Test-Path -LiteralPath $target)) {
            Write-Host "[SKIP] No existing build directory: $target" -ForegroundColor DarkGray
            continue
        }

        $item = Get-Item -LiteralPath $target -Force
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            $linkTarget = [System.IO.Path]::GetFullPath([string]$item.Target)
            if (-not $linkTarget.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove a build link that points outside the project: $target -> $linkTarget"
            }
            [System.IO.Directory]::Delete($target, $false)
        } else {
            Remove-Item -LiteralPath $target -Recurse -Force
        }

        if (Test-Path -LiteralPath $target) {
            throw "Build directory still exists after cleanup: $target"
        }
        Write-Host "[CLEAN] Removed: $target" -ForegroundColor Green
    }

    Write-Host '[DONE] Existing build artifacts were removed. Starting a full rebuild.' -ForegroundColor Green
    exit 0
} catch {
    Write-Host "[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
