# 大富翁迭代三自动化测试环境

本目录只包含测试环境、测试用例和接口规范。本轮没有修改 `rich/` 中的开发程序。

## 一键使用

- 双击工程根目录的 `..\build.bat`：先删除工程内已知构建目录中的已有编译产物，再完整编译游戏程序和两个测试运行器，并校验全部 v2.0 测试资产。
- 双击 `run_tests.bat`：先做增量编译和全部资产校验，然后依次运行 258 条 STATE 正向用例和 30 条 INTERACTIVE 正向用例；显示结果后等待按键，不会自动关闭窗口。
- 命令行/CI 使用 `run_tests.bat -NoPause`；288 条全部通过时返回 `0`，任一 FAIL/ERROR 时返回 `1`。
- 自动找不到程序时，可用 `program.txt` 指定 `monopoly_test.exe`，用 `interactive_program.txt` 指定 `monopoly.exe` 或 `rich.exe`。

失败报告不会被掩盖或改写：`results_state_v2.json`、`results_interactive_v2.json` 和 `junit_state_v2.xml` 保留开发程序当前的真实结果。

当前运行器对开发人员9.4版测试入口做两项兼容处理：将 v2.0 的
`random_control.streams.DICE` 投影为旧入口可读取的临时骰子序列；当 Actual 未输出
`display_cells` 时，仅依据 Actual 状态和标准地图推导显示单元。投影只发生在运行时
临时副本中，不改活动 JSON，也不读取 Expected 来构造 Actual。

开发侧 `monopoly_test.exe` 现已实现 STATE 专用 `ADVANCE_TURN`，并支持其所需的
`turn_number`、财神 Preset、具名随机流和 Actual 快照；该命令不会注册到正常玩家
命令行。实现调用既有回合与财神逻辑，不模拟移动或落点，也不读取 Expected 来构造
Actual。

## v2.0 测试资产

```text
automation/
├── run_tests.bat
├── c/run_tests.c
├── interactive/run_interactive_tests.c
├── scripts/validate_test_assets.ps1
├── scripts/clean_build_artifacts.ps1
├── spec/
│   ├── game_test_spec_v2.md
│   └── map.json
├── source_cases/
│   ├── FirstGroup_Iteration3_All_v2.json
│   ├── FirstGroup_Iteration3_Source_Catalog.json
│   ├── FirstGroup_Iteration3_Conversion_Report_v2.json
│   └── *_PreV2.json
├── testcases/
│   └── *.json（运行时递归读取全部文件）
└── interactive/cases_v2/
    └── 30 个命令行交互 JSON
```

`FirstGroup_Iteration3_All_v2.json` 是288条正向用例的活动主档。运行资产按接口能力拆分：258条状态接口用例由 `run_tests.exe` 执行，30条启动、正常输入、帮助、显示和真实Quit输入用例由 `run_interactive_tests.exe` 执行。除已删除87条负向案例外，本轮又合并18条完全重复或被其他功能完整覆盖的执行；校验器会阻止新的完全重复执行进入活动主档。保留用例仍含 `source_trace`、`adaptation_note` 和合并追踪。

转换前的目录和旧问题清单保留为 `*_PreV2.json`，便于审计。旧的 202 条活动套件已经按用户授权删除并由 v2.0 套件取代。

STATE 运行器只会递归扫描 `testcases/` 下的全部 `.json`；文件名可以任意，既支持顶层
`tests` 数组的用例集，也支持单条用例 JSON。新增用例只需放进该目录或其子目录，
一键脚本会自动发现；不同文件之间的 `case_id` 仍必须保持唯一。其他目录中的 JSON
不会作为 STATE 用例执行。

## 规范要点

- `schema_version` 为 `2.0`。
- `step.steps` 可大于 70；仅当步数大于 70 时按 `steps % 70` 计算实际移动量。
- 活动资产只包含正向用例，不使用 `expected_outcome: "ERROR"` 或 `expected_error`。
- 财神随机性通过 `random_control` 固定，确保用例可重复。
- 迭代三地图中原监狱、医院、魔法屋位置均按公园处理；用例结构不再使用炸弹、住院、入狱、轮空字段。
- 多实例、重复启动等启动层语义在主档 `launch` 中显式保留，并由交互运行器实际执行；派生交互用例分别使用 `instances`、`instance_inputs`、`instance_expect` 与 `repetitions`，可以逐进程输入和断言。
- `pending_prompt` 统一为 `null` 或字符串枚举；`PROMPT` 阶段必须明确提示类型。
- 每条交互派生用例显式携带 `timeout_ms` 与预期 `exit_code`，防止程序挂起或异常退出被误判。
- 开局严格按PDF执行：先设置初始资金，再一次输入 `12`/`123`/`1234` 等不重复角色组合；组合长度决定人数，不存在独立人数步骤或逐玩家选角步骤。
- `Quit` 严格按PDF定义为强制结束整局游戏，不表示当前玩家退出后让其他玩家继续；活动A20保留正常命令、财神状态以及BUY/UPGRADE/TOOL_SHOP/GIFT_SHOP四类提示处理器。

完整字段说明见 `spec/game_test_spec_v2.md`，转换取舍见 `source_cases/FirstGroup_Iteration3_Conversion_Report_v2.json`。尚未实现的检查点式交互能力见 `spec/known_open_questions.md`。

## 单独运行

```powershell
..\build.bat
.\build\run_tests.exe --program <monopoly_test.exe> --cases .\testcases --map .\spec\map.json --out .\results_state_v2.json
.\build\run_interactive_tests.exe --program <monopoly.exe> --cases .\interactive\cases_v2 --out .\results_interactive_v2.json --quiet
```

只校验 JSON 与地图资产：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate_test_assets.ps1
```
