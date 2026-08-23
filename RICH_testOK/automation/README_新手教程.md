# 大富翁命令行游戏 · 自动化测试（C 语言）新手教程

> 配套文件位置：本目录 `src/`、`testcases/`、`third_party/cjson/`。
> 适用人群：测试人员，第一次写自动化测试。
> 目标：在开发人员写出游戏逻辑之前，先把“能自动判断对错”的测试脚本搭起来。

## 1. 先建立正确的心态

这个项目的自动化测试，**不是**让脚本像人一样敲键盘、读屏幕上的彩色地图。
你的 JSON 接口规范已经规定好了一种更稳定的做法：

1. 读入一个 **测试用例 JSON**；
2. 根据用例里的 `preset` 把游戏摆成指定状态；
3. 按顺序执行 `actions` 里的命令；
4. 让游戏把最终状态导出成 `actual` JSON；
5. 拿 `actual` 和用例里的 `expected` 做“部分匹配”；
6. 输出 `PASS`、`FAIL` 或 `ERROR`。

所以你的工作分成三块：

- 写测试用例 JSON（`testcases/`）：这是测试人员最核心的产出；
- 写测试执行器（`src/test_runner.c`）：负责读 JSON、调游戏、比较结果；
- 定义游戏接口（`src/game_engine.h`）：开发人员按这个接口实现真正的游戏逻辑。

## 2. 一个测试用例 JSON 长什么样

最小结构：

```json
{
  "schema_version": "1.0",
  "case_id": "SMOKE-LOAD-001",
  "case_name": "加载前置状态",
  "map_file": "map.json",
  "preset": {},
  "actions": [],
  "expected": {}
}
```

字段含义：

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `schema_version` | 是 | 目前固定 `"1.0"` |
| `case_id` | 是 | 唯一编号，例如 `TC-LAND-001` |
| `case_name` | 是 | 用例名称 |
| `map_file` | 是 | 统一地图文件名，通常是 `map.json` |
| `preset` | 是 | 游戏开始前的状态 |
| `actions` | 是 | 按顺序执行的命令 |
| `expected` | 是 | 只写你需要断言的字段 |

一个带真实前置状态的例子（玩家 A 在 0 号位置）：

```json
{
  "preset": {
    "users": ["A", "Q"],
    "current_user": "A",
    "phase": "COMMAND",
    "game_status": "RUNNING",
    "players": [
      {
        "id": "A",
        "fund": 1000,
        "credit": 0,
        "position": 0,
        "status": "NORMAL",
        "remaining_rounds": 0,
        "items": { "BLOCK": 1, "BOMB": 1, "ROBOT": 0 },
        "god_of_wealth_rounds": 0
      },
      {
        "id": "Q",
        "fund": 1000,
        "credit": 0,
        "position": 5,
        "status": "NORMAL",
        "remaining_rounds": 0,
        "items": { "BLOCK": 0, "BOMB": 0, "ROBOT": 0 },
        "god_of_wealth_rounds": 0
      }
    ],
    "properties": [],
    "map_items": [],
    "dice_sequence": [3]
  },
  "actions": [
    { "command": "ROLL", "params": {} }
  ],
  "expected": {
    "players": [
      { "id": "A", "position": 3 }
    ]
  }
}
```

### 常用 Action

| command | params | 是否结束回合 |
| --- | --- | --- |
| `ROLL` | `{}` | 落点流程完成后 |
| `STEP` | `{"steps": n}` | 落点流程完成后 |
| `SELL` | `{"position": n}` | 否 |
| `BLOCK` | `{"offset": n}` | 否 |
| `BOMB` | `{"offset": n}` | 否 |
| `ROBOT` | `{}` | 否 |
| `QUERY` | `{}` | 否 |
| `HELP` | `{}` | 否 |
| `ANSWER` | `{"value": "Y"}` | 视提示而定 |
| `QUIT` | `{}` | 结束游戏 |

`offset` 范围是 `-10..10`；`SELL`、`BLOCK`、`BOMB`、`ROBOT`、`QUERY`、`HELP` 都不会结束当前玩家的回合。

## 3. 开始之前：准备 C 编译环境

推荐 Windows 下用 MinGW-w64 的 `gcc`，命令行输入：

```bat
gcc --version
```

如果提示找不到命令，先安装 MinGW-w64（或使用你熟悉的 C 编译器）。本模板用标准 C11，不依赖特殊系统库。

## 4. 下载 cJSON（只需一次）

规范里建议 C 语言使用 cJSON。它是一个单文件 JSON 解析库。

请把这两个文件放到 `third_party/cjson/` 目录下：

- `cJSON.c`
- `cJSON.h`

可以从 cJSON 官方仓库下载：

```text
https://github.com/DaveGamble/cJSON
```

下载后目录结构应是：

```text
third_party/
└── cjson/
    ├── cJSON.c
    └── cJSON.h
```

> 如果当前环境无法访问 GitHub，可请开发人员或网络条件好的同事帮忙下载后拷贝这两个文件。

## 5. 编译测试执行器

### 方式一：本机已装 Visual Studio 2022，用 MSVC（推荐）

双击或在 PowerShell 里运行：

```powershell
.\build_cl.bat
```

它内部会调用 Visual Studio 2022 的 `vcvars64.bat` 和 `cl`，不需要 gcc。

> 如果你的 VS 安装在其他盘，请先打开 `build_cl.bat`，修改 `vcvars64.bat` 那一行的路径。

### 方式二：已安装 gcc

在 PowerShell 中进入本目录，执行：

```powershell
gcc -std=c11 -Wall -Wextra -I third_party/cjson -I src `
  src/test_runner.c src/game_engine_stub.c third_party/cjson/cJSON.c `
  -o rich_test.exe
```

也可以直接运行：

```powershell
.\build.ps1
```

## 6. 运行测试

```powershell
.\rich_test.exe testcases\iteration1\smoke_load_state.json
```

同时运行多个用例：

```powershell
.\rich_test.exe testcases\iteration1\*.json
```

输出示例（当前是“红”阶段，游戏引擎还没实现）：

```json
{
  "schema_version": "1.0",
  "case_id": "SMOKE-LOAD-001",
  "result": "ERROR",
  "errors": [
    {
      "code": "NOT_IMPLEMENTED",
      "message": "游戏引擎尚未实现（TDD 红阶段，等待开发人员接入）"
    }
  ]
}
```

这不是坏事。TDD 的顺序本来就是：

1. **红**：先写测试，运行后失败/报错；
2. **绿**：开发人员实现 `game_engine_execute`，测试通过；
3. **重构**：代码保持可维护。

## 7. 测试人员与开发人员怎么协作

测试人员负责维护：

- `testcases/` 下的 JSON 用例；
- `src/test_runner.c` 的校验和比较逻辑；
- `src/game_engine.h` 的接口约定。

开发人员负责实现：

- `src/game_engine.c`（真正的游戏状态机）；
- 把 `game_engine_execute` 接上他们的游戏逻辑。

接口只有一个核心函数：

```c
cJSON *game_engine_execute(
    const cJSON *preset,
    const cJSON *actions,
    char **error_code,
    char **error_message);
```

它读入 `preset` 和 `actions`，返回 `actual` JSON；失败时返回 `NULL` 并给出错误码。

## 8. 每次迭代只写本迭代的用例

不要一次写完全部 Story。按 Story 卡片里的“迭代”列分批：

- 第一迭代：先覆盖规范第 16 节“最低实现范围”里最基础的部分，例如：
  - ROLL / STEP 移动；
  - 地图循环（69 下一格回到 0）；
  - 路障、炸弹途中触发；
  - 购买、升级、出售、租金；
  - 医院、监狱、轮空；
  - 破产和游戏结束。
- 后续迭代：再加道具、财神、礼品屋、魔法屋、矿地点数、玩家重叠显示等。

每个用例建议一个文件，放进对应迭代目录，例如：

```text
testcases/
├── iteration1/
│   ├── smoke_load_state.json
│   ├── turn_roll_moves.json
│   ├── item_two_tools.json
│   └── item_conflict.json
└── iteration2/
    └── ...
```

## 9. 写用例时要特别注意的坑

1. 文件必须是 **UTF-8 无 BOM**，不要有注释、尾随逗号、NaN、Infinity。
2. 整数就是整数，不要写 `800.0`；范围限制在 int32。
3. 布尔只写 `true` / `false`，不要写 `0` / `1`。
4. 空值只写 `null`，不要用 `""` 或 `-1`。
5. 命令、枚举统一大写。
6. `expected` 只写要断言的字段，没写的字段不比较。
7. `players` 按 `id`、`properties` / `map_items` / `display_players` 按 `position` 主键匹配，顺序不重要。
8. 要验证“某个位置没有地产/道具”时，用 `properties_absent`、`map_items_absent`。
9. 不要比较终端字符画、颜色码、空格布局；业务状态以 `players`、`properties`、`map_items` 为准。
10. 每份 JSON 尽量覆盖一个独立业务点，失败时容易定位。

## 10. 你现在的下一步

1. 向团队确认并固定 `map.json`（70 个位置及其类型、价格）。
2. 确认接口规范是否还会更新；如果更新，先锁定 `schema_version`。
3. 从第一迭代的 Story 里挑 5～8 个基础场景，把它们转成 `testcases/iteration1/*.json`。
4. 用 `rich_test.exe` 跑一遍，确认现在全部报 `ERROR`（红）。
5. 把 `src/game_engine.h` 交给开发人员，等他们实现后重新跑，逐步转绿。
