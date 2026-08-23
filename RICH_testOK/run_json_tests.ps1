param(
    [string]$ExePath = ".\json_test_runner.exe",
    [string]$CaseDir = ""
)

$ErrorActionPreference = "Stop"

if ($CaseDir -eq "") {
    $CaseDir = "C:\Users\48181\Documents\Codex\2026-08-21\yi\outputs\iteration1_automation_template\testcases\iteration1"
}

$script:errors = @()

function Add-Error {
    param([string]$Message)
    $script:errors += $Message
}

function Get-PrimaryKey {
    param([string]$ArrayName)
    if ($ArrayName -eq "players") { return "id" }
    if ($ArrayName -in @("properties", "map_items", "display_players")) { return "position" }
    return ""
}

function Find-ByKey {
    param($Array, [string]$KeyName, $ExpectedItem)
    if ($null -eq $Array -or $Array -isnot [System.Array]) { return $null }
    $keyValue = $ExpectedItem.$KeyName
    foreach ($item in $Array) {
        if ($item.$KeyName -eq $keyValue) { return $item }
    }
    return $null
}

function Compare-Array {
    param($Expected, $Actual, [string]$Path, [string]$ArrayName)

    if ($Actual -isnot [System.Array]) {
        Add-Error "$Path expected array"
        return
    }

    $keyName = Get-PrimaryKey $ArrayName
    $i = 0
    foreach ($expectedItem in $Expected) {
        if ($keyName) {
            $actualItem = Find-ByKey $Actual $keyName $expectedItem
            if ($null -eq $actualItem) {
                Add-Error "$Path[$($keyName)=$($expectedItem.$keyName)] not found"
            } else {
                Compare-Node $expectedItem $actualItem "$Path[$($keyName)=$($expectedItem.$keyName)]" $keyName
            }
        } else {
            if ($Actual.Count -gt $i) {
                Compare-Node $expectedItem $Actual[$i] "$Path[$i]" $ArrayName
            } else {
                Add-Error "$Path[$i] not found"
            }
        }
        $i++
    }
}

function Compare-Node {
    param($Expected, $Actual, [string]$Path, [string]$ArrayName = "")

    if ($null -eq $Expected) { return }
    if ($Expected -is [string] -or $Expected -is [bool] -or
        $Expected -is [int] -or $Expected -is [long] -or $Expected -is [double]) {
        if ($Actual -ne $Expected) {
            Add-Error "$Path expected $Expected actual $Actual"
        }
        return
    }
    if ($Expected -is [System.Array]) {
        Compare-Array $Expected $Actual $Path $ArrayName
        return
    }
    if ($Expected -is [System.Management.Automation.PSCustomObject]) {
        if ($Actual -isnot [System.Management.Automation.PSCustomObject]) {
            Add-Error "$Path type mismatch"
            return
        }
        foreach ($prop in $Expected.PSObject.Properties) {
            $name = $prop.Name
            if ($name -eq "properties_absent") {
                foreach ($pos in $Expected.$name) {
                    $found = $false
                    foreach ($p in $Actual.properties) { if ($p.position -eq $pos) { $found = $true } }
                    if ($found) { Add-Error "actual.properties[position=$pos] should be absent" }
                }
                continue
            }
            if ($name -eq "map_items_absent") {
                foreach ($pos in $Expected.$name) {
                    $found = $false
                    foreach ($m in $Actual.map_items) { if ($m.position -eq $pos) { $found = $true } }
                    if ($found) { Add-Error "actual.map_items[position=$pos] should be absent" }
                }
                continue
            }

            $actualChild = $Actual.PSObject.Properties[$name]
            if ($null -eq $actualChild) {
                Add-Error "$Path.$name not found"
                continue
            }
            $expectedValue = $Expected.$name
            if ($expectedValue -is [System.Array]) {
                Compare-Array $expectedValue $actualChild.Value "$Path.$name" $name
            } else {
                Compare-Node $expectedValue $actualChild.Value "$Path.$name" $name
            }
        }
    }
}

$caseFiles = Get-ChildItem -Path (Join-Path $CaseDir "*.json") | Sort-Object Name
$pass = 0
$fail = 0

foreach ($file in $caseFiles) {
    $script:errors = @()
    $case = Get-Content -Path $file.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
    $actualText = (& $ExePath --run-test $file.FullName 2>$null) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("[ERROR] {0}  exit={1}" -f $file.Name, $LASTEXITCODE)
        $fail++
        continue
    }
    $actual = $actualText | ConvertFrom-Json
    Compare-Node $case.expected $actual "actual"

    if ($script:errors.Count -eq 0) {
        Write-Host ("[PASS] {0}  {1}" -f $file.Name, $case.case_id)
        $pass++
    } else {
        Write-Host ("[FAIL] {0}  {1}" -f $file.Name, ($script:errors -join " | "))
        $fail++
    }
}

Write-Host ""
Write-Host ("JSON exe tests: PASS={0} FAIL={1}" -f $pass, $fail)

if ($fail -gt 0) { exit 1 }
exit 0
