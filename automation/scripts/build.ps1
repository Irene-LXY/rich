# 通用一键编译脚本（Windows）——测试侧自动化框架
#
# 只负责编译出 run_tests.exe 与 run_interactive_tests.exe。
# 适配不同电脑的编译环境与工具：
#   - 以 CMake 为唯一构建来源，由 CMake 自动探测编译器：
#       MSVC (cl.exe) / MinGW-w64 / MSYS2 / LLVM clang
#   - 有 gcc/clang 时优先用 MinGW Makefiles（无需 vcvars，开箱即用）；
#     仅有 MSVC 时用 Visual Studio 生成器（自动配置 INCLUDE/LIB 环境），
#     避免默认的 NMake 生成器因缺 SDK 路径而链接失败、窗口闪退
#   - PATH 中没有 gcc/clang 时按常见安装目录自动搜索
#   - 项目路径含非 ASCII 字符时自动建 ASCII junction
#   - 检测到旧缓存生成器不一致时自动清理，避免 CMake 报 "generator mismatch"
#
# 用法：
#   .\build.ps1                  # 自动探测工具链并编译，结束时暂停
#   .\build.ps1 -NoPause         # 不暂停（供 run_tests.bat / run_interactive.bat 内部调用）
#   .\build.ps1 -Generator Ninja # 显式指定生成器

[CmdletBinding()]
param(
    [string]$Generator = '',
    [string]$BuildType = 'Release',
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'

function Invoke-Pause {
    if ($NoPause) { return }
    if (-not [Environment]::UserInteractive) { return }
    try {
        Write-Host ''
        Read-Host '按回车键退出' | Out-Null
    } catch {
        # 输入被重定向（如 CI），忽略并继续
    }
}

$projectRoot = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# 1. 探测 cmake
# ---------------------------------------------------------------------------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host '[错误] 未找到 cmake。' -ForegroundColor Red
    Write-Host '  请安装 CMake (https://cmake.org/download/)，勾选"加入 PATH"。' -ForegroundColor Yellow
    Invoke-Pause
    exit 1
}

# ---------------------------------------------------------------------------
# 2. PATH 中没有 gcc/clang 时，按常见安装目录自动搜索并临时加入 PATH
#    （MSVC 的 cl.exe 通常不在 PATH，但 CMake 会通过 vswhere 自动定位）
# ---------------------------------------------------------------------------
function Test-CompilerInPath {
    foreach ($name in 'gcc', 'clang', 'cc') {
        if (Get-Command $name -ErrorAction SilentlyContinue) { return $true }
    }
    return $false
}

if (-not (Test-CompilerInPath)) {
    $candidates = @(
        'C:\msys64\ucrt64\bin',
        'C:\msys64\mingw64\bin',
        'C:\msys64\clang64\bin',
        'C:\MinGW\bin',
        'C:\MinGW\mingw64\bin',
        'C:\mingw64\bin',
        'C:\TDM-GCC-64\bin',
        'C:\Program Files\LLVM\bin'
    )
    $found = $null
    foreach ($dir in $candidates) {
        if ((Test-Path (Join-Path $dir 'gcc.exe')) -or (Test-Path (Join-Path $dir 'clang.exe'))) {
            $found = $dir
            break
        }
    }
    if ($found) {
        Write-Host "[提示] PATH 中未发现编译器，自动加入: $found" -ForegroundColor Cyan
        $env:Path = "$found;$env:Path"
    } else {
        Write-Host '[提示] PATH 中未发现 gcc/clang；若已安装 MSVC，CMake 会自动定位。' -ForegroundColor DarkGray
    }
}

# ---------------------------------------------------------------------------
# 3. 项目路径含非 ASCII 时，用 %TEMP% 下的 ASCII junction 代替
# ---------------------------------------------------------------------------
$compileRoot = $projectRoot
if ($projectRoot -match '[^\x00-\x7F]') {
    $pathBytes = [System.Text.Encoding]::UTF8.GetBytes($projectRoot.ToLowerInvariant())
    $hashBytes = [System.Security.Cryptography.SHA256]::Create().ComputeHash($pathBytes)
    $pathKey = -join ($hashBytes[0..5] | ForEach-Object { $_.ToString('x2') })
    $asciiRoot = Join-Path $env:TEMP ("rich_automation_" + $pathKey)

    if (Test-Path -LiteralPath $asciiRoot) {
        $item = Get-Item -LiteralPath $asciiRoot -ErrorAction SilentlyContinue
        if ($null -eq $item -or $item.LinkType -ne 'Junction' -or $item.Target -ne $projectRoot) {
            Remove-Item -LiteralPath $asciiRoot -Force -ErrorAction SilentlyContinue
        }
    }
    if (-not (Test-Path -LiteralPath $asciiRoot)) {
        New-Item -ItemType Junction -Path $asciiRoot -Target $projectRoot | Out-Null
    }
    Write-Host "[提示] 项目路径含中文，改用临时 ASCII 路径: $asciiRoot" -ForegroundColor Cyan
    $compileRoot = $asciiRoot
}

# ---------------------------------------------------------------------------
# 4. 选择生成器（优先"开箱即用"的生成器）
# ---------------------------------------------------------------------------
$buildDir = Join-Path $compileRoot 'build'
$genArgs = @()
$genLabel = '自动探测'
if ($Generator) {
    $genArgs = @('-G', $Generator)
    $genLabel = $Generator
} elseif ((Get-Command gcc -ErrorAction SilentlyContinue) -or (Get-Command clang -ErrorAction SilentlyContinue)) {
    $genArgs = @('-G', 'MinGW Makefiles')
    $genLabel = 'MinGW Makefiles (gcc/clang)'
} else {
    $vsMatch = (& cmake --help | Select-String -Pattern 'Visual Studio \d+ \d+' | Select-Object -First 1)
    if ($vsMatch) {
        $vsName = $vsMatch.Matches[0].Value
        $genArgs = @('-G', $vsName)
        $genLabel = $vsName
    }
}

# ---------------------------------------------------------------------------
# 5. 清理不一致的旧缓存（避免 CMake 报 "generator mismatch"）
# ---------------------------------------------------------------------------
$chosenGenName = if ($genArgs.Count -ge 2) { $genArgs[1] } else { '' }
$cacheFile = Join-Path $buildDir 'CMakeCache.txt'
if ($chosenGenName -and (Test-Path -LiteralPath $cacheFile)) {
    $cachedGen = (Select-String -Path $cacheFile -Pattern '^CMAKE_GENERATOR:INTERNAL=(.*)$' | Select-Object -First 1)
    if ($cachedGen) {
        $cachedGenName = $cachedGen.Matches[0].Groups[1].Value
        if ($cachedGenName -and $cachedGenName -ne $chosenGenName) {
            Write-Host "[提示] 旧缓存生成器 '$cachedGenName' 与本次 '$chosenGenName' 不一致，清理缓存。" -ForegroundColor Cyan
            Remove-Item -LiteralPath $cacheFile -Force -ErrorAction SilentlyContinue
            Remove-Item -LiteralPath (Join-Path $buildDir 'CMakeFiles') -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# ---------------------------------------------------------------------------
# 6. 配置 + 编译
# ---------------------------------------------------------------------------
Write-Host "[1/2] CMake 配置 (生成器: $genLabel, 构建类型: $BuildType) ..." -ForegroundColor Cyan
# CMAKE_RUNTIME_OUTPUT_DIRECTORY 让 exe 固定输出到 build\ 根目录，
# 这样无论单配置(MinGW/NMake)还是多配置(VS)生成器，run_*.bat 都能在 build\ 下找到 exe
& cmake -S $compileRoot -B $buildDir @genArgs "-DCMAKE_BUILD_TYPE=$BuildType" "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$buildDir"
if ($LASTEXITCODE -ne 0) {
    Write-Host '[错误] CMake 配置失败。可能原因：未安装任何 C 编译器。' -ForegroundColor Red
    Write-Host '  请安装以下任一工具链后重试：' -ForegroundColor Yellow
    Write-Host '    - Visual Studio Build Tools (C++ 桌面开发)' -ForegroundColor Yellow
    Write-Host '    - MinGW-w64 或 MSYS2' -ForegroundColor Yellow
    Write-Host '    - LLVM clang' -ForegroundColor Yellow
    Invoke-Pause
    exit $LASTEXITCODE
}

Write-Host "[2/2] 编译 ..." -ForegroundColor Cyan
& cmake --build $buildDir --config $BuildType
if ($LASTEXITCODE -ne 0) {
    Write-Host '[错误] 编译失败，请查看上方报错信息。' -ForegroundColor Red
    Invoke-Pause
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host "[完成] 构建产物位于: $buildDir" -ForegroundColor Green
Invoke-Pause
exit 0
