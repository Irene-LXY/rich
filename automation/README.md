# 大富翁自动化测试（C 版）

本目录是测试人员侧的自动化测试框架，用 C 编写，无第三方依赖。用于在开发完成前就准备好可运行的测试。

它负责：

1. 读取测试用例 JSON；
2. 调用开发人员提供的程序；
3. 按 `大富翁游戏自动化测试JSON接口规范v1.1` 做 Expected 部分匹配判定；
4. 汇总输出 PASS / FAIL / ERROR 报告。

## 目录结构

```
automation/
├── c/
│   ├── run_tests.c        JSON 测试运行器（入口，纯 C）
│   └── README.md          运行器用法
├── CMakeLists.txt         统一构建（自动发现编译器）
├── build.bat              Windows 一键编译
├── interactive/
│   ├── run_interactive_tests.c   交互黑盒运行器（本组本地，纯 C）
│   ├── cases/                    交互用例
│   └── README.md
├── spec/
│   └── map.json          统一地图（70 格）
├── testcases/
│   ├── A4, A7~A21/       正向用例（按 Story 分目录，可跨组共享）
│   └── land, item, turn/ 手写示例用例
└── README.md
```

## 编译与运行（跨组 JSON 测试）

先安装任意 C11 编译器和 CMake，然后：

```powershell
cmake -S . -B build
cmake --build build
```

会生成 `run_tests`（Windows 下为 `run_tests.exe`，多配置生成器下路径可能是
`build\Debug\` 或 `build\Release\`）。然后运行：

```powershell
run_tests.exe --program <被测程序> --cases testcases --map spec\map.json
```

只跑一个用例：

```powershell
run_tests.exe --program <被测程序> --cases testcases\land\TC-LAND-001.json --map spec\map.json
```

常用参数（详见 [`c/README.md`](c/README.md)）：

- `--program`：被测程序命令（必填）。
- `--cases`：测试用例文件或目录（必填）。
- `--map`：地图文件路径。
- `--map-dir`：`map_file` 的解析目录，默认 `<本目录>/spec`。
- `--out`：结果输出文件，默认 `results.json`。
- `--junit`：可选 JUnit XML。
- `--quiet`：只打印汇总。

退出码：全部通过返回 `0`，存在 FAIL 或 ERROR 返回 `1`。

## 程序调用契约（重要）

> 说明：接口规范没有规定“测试程序如何调用游戏程序”，下面的调用方式由测试侧自行定义。

调用方式：

```text
<program> <test_case.json> <map.json>
```

- 第 1 个参数：测试用例 JSON 文件的绝对路径。
- 第 2 个参数：解析后的地图文件绝对路径（测试侧会解析用例中的 `map_file` 并传入）。

程序必须向 stdout 输出一个 JSON 对象，二选一：

1. **完整 Actual 状态**：即接口规范表 28 中 `"actual"` 的值，必须包含
   `users`、`current_user`、`phase`、`pending_prompt`、`game_status`、`winner`、
   `players`、`properties`、`map_items`（可选 `display_players`）。
2. **完整结果报告**：包含 `"result": "PASS" | "FAIL" | "ERROR"` 和 `"errors"` 数组。

程序异常（崩溃、输出非 JSON、无输出）会被判为 ERROR。

## 测试用例 JSON 结构

顶层字段（详见接口规范第 6 节）：

```json
{
  "schema_version": "1.0",
  "case_id": "TC-LAND-001",
  "case_name": "购买地段1空地成功",
  "map_file": "map.json",
  "preset": { },
  "actions": [ ],
  "expected": { },
  "expected_result": "PASS",
  "expected_errors": [ ]
}
```

- `preset`：游戏前置状态（`users`、`current_user`、`phase`、`game_status`、
  `players`、`properties`、`map_items`、`dice_sequence`）。
- `actions`：按顺序执行的命令（`ROLL`、`STEP`、`SELL`、`BLOCK`、`BOMB`、`ROBOT`、
  `QUERY`、`HELP`、`ANSWER`、`QUIT`）。
- `expected`：只写需要验证的字段，未写的字段不参与比较。
- `expected_result`（可选）：缺省为 `PASS`；填 `ERROR` 表示这是一个负向用例，
  期望被测程序报错而不是输出状态。
- `expected_errors`（可选）：配合 `expected_result=ERROR` 使用，列出期望的错误码
  （如 `INVALID_PARAMS`）。运行器会校验程序确实报 `ERROR`，且这些错误码都出现。

示例见 `testcases/`。

> 说明：`expected_result` / `expected_errors` 是测试侧自定扩展（接口规范未规定）。
> 运行器向后兼容，缺少这两个字段时默认按 `PASS` 处理。

## 判定规则（部分匹配）

按接口规范第 11 节实现：

- 标量：完全相等，且类型一致（`true` 不等于 `1`）。
- 对象：递归部分匹配，只比对 `expected` 中写出的键。
- 数组按主键匹配、不按顺序、不比长度：
  - `players` → 主键 `id`
  - `properties` → 主键 `position`
  - `map_items` → 主键 `position`
  - `display_players` → 主键 `position`
- 特殊字段：
  - `properties_absent`：列出的 position 不得存在已购地产。
  - `map_items_absent`：列出的 position 不得存在地图道具。

> 说明：接口规范只点名了上面四个“按主键”的数组。对于其他数组（如 `users`、
> `dice_sequence`），测试侧自行规定为“有序、严格相等”。

失败时错误对象格式（接口规范表 35）：

```json
{
  "code": "ASSERT_NOT_EQUAL",
  "path": "actual.players[id=A].items.BOMB",
  "expected": 0,
  "actual": 1,
  "message": "炸弹放置后背包数量不正确"
}
```

## map.json（统一地图）

> 说明：接口规范只说明“地块类型和特殊地块位置由统一的 map.json 确定”，
> 但没有给出 map.json 的具体结构。下面的结构由测试侧自行定义；其中 70 格位置
> 严格依据客户需求 PDF 的地图推导。

```json
{
  "schema_version": "1.0",
  "size": 70,
  "cells": [
    { "position": 0, "type": "START" },
    { "position": 1, "type": "LAND_1", "price": 200 },
    { "position": 28, "type": "TOOL_SHOP" },
    { "position": 64, "type": "MINE", "points": 60 }
  ]
}
```

70 格权威布局（`cells` 里的 `position` 即地图编号）：

| 编号 | 类型 |
| --- | --- |
| 0 | START |
| 1–13 | LAND_1（地段1） |
| 14 | HOSPITAL |
| 15–27 | LAND_1（地段1） |
| 28 | TOOL_SHOP |
| 29–34 | LAND_2（地段2） |
| 35 | GIFT_SHOP |
| 36–48 | LAND_3（地段3） |
| 49 | JAIL |
| 50–62 | LAND_3（地段3） |
| 63 | MAGIC_HOUSE |
| 64–69 | MINE（矿地） |

矿地点数从上至下为 20、80、100、40、80、60，对应编号 69、68、67、66、65、64。

## 已确认的业务决策

为便于开发侧对齐，测试侧统一采用以下已与需求确认的规则：

- 道具购买点数：路障 50、机器娃娃 30、炸弹 50。
- `Quit`：强制结束整局游戏。
- 过路费不足时：付款方资金扣到负数，房主只收到付款方“实际可支付的部分”。
- 刚好走到监狱才扣留 2 天，路过不停留不入狱。
- 魔法屋：具体效果客户尚未确定，暂按“可配置清单”占位。

## 测试侧自行定义、规范未规定的部分

以下内容在接口规范中没有规定，由测试侧自行定义，后续如需调整请优先改这里：

1. 程序调用方式：`<program> <test_case.json> <map.json>`。
2. 程序 stdout 输出契约（Actual 状态 或 result 报告）。
3. `map.json` 的具体结构。
4. 非主键数组（如 `users`、`dice_sequence`）的“有序、严格相等”比较规则。
5. 进程级错误码 `PROCESS_ERROR`（程序崩溃等，超出规范表 36）。
