[CmdletBinding()]
param(
    [string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$failures = [System.Collections.Generic.List[string]]::new()

function Add-Failure([string]$Message) {
    $failures.Add($Message)
}

function Read-JsonFile([string]$Path) {
    try {
        return Get-Content -Raw -Encoding UTF8 -LiteralPath $Path | ConvertFrom-Json
    } catch {
        Add-Failure "JSON解析失败: $Path ($($_.Exception.Message))"
        return $null
    }
}

$mapPath = Join-Path $ProjectRoot 'spec\map.json'
$map = Read-JsonFile $mapPath
$mapByPosition = @{}
if ($null -ne $map) {
    if ([int]$map.size -ne 70) { Add-Failure 'map.json 的 size 必须为 70。' }
    $cells = @($map.cells)
    if ($cells.Count -ne 70) { Add-Failure "map.json 必须有70个格子，实际为 $($cells.Count)。" }
    $positions = @($cells | ForEach-Object { [int]$_.position } | Sort-Object -Unique)
    if ($positions.Count -ne 70 -or $positions[0] -ne 0 -or $positions[-1] -ne 69) {
        Add-Failure 'map.json 的 position 必须唯一且完整覆盖 0..69。'
    }
    foreach ($parkPosition in 14, 49, 63) {
        $cell = @($cells | Where-Object { [int]$_.position -eq $parkPosition })
        if ($cell.Count -ne 1 -or ([string]$cell[0].type -ne 'PARK')) {
            Add-Failure "位置 $parkPosition 必须为 PARK。"
        }
    }
    $removedTypes = @($cells | Where-Object { [string]$_.type -in @('HOSPITAL', 'JAIL', 'MAGIC_HOUSE') })
    if ($removedTypes.Count -gt 0) { Add-Failure '活动地图仍含 HOSPITAL/JAIL/MAGIC_HOUSE。' }
    foreach ($cell in $cells) { $mapByPosition[[int]$cell.position] = $cell }
}

$caseDir = Join-Path $ProjectRoot 'testcases'
$caseFiles = @(Get-ChildItem -LiteralPath $caseDir -Filter '*.json' -File)
if ($caseFiles.Count -eq 0) { Add-Failure 'testcases 下没有活动 JSON 用例。' }
$suitePath = Join-Path $caseDir 'FirstGroup_Iteration3_State_v2.json'
if (-not (Test-Path -LiteralPath $suitePath)) {
    Add-Failure '缺少第一组迭代三 STATE v2.0 活动用例文件。'
}
$catalogPath = Join-Path $ProjectRoot 'source_cases\FirstGroup_Iteration3_Source_Catalog.json'
$canonicalPath = Join-Path $ProjectRoot 'source_cases\FirstGroup_Iteration3_All_v2.json'
$reportPath = Join-Path $ProjectRoot 'source_cases\FirstGroup_Iteration3_Conversion_Report_v2.json'
$interactiveDir = Join-Path $ProjectRoot 'interactive\cases_v2'
$catalog = Read-JsonFile $catalogPath
$canonical = Read-JsonFile $canonicalPath
$report = Read-JsonFile $reportPath
$stateSuite = Read-JsonFile $suitePath

$catalogCases = if ($null -ne $catalog) { @($catalog.cases) } else { @() }
$canonicalCases = if ($null -ne $canonical) { @($canonical.tests) } else { @() }
$stateCases = if ($null -ne $stateSuite) { @($stateSuite.tests) } else { @() }

if ($null -ne $catalog) {
    if ([int]$catalog.total_cases -ne 288 -or $catalogCases.Count -ne 288) {
        Add-Failure "正向用例目录必须有288条，实际为 $($catalogCases.Count)。"
    }
}

if ($null -ne $canonical) {
    if ([string]$canonical.schema_version -ne '2.0') { Add-Failure '完整用例集 schema_version 必须为2.0。' }
    if ($canonicalCases.Count -ne 288) { Add-Failure "正向v2.0用例集必须有288条，实际为 $($canonicalCases.Count)。" }
    if (@($canonicalCases | Where-Object { [string]$_.mode -eq 'STATE' }).Count -ne 258) { Add-Failure '完整用例集中的STATE数量必须为258。' }
    if (@($canonicalCases | Where-Object { [string]$_.mode -eq 'INTERACTIVE' }).Count -ne 30) { Add-Failure '完整用例集中的INTERACTIVE数量必须为30。' }
}
if ($null -ne $report) {
    if ([int]$report.summary.removed_negative_cases -ne 87) { Add-Failure '转换报告中的已删除负向用例数必须为87。' }
    if ([int]$report.summary.expected_error_cases -ne 0) { Add-Failure '转换报告仍包含负向预期用例。' }
}
if ($null -ne $stateSuite) {
    if ([string]$stateSuite.schema_version -ne '2.0') { Add-Failure 'STATE用例集 schema_version 必须为2.0。' }
    if ($stateCases.Count -ne 258) { Add-Failure "STATE正向活动用例应为258条，实际为 $($stateCases.Count)。" }
}

$allIds = [System.Collections.Generic.HashSet[string]]::new()
$catalogIds = [System.Collections.Generic.HashSet[string]]::new()
$catalogById = @{}
$canonicalById = @{}
foreach ($case in $catalogCases) {
    [void]$catalogIds.Add([string]$case.case_id)
    $catalogById[[string]$case.case_id] = $case
}
foreach ($case in $canonicalCases) {
    $id = [string]$case.case_id
    $canonicalById[$id] = $case
    if ([string]::IsNullOrWhiteSpace($id)) { Add-Failure '完整v2.0用例集中存在空case_id。'; continue }
    if (-not $allIds.Add($id)) { Add-Failure "完整v2.0用例 case_id 重复: $id" }
    if (-not $catalogIds.Contains($id)) { Add-Failure "v2.0用例不在Excel原始目录: $id" }
    elseif ($null -eq $case.source_trace -or
            [string]$case.source_trace.preset_text -cne [string]$catalogById[$id].preset_text -or
            [string]$case.source_trace.input_text -cne [string]$catalogById[$id].input_text -or
            [string]$case.source_trace.expected_text -cne [string]$catalogById[$id].expected_text) {
        Add-Failure "$id 的source_trace与Excel原始目录不一致。"
    }
    if ([string]$case.schema_version -ne '2.0') { Add-Failure "$id 未显式声明schema_version=2.0。" }
    if ($null -ne $case.expected_outcome -or $null -ne $case.expected_error -or
        $null -ne $case.expected_errors -or $null -ne $case.expected_error_code -or
        $null -ne $case.expected_result) { Add-Failure "$id 仍包含已删除的负向用例字段。" }
    $sourceText = $case.source_trace | ConvertTo-Json -Depth 10 -Compress
    if ($sourceText -match '测试类型.{0,3}(反向|负向)|非法输入|错误命令|启动失败') {
        Add-Failure "$id 的原始说明仍明确标注为负向场景。"
    }
    if ($sourceText -match '公园P\((?:位置)?(?:4|13|29|48|54)\)|位置(?:4|13|29|48|54)为公园P') {
        Add-Failure "$id 仍把非公园位置写成公园；合法公园仅为14、49、63。"
    }
    if ($null -eq $case.expected) { Add-Failure "$id 缺少Expected。" }

    if ([string]$case.mode -eq 'STATE') {
        $gameSizeMatch = [regex]::Match([string]$case.source_trace.preset_text, '(?:对局规模[：:]\s*)?([234])人局')
        if ($gameSizeMatch.Success -and @($case.preset.users).Count -ne [int]$gameSizeMatch.Groups[1].Value) {
            Add-Failure "$id 的Excel对局规模与preset.users数量不一致。"
        }
        $phase = [string]$case.expected.phase
        $prompt = $case.expected.pending_prompt
        $allowedPrompts = @('BUY', 'UPGRADE', 'TOOL_SHOP', 'GIFT_SHOP')
        if ($null -ne $prompt -and $prompt -isnot [string]) {
            Add-Failure "$id 的expected.pending_prompt必须是null或字符串枚举，不能是对象。"
        } elseif ($phase -eq 'PROMPT' -and ([string]::IsNullOrWhiteSpace([string]$prompt) -or [string]$prompt -notin $allowedPrompts)) {
            Add-Failure "$id 的expected.phase为PROMPT时必须给出合法pending_prompt。"
        } elseif ($phase -in @('COMMAND', 'ENDED') -and $null -ne $prompt) {
            Add-Failure "$id 的expected.phase为$phase时pending_prompt必须为null或省略。"
        }
    }

    if ($id -like 'Case_A8_*' -and $id -notin @('Case_A8_001', 'Case_A8_002')) {
        $presetText = [string]$case.source_trace.preset_text
        $expectedText = [string]$case.source_trace.expected_text
        $diceMatch = [regex]::Match($presetText, '测试骰子返回[：:]\s*([1-6])')
        if (-not $diceMatch.Success) {
            Add-Failure "$id 未在Excel来源中固定1..6骰子值。"
        } else {
            $sourceDice = [int]$diceMatch.Groups[1].Value
            $jsonDice = @($case.preset.random_control.streams.DICE)
            if ($jsonDice.Count -ne 1 -or [int]$jsonDice[0] -ne $sourceDice) {
                Add-Failure "$id 的Excel骰子值与JSON DICE流不一致。"
            }
        }
        if ($expectedText -match '掷出X|移动X') { Add-Failure "$id 仍用X点断言固定落点。" }

        $roleMatch = [regex]::Match($presetText, '当前角色[：:]\s*([QASJ])')
        if (-not $roleMatch.Success -or [string]$case.preset.current_user -ne $roleMatch.Groups[1].Value) {
            Add-Failure "$id 的Excel当前角色与JSON current_user不一致。"
        }
        $role = if ($roleMatch.Success) { $roleMatch.Groups[1].Value } else { [string]$case.preset.current_user }
        $currentPlayer = @($case.preset.players | Where-Object { [string]$_.id -eq $role })
        $expectedPlayer = @($case.expected.players | Where-Object { [string]$_.id -eq $role })
        $startMatch = [regex]::Match($presetText, '出发位置[：:][^\r\n（(]*[（(](?:位置)?(\d+)[）)]')
        $targetMatch = [regex]::Match($presetText, '预定落点[：:]?[^\r\n（(]*[（(](?:位置)?(\d+)[）)]')
        if ($currentPlayer.Count -ne 1 -or -not $startMatch.Success -or [int]$currentPlayer[0].position -ne [int]$startMatch.Groups[1].Value) {
            Add-Failure "$id 的Excel出发位置与JSON当前玩家位置不一致。"
        }
        if ($expectedPlayer.Count -ne 1 -or -not $targetMatch.Success -or [int]$expectedPlayer[0].position -ne [int]$targetMatch.Groups[1].Value) {
            Add-Failure "$id 的Excel预定落点与JSON Expected位置不一致。"
        }
        if ($diceMatch.Success -and $startMatch.Success -and $targetMatch.Success -and
            @($case.preset.map_items | Where-Object { [string]$_.type -eq 'BLOCK' }).Count -eq 0) {
            $calculated = ([int]$startMatch.Groups[1].Value + [int]$diceMatch.Groups[1].Value) % 70
            if ($calculated -ne [int]$targetMatch.Groups[1].Value) {
                Add-Failure "$id 的固定骰子无法从出发位置到达预定落点。"
            }
        }

        $mentions = [regex]::Matches($presetText, '(无主空地|空地|自己房产|他人房产|公园P|道具屋|礼品屋|矿地|起点)[（(](?:位置)?(\d+)[）)]')
        foreach ($mention in $mentions) {
            $label = $mention.Groups[1].Value
            $position = [int]$mention.Groups[2].Value
            $actualType = if ($mapByPosition.ContainsKey($position)) { [string]$mapByPosition[$position].type } else { '' }
            $valid = switch ($label) {
                { $_ -in @('无主空地', '空地', '自己房产', '他人房产') } { $actualType -like 'LAND_*'; break }
                '公园P' { $actualType -eq 'PARK'; break }
                '道具屋' { $actualType -eq 'TOOL_SHOP'; break }
                '礼品屋' { $actualType -eq 'GIFT_SHOP'; break }
                '矿地' { $actualType -eq 'MINE'; break }
                '起点' { $actualType -eq 'START'; break }
                default { $true }
            }
            if (-not $valid) { Add-Failure "$id 把位置 $position 描述为 $label，但map.json类型为 $actualType。" }
        }

        $requiredPrompt = $null
        if ($expectedText -match '提示是否购买') { $requiredPrompt = 'BUY' }
        elseif ($expectedText -match '提示是否升级') { $requiredPrompt = 'UPGRADE' }
        elseif ($expectedText -match '道具屋购买流程') { $requiredPrompt = 'TOOL_SHOP' }
        elseif ($expectedText -match '礼品屋.*流程') { $requiredPrompt = 'GIFT_SHOP' }
        if ($null -ne $requiredPrompt) {
            if ([string]$case.expected.phase -ne 'PROMPT' -or [string]$case.expected.pending_prompt -ne $requiredPrompt -or
                [string]$case.expected.current_user -ne $role) {
                Add-Failure "$id 的Excel交互预期与JSON phase/pending_prompt/current_user不一致。"
            }
        }
    }
}

$executionSignatures = [System.Collections.Generic.Dictionary[string,string]]::new([System.StringComparer]::Ordinal)
foreach ($case in $canonicalCases) {
    $signature = [ordered]@{
        mode = $case.mode
        launch = $case.launch
        preset = $case.preset
        actions = $case.actions
        expected = $case.expected
    } | ConvertTo-Json -Depth 100 -Compress
    if ($executionSignatures.ContainsKey($signature)) {
        Add-Failure "$($case.case_id) 与 $($executionSignatures[$signature]) 的执行输入和断言完全相同，应合并或补足语义差异。"
    } else {
        $executionSignatures[$signature] = [string]$case.case_id
    }
}

$stateIds = [System.Collections.Generic.HashSet[string]]::new()
foreach ($case in $stateCases) {
    $id = [string]$case.case_id
    if (-not $stateIds.Add($id)) { Add-Failure "STATE case_id重复: $id" }
    if ([string]$case.mode -ne 'STATE') { Add-Failure "$id 在STATE活动文件中mode不是STATE。" }
    $structural = @{ preset = $case.preset; actions = $case.actions; expected = $case.expected } | ConvertTo-Json -Depth 100 -Compress
    if ($structural -match '"remaining_rounds"|"BOMB"\s*:|"status"\s*:\s*"(?:HOSPITAL|JAIL)"|"type"\s*:\s*"BOMB"|"dice_sequence"|"spawned_turn"|"next_spawn_turn"|"next_spawn_in_turns"') {
        Add-Failure "$id 的结构化状态仍包含v2.0废弃字段。"
    }
    if (-not $canonicalById.ContainsKey($id)) {
        Add-Failure "$id 在活动主档中不存在。"
    } else {
        $masterJson = $canonicalById[$id] | ConvertTo-Json -Depth 100 -Compress
        $suiteJson = $case | ConvertTo-Json -Depth 100 -Compress
        if ($masterJson -cne $suiteJson) { Add-Failure "$id 的STATE派生用例与活动主档不一致。" }
    }
}

$interactiveFiles = if (Test-Path -LiteralPath $interactiveDir) { @(Get-ChildItem -LiteralPath $interactiveDir -Filter '*.json' -File) } else { @() }
if ($interactiveFiles.Count -ne 30) { Add-Failure "INTERACTIVE正向派生用例应为30条，实际为 $($interactiveFiles.Count)。" }
foreach ($file in $interactiveFiles) {
    $interactiveCase = Read-JsonFile $file.FullName
    if ($null -eq $interactiveCase) { continue }
    if ($null -eq $interactiveCase.input -or $null -eq $interactiveCase.expect -or $null -eq $interactiveCase.forbid) {
        Add-Failure "$($file.Name) 不是有效的交互运行器用例。"
    }
    $caseId = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
    if (-not $canonicalById.ContainsKey($caseId)) {
        Add-Failure "$($file.Name) 在活动主档中不存在。"
        continue
    }
    $master = $canonicalById[$caseId]
    $expectedInstances = if ($null -ne $master.launch.instances) { [int]$master.launch.instances } else { 1 }
    $expectedRepetitions = if ($null -ne $master.launch.repetitions) { [int]$master.launch.repetitions } else { 1 }
    $actualInstances = if ($null -ne $interactiveCase.instances) { [int]$interactiveCase.instances } else { 1 }
    $actualRepetitions = if ($null -ne $interactiveCase.repetitions) { [int]$interactiveCase.repetitions } else { 1 }
    if ($actualInstances -ne $expectedInstances) {
        Add-Failure "$caseId 的派生instances=$actualInstances，与主档launch.instances=$expectedInstances 不一致。"
    }
    if ($actualRepetitions -ne $expectedRepetitions) {
        Add-Failure "$caseId 的派生repetitions=$actualRepetitions，与主档launch.repetitions=$expectedRepetitions 不一致。"
    }
    $expectedTimeout = if ($null -ne $master.launch.timeout_ms) { [int]$master.launch.timeout_ms } else { 5000 }
    $expectedExitCode = if ($null -ne $master.expected.process.exit_code) { [int]$master.expected.process.exit_code } else { 0 }
    if ([int]$interactiveCase.timeout_ms -ne $expectedTimeout) {
        Add-Failure "$caseId 的派生timeout_ms与主档launch.timeout_ms不一致。"
    }
    if ([int]$interactiveCase.exit_code -ne $expectedExitCode) {
        Add-Failure "$caseId 的派生exit_code与主档expected.process.exit_code不一致。"
    }

    $expectedInput = @($master.actions | Where-Object { [string]$_.command -eq 'INPUT' } | ForEach-Object { [string]$_.params.text })
    $actualInput = @($interactiveCase.input | ForEach-Object { [string]$_ })
    if (($expectedInput | ConvertTo-Json -Compress) -cne ($actualInput | ConvertTo-Json -Compress)) {
        Add-Failure "$caseId 的派生input与活动主档INPUT动作不一致。"
    }
    $contains = @($master.expected.process.stdout.matchers | Where-Object { [string]$_.type -eq 'CONTAINS' } | ForEach-Object { [string]$_.value })
    $notContains = @($master.expected.process.stdout.matchers | Where-Object { [string]$_.type -eq 'NOT_CONTAINS' } | ForEach-Object { [string]$_.value })
    if (($contains | ConvertTo-Json -Compress) -cne (@($interactiveCase.expect) | ConvertTo-Json -Compress)) {
        Add-Failure "$caseId 的派生expect与活动主档CONTAINS断言不一致。"
    }
    if (($notContains | ConvertTo-Json -Compress) -cne (@($interactiveCase.forbid) | ConvertTo-Json -Compress)) {
        Add-Failure "$caseId 的派生forbid与活动主档NOT_CONTAINS断言不一致。"
    }

    $masterInstanceInputs = if ($null -ne $master.launch.instance_inputs) { @($master.launch.instance_inputs) } else { @() }
    $derivedInstanceInputs = if ($null -ne $interactiveCase.instance_inputs) { @($interactiveCase.instance_inputs) } else { @() }
    $masterInstanceResults = if ($null -ne $master.expected.process.instances) { @($master.expected.process.instances) } else { @() }
    $derivedInstanceExpect = if ($null -ne $interactiveCase.instance_expect) { @($interactiveCase.instance_expect) } else { @() }
    $derivedInstanceForbid = if ($null -ne $interactiveCase.instance_forbid) { @($interactiveCase.instance_forbid) } else { @() }
    if ($expectedInstances -gt 1) {
        if ($masterInstanceInputs.Count -ne $expectedInstances -or $derivedInstanceInputs.Count -ne $expectedInstances) {
            Add-Failure "$caseId 必须为每个并发实例提供一组独立输入。"
        } elseif (($masterInstanceInputs | ConvertTo-Json -Depth 20 -Compress) -cne
                  ($derivedInstanceInputs | ConvertTo-Json -Depth 20 -Compress)) {
            Add-Failure "$caseId 的派生instance_inputs与活动主档launch.instance_inputs不一致。"
        }
        if ($masterInstanceResults.Count -ne $expectedInstances -or
            $derivedInstanceExpect.Count -ne $expectedInstances -or
            $derivedInstanceForbid.Count -ne $expectedInstances) {
            Add-Failure "$caseId 必须为每个并发实例提供独立输出断言。"
        } else {
            for ($instanceIndex = 0; $instanceIndex -lt $expectedInstances; $instanceIndex++) {
                $masterContains = @($masterInstanceResults[$instanceIndex].stdout.matchers | Where-Object { [string]$_.type -eq 'CONTAINS' } | ForEach-Object { [string]$_.value })
                $masterNotContains = @($masterInstanceResults[$instanceIndex].stdout.matchers | Where-Object { [string]$_.type -eq 'NOT_CONTAINS' } | ForEach-Object { [string]$_.value })
                if (($masterContains | ConvertTo-Json -Compress) -cne (@($derivedInstanceExpect[$instanceIndex]) | ConvertTo-Json -Compress)) {
                    Add-Failure "$caseId 实例$($instanceIndex + 1)的派生instance_expect与主档不一致。"
                }
                if (($masterNotContains | ConvertTo-Json -Compress) -cne (@($derivedInstanceForbid[$instanceIndex]) | ConvertTo-Json -Compress)) {
                    Add-Failure "$caseId 实例$($instanceIndex + 1)的派生instance_forbid与主档不一致。"
                }
            }
        }
    } elseif ($masterInstanceInputs.Count -gt 0 -or $derivedInstanceInputs.Count -gt 0 -or
              $masterInstanceResults.Count -gt 0 -or $derivedInstanceExpect.Count -gt 0 -or $derivedInstanceForbid.Count -gt 0) {
        Add-Failure "$caseId 只有一个实例，不应使用按实例输入或断言字段。"
    }

    if ($caseId -notin @('Case_A1_001', 'Case_A1_002', 'Case_A1_003')) {
        if ($actualInput.Count -lt 2) {
            Add-Failure "$caseId 缺少PDF规定的“初始资金→角色组合”开局输入。"
        } else {
            $moneyText = [string]$actualInput[0]
            $moneyValue = 0
            if ($moneyText -ne '' -and (-not [int]::TryParse($moneyText, [ref]$moneyValue) -or $moneyValue -lt 1000 -or $moneyValue -gt 50000)) {
                Add-Failure "$caseId 的第一个开局输入必须为空（默认10000）或1000..50000初始资金。"
            }
            $roles = [string]$actualInput[1]
            $roleChars = $roles.ToCharArray()
            if ($roles -notmatch '^[1-4]{2,4}$' -or @($roleChars | Sort-Object -Unique).Count -ne $roleChars.Count) {
                Add-Failure "$caseId 的第二个开局输入必须是2..4位不重复角色组合（如12/123/1234）。"
            }
        }
    }
    if ($expectedInstances -gt 1 -and $derivedInstanceInputs.Count -eq $expectedInstances) {
        for ($instanceIndex = 0; $instanceIndex -lt $expectedInstances; $instanceIndex++) {
            $instanceInput = @($derivedInstanceInputs[$instanceIndex])
            if ($instanceInput.Count -lt 2) {
                Add-Failure "$caseId 实例$($instanceIndex + 1)缺少PDF规定的开局输入。"
                continue
            }
            $instanceMoneyText = [string]$instanceInput[0]
            $instanceMoneyValue = 0
            if ($instanceMoneyText -ne '' -and (-not [int]::TryParse($instanceMoneyText, [ref]$instanceMoneyValue) -or $instanceMoneyValue -lt 1000 -or $instanceMoneyValue -gt 50000)) {
                Add-Failure "$caseId 实例$($instanceIndex + 1)的首项必须为空或1000..50000初始资金。"
            }
            $instanceRoles = [string]$instanceInput[1]
            $instanceRoleChars = $instanceRoles.ToCharArray()
            if ($instanceRoles -notmatch '^[1-4]{2,4}$' -or @($instanceRoleChars | Sort-Object -Unique).Count -ne $instanceRoleChars.Count) {
                Add-Failure "$caseId 实例$($instanceIndex + 1)的第二项必须是2..4位不重复角色组合。"
            }
        }
    }
    $positiveMatcherText = @($master.expected.process.stdout.matchers | Where-Object { [string]$_.type -ne 'NOT_CONTAINS' } | ForEach-Object { [string]$_.value }) -join "`n"
    if ($positiveMatcherText -match '玩家人数|请输入玩家\s*[1-4]\s*的角色') {
        Add-Failure "$caseId 仍断言PDF不存在的独立人数步骤或逐玩家选角提示。"
    }
}

if ($failures.Count -gt 0) {
    Write-Host '[失败] 迭代三测试资产校验未通过：' -ForegroundColor Red
    foreach ($failure in $failures) { Write-Host "  - $failure" -ForegroundColor Red }
    exit 1
}

Write-Host "[通过] v2.0正向测试资产校验完成：288条用例（258条STATE + 30条INTERACTIVE），无完全重复执行且不含负向用例。" -ForegroundColor Green
exit 0
