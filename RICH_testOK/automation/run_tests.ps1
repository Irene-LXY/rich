$ErrorActionPreference = "Stop"

[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding -ArgumentList $false

$Root = $PSScriptRoot
$Runner = Join-Path $Root "rich_test.exe"
$BuildScript = Join-Path $Root "build_auto.ps1"

Write-Host "Building test runner..."
& $BuildScript
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$caseFiles = Get-ChildItem -Path (Join-Path $Root "testcases\iteration1\*.json") | Sort-Object Name

if ($caseFiles.Count -eq 0) {
    Write-Error "No test cases found under testcases\iteration1\*.json"
    exit 2
}

$passCount = 0
$failCount = 0
$errorCount = 0

foreach ($case in $caseFiles) {
    $raw = (& $Runner $case.FullName) -join "`n"
    $result = $raw | ConvertFrom-Json

    $status = $result.result
    if ($status -eq "PASS") {
        $passCount++
    } elseif ($status -eq "FAIL") {
        $failCount++
    } else {
        $errorCount++
    }

    Write-Host ""
    Write-Host "=================================================="
    Write-Host ("[{0}] {1}  ->  {2}" -f $case.Name, $result.case_id, $status)
    Write-Host $raw
}

Write-Host ""
Write-Host "=================================================="
Write-Host ("Total: PASS={0} FAIL={1} ERROR={2}" -f $passCount, $failCount, $errorCount)

if ($passCount -eq $caseFiles.Count) {
    Write-Host "Result: all cases PASS"
    exit 0
} else {
    Write-Host "Result: some cases did not pass"
    exit 1
}
