# 通用一键编译 + 测试脚本（Windows）
#
# 适配不同电脑的编译环境与工具：
#   - 以 CMake 为唯一构建来源，由 CMake 自动探测编译器：
#       MSVC (cl.exe，通过 vswhere 自动定位) / MinGW-w64 / MSYS2 (ucrt64|mingw64|clang64) / LLVM clang
#   - PATH 中没有 gcc/clang 时，按常见安装目录自动搜索并临时加入 PATH（MSVC 由 CMake 自行定位）
#   - 项目路径含非 ASCII 字符（如中文用户名）时，自动在 %TEMP% 建 ASCII junction，
#     规避老版本 gcc/cc1 对中文路径的报错
#   - 出错或结束时暂停显示信息，避免闪退
#
# 用法：
#   .\build_and_test_windows.ps1                     # 自动探测工具链，编译并跑全部测试
#   .\build_and_test_windows.ps1 -Generator Ninja     # 显式指定生成器
#   .\build_and_test_windows.ps1 -SkipTest            # 只编译，不跑测试
#   .\build_and_test_windows.ps1 -BuildType Debug     # 指定构建类型

[CmdletBinding()]
param(
    [string]$Generator = '',       # 可选：显式指定生成器，如 "Ninja" / "MinGW Makefiles" / "Visual Studio 17 2022"
    [string]$BuildType = 'Debug',
    [switch]$SkipTest,
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'

function Invoke-Pause {
    if ($NoPause) { return }
    # 仅在交互式控制台暂停；CI/重定向环境不卡住
    if (-not [Environment]::UserInteractive) { return }
    try {
        Write-Host ''
        Read-Host '按回车键退出' | Out-Null
    } catch {
        # 输入被重定向（如 CI / 管道调用），忽略并继续
    }
}

$projectRoot = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# 1. 探测 cmake
# ---------------------------------------------------------------------------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Host '[错误] 未找到 cmake。' -ForegroundColor Red
    Write-Host '  请安装 CMake (https://cmake.org/download/)，安装时勾选"加入 PATH"，然后重新运行。' -ForegroundColor Yellow
    Invoke-Pause
    exit 1
}
$cmakeDir = Split-Path -Parent $cmake.Source

# ---------------------------------------------------------------------------
# 2. PATH 中没有 gcc/clang 时，按常见安装目录自动搜索并临时加入 PATH
#    （MSVC 的 cl.exe 通常不在 PATH，但 CMake 会通过 vswhere 自动定位，无需处理）
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
    $asciiRoot = Join-Path $env:TEMP ("monopoly_c_project_" + $pathKey)

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
# 4. 选择生成器
#    优先"开箱即用"的生成器，避免依赖 vcvars 等外部环境配置：
#      - 有 gcc/clang（MinGW 或 MSYS2）-> MinGW Makefiles，无需任何环境变量
#      - 仅有 MSVC -> Visual Studio 生成器（CMake 会自动配置 INCLUDE/LIB 环境，
#        避免默认的 NMake 生成器因缺 SDK 路径而链接失败）
# ---------------------------------------------------------------------------
$buildDir = Join-Path $compileRoot 'build-local'
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

Write-Host "[1/3] CMake 配置 (生成器: $genLabel, 构建类型: $BuildType) ..." -ForegroundColor Cyan
& cmake -S $compileRoot -B $buildDir @genArgs "-DCMAKE_BUILD_TYPE=$BuildType"
if ($LASTEXITCODE -ne 0) {
    Write-Host '[错误] CMake 配置失败。可能原因：未安装任何 C 编译器。' -ForegroundColor Red
    Write-Host '  请安装以下任一工具链后重试：' -ForegroundColor Yellow
    Write-Host '    - Visual Studio Build Tools (勾选"C++ 桌面开发"工作负载)' -ForegroundColor Yellow
    Write-Host '    - MinGW-w64 或 MSYS2 (ucrt64/mingw64)' -ForegroundColor Yellow
    Write-Host '    - LLVM clang' -ForegroundColor Yellow
    Invoke-Pause
    exit $LASTEXITCODE
}

Write-Host "[2/3] 编译 ..." -ForegroundColor Cyan
& cmake --build $buildDir --config $BuildType
if ($LASTEXITCODE -ne 0) {
    Write-Host '[错误] 编译失败，请查看上方报错信息。' -ForegroundColor Red
    Invoke-Pause
    exit $LASTEXITCODE
}

if (-not $SkipTest) {
    Write-Host "[3/3] 运行测试 (ctest) ..." -ForegroundColor Cyan
    $ctest = Join-Path $cmakeDir 'ctest.exe'
    if (-not (Test-Path $ctest)) { $ctest = (Get-Command ctest -ErrorAction SilentlyContinue).Source }
    if ($ctest) {
        & $ctest --test-dir $buildDir -C $BuildType --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            Write-Host '[警告] 部分测试未通过，详见上方输出。' -ForegroundColor Yellow
        } else {
            Write-Host '[通过] 全部测试通过。' -ForegroundColor Green
        }
    } else {
        Write-Host '[警告] 未找到 ctest，跳过测试。' -ForegroundColor Yellow
    }
}

Write-Host ''
Write-Host "[完成] 构建产物位于: $buildDir" -ForegroundColor Green
Invoke-Pause
exit 0
