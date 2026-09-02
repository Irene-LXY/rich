---
title: "大富翁游戏自动化测试 JSON 接口规范 v2.0"
schema_version: "2.0"
encoding: "UTF-8"
source: "大富翁游戏自动化测试JSON接口规范v2.0.docx"
---

> 本文件是供测试人员和AI读取的Markdown版本。规范语义与同名DOCX一致。

技术规范  ·  自动化测试  ·  跨语言

# 大富翁游戏自动化测试 JSON 接口规范 v2.0

> 以 schema 1.0 核心结构为基线，覆盖迭代三需求与一键自动化测试扩展

| 项目 | 内容 |
| --- | --- |
| 规范版本 | 2.0 |
| 发布日期 | 2026-09-01 |
| 适用对象 | 测试人员、C/C++/Python 开发人员、自动化测试运行器维护人员 |
| 兼容基线 | 保留1.0的 case_id / preset / actions / expected 主结构；2.0文件必须显式写 schema_version=2.0 |
| 目标 | 同一份JSON可确定地驱动状态测试、命令行进程测试和交互测试，并产生可比较结果 |

### 版本标识说明

| 标识 | 含义 |
| --- | --- |
| 【沿用1.0】 | 原1.0规则继续有效，仅做文字澄清。 |
| 【2.0新增】 | 2.0首次定义的字段、模式、命令或比较能力。 |
| 【2.0变更】 | 1.0已有规则在2.0中改变；实现必须按2.0执行。 |
| 【2.0删除】 | 因迭代三需求删除；2.0中不得作为合法正向状态使用。 |

> 兼容性结论 2.0不是1.0文件的无条件向后兼容模式。运行器应按 schema_version 选择校验器；2.0不得静默接受已删除字段。

## 0. 2.0变更总览

| 类别 | 2.0处理 | 标识 |
| --- | --- | --- |
| 测试文件 | 增加测试集包装和mode | 新增 |
| 测试能力 | 增加STATE、PROCESS、INTERACTIVE三种模式 | 新增 |
| 随机性 | 增加按流控制的确定性随机序列/PRNG | 新增 |
| 终端测试 | 增加输入、输出、退出码、终端尺寸和ANSI归一化断言 | 新增 |
| 地图显示 | 增加display_cells，区分底层地块与覆盖符号 | 新增 |
| 财神 | 定义地图生成、保留、领取、失效、再次生成和当轮免租 | 新增 |
| 公园 | PARK成为正式地图类型；位置14、49、63 | 变更 |
| 命令大小写 | 输入命令不区分ASCII大小写，导出枚举仍统一大写 | 变更 |
| 废弃功能 | 删除BOMB、HOSPITAL、JAIL、MAGIC_HOUSE、remaining_rounds | 删除 |

## 1. 目的和适用范围

> 【沿用1.0】 统一测试文件结构、游戏状态、动作、Actual导出和Expected比较；不限制程序内部类设计、模块划分或语言。

- 适用于2～4名玩家、70格统一地图以及C、C++、Python等不同语言实现。

- 游戏实现必须通过正式业务逻辑执行Action，不得为测试用例编写特判。

- 测试运行器负责解析、校验、启动被测程序、比较Expected并生成报告。

> 【2.0新增】 规范同时覆盖状态注入测试、真实命令行进程测试和逐步交互测试，解决启动、帮助、地图字符和终端宽度无法表达的问题。

## 2. 跨语言兼容原则

### 2.1 类型与编码

> 【沿用1.0】 整数使用JSON integer并限制在int32；布尔只允许true/false；空值只用null；文件统一UTF-8无BOM；禁止注释、尾随逗号、NaN和Infinity。

- 状态字段和输出枚举区分大小写并统一为大写。

- JSON对象成员顺序无意义；数组按本规范指定主键匹配。

- 字符串默认逐字比较；PROCESS/INTERACTIVE输出可按16.3规则归一化。

### 2.2 输入命令大小写

> 【2.0变更】 command作为输入时按ASCII不区分大小写，解析后统一转换为大写。因此QUIT、Quit、quit语义相同；状态枚举、错误码和Actual输出仍必须大写。

### 2.3 确定性

> 【2.0新增】 所有影响业务结果的随机行为必须从具名随机流读取。公共用例优先使用SEQUENCE模式，避免依赖语言自带随机库。

## 3. 测试模式

| mode | 用途 | 执行方式 | 2.0状态 |
| --- | --- | --- | --- |
| STATE | 地产、道具、移动、回合、财神等状态逻辑 | 注入Preset，执行游戏Action，导出Actual | 新增名称；继承1.0默认方式 |
| PROCESS | 启动参数、退出码、一次性命令行输出 | 启动真实可执行程序并捕获stdout/stderr | 新增 |
| INTERACTIVE | 角色选择、资金设置、Help、地图显示、终端尺寸 | 保持进程，按顺序输入并在检查点断言输出 | 新增 |

未写mode时，2.0校验器按STATE处理。STATE模式不得包含launch；PROCESS/INTERACTIVE不得使用完整游戏状态Preset，除非项目另行实现公开的状态注入启动参数。

## 4. 测试文件外层结构

### 4.1 单用例文件

```json
{
  "schema_version": "2.0",
  "case_id": "TC-PARK-001",
  "case_name": "STEP到达公园",
  "mode": "STATE",
  "map_file": "map.json",
  "preset": {},
  "actions": [],
  "expected": {}
}
```

### 4.2 测试集文件

> 【2.0新增】 一个JSON文件可以包含tests数组。内层用例继承外层schema_version、suite和默认map_file；内层同名字段优先。

```json
{
  "schema_version": "2.0",
  "suite": "iteration3",
  "map_file": "map.json",
  "tests": [
    {"case_id": "TC-001", "case_name": "示例", "mode": "STATE",
     "preset": {}, "actions": [], "expected": {}}
  ]
}
```

| 字段 | 类型 | 必填 | 含义 | 版本标记 |
| --- | --- | --- | --- | --- |
| schema_version | string | 是 | 固定为2.0 | 变更 |
| case_id | string | 是 | 全测试集唯一编号 | 沿用 |
| case_name | string | 是 | 可读名称 | 沿用 |
| mode | enum | 否 | STATE/PROCESS/INTERACTIVE，缺省STATE | 新增 |
| map_file | string | STATE必填 | 统一地图文件 | 沿用 |
| preset | object | STATE必填 | 前置状态；进程模式可为空 | 沿用/扩展 |
| launch | object | 进程模式必填 | 启动参数、环境和终端设置 | 新增 |
| actions | array | 是 | 顺序执行的游戏或测试控制动作 | 沿用/扩展 |
| expected | object | 是 | 状态或进程输出断言 | 沿用/扩展 |
| tags | array<string> | 否 | 过滤、分组、迭代标记 | 新增 |

## 5. 游戏基本数据

### 5.1 玩家与回合顺序

> 【沿用1.0】 每局2～4名玩家；users顺序即回合顺序；角色只能为Q、A、S、J且不可重复；BANKRUPT玩家跳过。

| 字段 | 类型 | 规则 | 版本标记 |
| --- | --- | --- | --- |
| id | enum | Q/A/S/J | 沿用 |
| fund | int32 | 现金，可为0，不得为负 | 沿用 |
| credit | int32 | 点数，可为0，不得为负 | 沿用 |
| position | integer | 0..69 | 沿用 |
| status | enum | NORMAL/BANKRUPT | 变更 |
| items.BLOCK | integer | 0..10 | 沿用 |
| items.ROBOT | integer | 0..10 | 沿用 |
| god_of_wealth_rounds | integer | 0..5；见14.5 | 沿用并明确 |

### 5.2 地图

地图固定70格，位置0..69，顺时针移动，下一位置为(position+1)%70。地图类型由map.json唯一确定。

| 类型 | 含义 | 版本标记 |
| --- | --- | --- |
| START | 起点 | 沿用 |
| LAND_1 | 地段一 | 沿用 |
| LAND_2 | 地段二 | 沿用 |
| LAND_3 | 地段三 | 沿用 |
| TOOL_SHOP | 道具屋 | 沿用 |
| GIFT_SHOP | 礼品屋 | 沿用 |
| PARK | 公园，到达和经过均无事件 | 新增 |
| MINE | 矿地 | 沿用 |

> 【2.0变更】 统一地图位置14、49、63必须为PARK。测试文件不得自行把4、13、29、48、54等其他位置描述为公园。

### 5.3 地产与道具

properties按position唯一；level为0..3；价格、升级、租金和出售计算继续沿用1.0。地图道具map_items只允许BLOCK，同一位置最多一个。

> 【2.0删除】 删除BOMB道具、BOMB命令和地图BOMB。玩家背包不得含items.BOMB；旧字段必须返回INVALID_PRESET，旧命令必须返回INVALID_COMMAND。

### 5.4 玩家状态

> 【2.0删除】 删除HOSPITAL、JAIL及remaining_rounds。状态只允许NORMAL和BANKRUPT；不存在住院、入狱或轮空流程。

## 6. 回合与阶段

| phase | 含义 | 允许的游戏Action |
| --- | --- | --- |
| COMMAND | 等待正式命令 | ROLL、STEP、SELL、BLOCK、ROBOT、QUERY、HELP、QUIT |
| PROMPT | 等待购买/升级/商店回答 | ANSWER、QUERY、HELP、QUIT |
| ENDED | 游戏结束 | 不允许游戏Action |

SELL、BLOCK、ROBOT、QUERY、HELP不结束回合。ROLL或合法STEP完成逐格移动、途中道具、落点、交互及结算后结束回合。

> 【2.0新增】 STATE模式提供测试控制Action ADVANCE_TURN，用于原地结束当前玩家回合并推进计时器，不触发移动或落点。它不是玩家可输入的游戏命令。

> 【2.0变更】 STEP的steps允许大于70。steps必须是1..2147483647的整数；当steps>70时，先计算effective_steps=steps%70，再按effective_steps执行逐格移动。示例：69→69步，70→70步，71→1步，140→0步。

0、负数、小数、字符串、null或缺失仍返回INVALID_PARAMS；不得用STEP 0模拟时间。取余结果为0是由合法的steps>70产生的有效移动距离，不等同于输入STEP 0。

## 7. 确定性随机控制

> 【2.0新增】 Preset增加random_control。每类随机行为使用独立具名流，避免某一功能多取一次随机数后扰动其他功能。公共跨语言用例应优先使用SEQUENCE。

```json
"random_control": {
  "mode": "SEQUENCE",
  "streams": {
    "DICE": [6, 2],
    "FORTUNE_POSITION": [28, 20],
    "FORTUNE_RESPAWN_DELAY": [7],
    "GIFT": [2]
  }
}
```

| 流 | 取值 | 消费规则 |
| --- | --- | --- |
| DICE | 1..6 | 每次ROLL消费一个 |
| FORTUNE_POSITION | 0..69 | 每次候选消费一个；不合格则继续取下一个 |
| FORTUNE_RESPAWN_DELAY | 1..10 | 财神领取或自然失效时消费一个 |
| GIFT | 项目定义的礼品编号 | 礼品屋随机选择时消费一个 |

SEQUENCE流在需要取值时耗尽，返回RANDOM_SEQUENCE_EMPTY；值越界返回RANDOM_VALUE_OUT_OF_RANGE。

### 7.1 PRNG模式

```json
"random_control": {
  "mode": "PRNG",
  "algorithm": "XORSHIFT32",
  "stream_seeds": {
    "DICE": 12345,
    "FORTUNE_POSITION": 45678,
    "FORTUNE_RESPAWN_DELAY": 90123,
    "GIFT": 23456
  }
}
```

XORSHIFT32每个流独立维护uint32状态：x ^= (x << 13); x ^= (x >> 17); x ^= (x << 5)，每步均截断到32位。seed必须为1..4294967295。范围映射为min + next_uint32 % (max-min+1)。

## 8. STATE模式Preset

```json
"preset": {
  "users": ["A", "Q"],
  "current_user": "A",
  "phase": "COMMAND",
  "game_status": "RUNNING",
  "turn_number": 11,
  "players": [
    {"id":"A","fund":1000,"credit":0,"position":4,
     "status":"NORMAL","items":{"BLOCK":0,"ROBOT":0},
     "god_of_wealth_rounds":0}
  ],
  "properties": [],
  "map_items": [],
  "fortune": {
    "position": 5,
    "spawned_after_turn": 10,
    "remaining_map_turns": 5,
    "next_spawn_after_turn": null
  },
  "random_control": {"mode":"SEQUENCE","streams":{}}
}
```

| 字段 | 规则 | 版本标记 |
| --- | --- | --- |
| turn_number | 当前玩家回合编号，从1开始；完成第N回合后进入N+1 | 新增 |
| fortune | 地图财神完整生命周期；无财神时position=null | 新增 |
| random_control | 确定性随机源 | 新增 |
| players/properties/map_items | 继承1.0；不得含2.0删除字段 | 沿用/变更 |

## 9. Action结构

```json
{"command": "STEP", "params": {"steps": 5}}
```

| command | params | 作用 | 结束回合 | 版本标记 |
| --- | --- | --- | --- | --- |
| ROLL | {} | 从DICE流取值并移动 | 是 | 沿用 |
| STEP | {steps:1..2147483647} | steps>70时先对70取余，再按有效步数移动 | 是 | 变更 |
| SELL | {position:0..69} | 出售当前玩家地产 | 否 | 沿用 |
| BLOCK | {offset:-10..10} | 放置路障；0表示当前格 | 否 | 沿用并明确 |
| ROBOT | {} | 清除前方10格BLOCK | 否 | 变更 |
| QUERY | {} | 查询资产 | 否 | 沿用 |
| HELP | {} | 查看帮助 | 否 | 沿用 |
| ANSWER | {value:any} | 回答当前Prompt | 视Prompt | 沿用 |
| QUIT | {} | 强制结束整局游戏；不等同于当前玩家破产或退出后让其他玩家继续 | 是 | 沿用/大小写变更并明确 |
| ADVANCE_TURN | {} | STATE测试专用原地推进一回合 | 是 | 新增 |

### 9.1 Quit强制结束语义

> 【2.0明确】 以客户需求PDF“执行该命令，强制结束游戏”为准。`Quit` 在COMMAND或任一合法PROMPT阶段执行时，都立即将 `game_status` 设为 `FINISHED`、`phase` 设为 `ENDED`、`pending_prompt` 清空且 `winner` 为 `null`。它不触发当前玩家破产、资产清算、胜者判定或“剩余玩家继续”流程；这些流程只由资金低于0的破产规则触发。命令输入继续按ASCII不区分大小写。

## 10. PROCESS与INTERACTIVE模式

> 【2.0新增】 launch描述真实进程环境；此模式用于验证启动、玩家选择、资金输入、帮助、地图文字和退出行为。

```json
"launch": {
  "args": ["--cli"],
  "working_directory": ".",
  "environment": {},
  "encoding": "UTF-8",
  "timeout_ms": 5000,
  "terminal": {"columns": 100, "rows": 30, "ansi": false},
  "instances": 2,
  "instance_inputs": [["10000", "12", "quit"], ["20000", "34", "quit"]]
}
```

> 【2.0新增】 `instances` 取值1～16，缺省为1。并发多进程用例必须提供与实例数等长的 `instance_inputs`；每个元素是一组仅发送给对应进程的完整输入。单实例或未提供该字段时，继续从 `actions` 中的 `INPUT` 动作生成通用输入。

| 测试控制command | params | 说明 |
| --- | --- | --- |
| INPUT | {text:string, newline:bool=true} | 向stdin/伪终端写入文本 |
| EXPECT_OUTPUT | {stream, match, value, timeout_ms?} | 在当前检查点等待并断言输出 |
| RESIZE_TERMINAL | {columns, rows} | 改变伪终端尺寸 |
| WAIT | {timeout_ms:0..5000} | 仅用于等待异步输出，不得替代EXPECT_OUTPUT |
| TERMINATE | {signal:"TERM"} | 测试清理；不等同于游戏QUIT |

INTERACTIVE动作与游戏输入分离：游戏命令通过INPUT发送，不得把INPUT当成游戏内部Action执行。PROCESS默认在进程结束后比较输出；INTERACTIVE可在多个EXPECT_OUTPUT检查点比较。

### 10.1 开局输入协议

> 【2.0明确】 开局流程以客户需求 PDF 为准，顺序固定为“初始资金→角色组合”。不得增加独立的玩家人数步骤，也不得把角色组合拆成逐玩家输入。

1. 程序启动后先读取初始资金。允许1000～50000的整数；直接回车使用默认值10000。
2. 资金确定后，一次性输入由 `1`、`2`、`3`、`4` 组成的2～4位角色编号组合，例如 `12`、`123`、`1234`。
3. 组合内编号不得重复；组合长度就是玩家人数，组合顺序就是玩家顺序。映射固定为：`1=钱夫人(Q)`、`2=阿土伯(A)`、`3=孙小美(S)`、`4=金贝贝(J)`。
4. 合法角色组合输入后直接创建游戏并进入组合首位玩家回合，不再等待额外的“人数确认”“逐角色确认”或“最终确认”。

INTERACTIVE用例的开局 `INPUT` 必须按上述顺序排列。例如2人、资金10000的完整开局输入为 `10000`、`12`，而不是 `2`、`10000`、`1`、`2`。

多实例的每组 `launch.instance_inputs` 也必须分别满足本节协议，不得只让通用 `actions` 合法而向其他实例发送旧流程输入。

## 11. Expected与虚拟断言

> 【沿用1.0】 Expected对象递归部分匹配：只比较Expected写出的键；标量完全相等；players按id，properties/map_items/display_players/display_cells按position匹配。

### 11.1 pending_prompt类型与phase一致性

> 【2.0新增】 `pending_prompt` 只允许为 `null` 或枚举字符串 `BUY`、`UPGRADE`、`TOOL_SHOP`、`GIFT_SHOP`。位置由玩家状态或其他明确字段断言，不得把 `pending_prompt` 写成对象。

- `phase = COMMAND` 时，`pending_prompt` 必须为 `null`。
- `phase = PROMPT` 时，`pending_prompt` 必须为上述非空枚举，并且 `current_user` 仍为触发提示的玩家。
- `phase = ENDED` 时，`pending_prompt` 必须为 `null`。

| 虚拟断言 | 用途 | 版本标记 |
| --- | --- | --- |
| properties_absent | 指定位置不得存在地产 | 沿用 |
| map_items_absent | 指定位置不得存在地图道具 | 沿用 |
| fields_absent | 当前对象不得包含指定字段 | 新增 |
| fortune_assert | 财神存在、范围、禁用格、占用和地图道具冲突 | 新增 |
| output_assert | stdout/stderr匹配、归一化和布局断言 | 新增 |

```json
"fortune_assert": {
  "present": true,
  "position_between": [0, 69],
  "position_not_in": [28, 35],
  "unoccupied": true,
  "without_map_item": true
}
```

## 12. Actual状态输出

```json
{
  "schema_version": "2.0",
  "case_id": "TC-FORTUNE-001",
  "actual": {
    "users": ["A", "Q"],
    "current_user": "Q",
    "phase": "COMMAND",
    "pending_prompt": null,
    "game_status": "RUNNING",
    "winner": null,
    "turn_number": 11,
    "players": [],
    "properties": [],
    "map_items": [],
    "fortune": {
      "position": 20,
      "symbol": "F",
      "spawned_after_turn": 10,
      "remaining_map_turns": 5,
      "next_spawn_after_turn": null
    },
    "display_players": [],
    "display_cells": []
  }
}
```

STATE模式的stdout必须只输出一个JSON对象；诊断日志写stderr。Actual应完整导出，Expected仍按部分匹配。

## 13. 地图显示状态

> 【2.0新增】 display_cells用于测试公园、玩家重叠、道具和财神覆盖后的可见符号，不要求比较整张终端字符画。

```json
"display_cells": [
  {"position": 14, "base_type": "PARK", "base_symbol": "P",
   "visible_symbol": "A", "visible_entity": "PLAYER"},
  {"position": 20, "base_type": "LAND_1", "base_symbol": "0",
   "visible_symbol": "F", "visible_entity": "FORTUNE"}
]
```

| visible_entity | 说明 |
| --- | --- |
| BASE | 显示底层地图符号 |
| PLAYER | 玩家符号覆盖 |
| MAP_ITEM | 路障#覆盖 |
| FORTUNE | 财神F覆盖 |

玩家或财神离开后，visible_symbol必须恢复base_symbol；逻辑位置始终以players/map_items/fortune为准。

## 14. 财神规则

### 14.1 首次生成

> 【2.0新增】 完成第10个玩家回合后、切换至第11回合前首次生成一个地图财神。这里的回合指一名玩家完成一次行动，不是一整轮所有玩家。

候选格必须为0..69、无人、无map_item，且base_type不是TOOL_SHOP或GIFT_SHOP。PARK、START、MINE和普通地产均可成为候选。生成后显示F。

### 14.2 候选回退

从FORTUNE_POSITION流逐个取候选。候选不合格时不得生成，继续取下一个，直到合格；流耗尽返回RANDOM_SEQUENCE_EMPTY。同一时刻最多一个地图财神。

### 14.3 保留与自然失效

生成时remaining_map_turns=5。之后每完成一个玩家回合且财神未被领取，计数减1；减到0后立即移除F并恢复底层符号。

### 14.4 领取与再次生成

移动逐格检查财神。第一个进入财神位置的玩家立即领取，F立即消失，后续玩家不能重复领取。领取或自然失效时，从FORTUNE_RESPAWN_DELAY读取1..10；新财神在该事件发生后的第delay个已完成玩家回合结束时生成。因此“10回合内”被精确定义为1～10回合。

> 【2.0变更】 本条取代旧扩展中“失效满10回合后固定生成”的解释，解决Case_A14_012边界冲突。

### 14.5 玩家财神效果与当轮免租

领取时god_of_wealth_rounds立即设为5，当前移动继续执行。若同一移动最终落在他人地产，必须先应用财神再结算租金，本次租金为0，双方资金不变。领取所在回合不递减；以后该玩家每完成一个自己的回合减1。

> 礼品屋财神 2.0将地图财神与礼品屋礼品视为两个独立来源。若map.json或统一礼品配置仍包含财神礼品，地图F生成不得删除或改变礼品屋配置；若客户决定取消礼品屋财神，应通过独立需求和配置版本删除。

## 15. 公园与删除功能

### 15.1 PARK

到达或经过PARK不改变fund、credit、status、items或god_of_wealth_rounds，不产生pending_prompt，不扣留玩家，并按正常规则结束/继续移动。玩家离开后显示恢复P。

### 15.2 删除清单

| 删除项 | 2.0处理 | 错误码 |
| --- | --- | --- |
| BOMB命令 | 不支持 | INVALID_COMMAND |
| items.BOMB / map_items.type=BOMB | Preset非法 | INVALID_PRESET |
| HOSPITAL / JAIL状态 | Preset非法 | INVALID_PRESET |
| remaining_rounds | 字段非法 | INVALID_PRESET |
| MAGIC_HOUSE地图类型 | map.json非法 | INVALID_MAP |

## 16. 输出断言

### 16.1 进程最终结果

```json
"expected": {
  "process": {
    "exit_code": 0,
    "stdout": {
      "normalize": {"line_endings":"LF", "strip_ansi":true,
                    "trim_trailing_spaces":false},
      "matchers": [
        {"type":"CONTAINS", "value":"请选择角色"},
        {"type":"NOT_CONTAINS", "value":"监狱"}
      ]
    },
    "instances": [
      {"stdout": {"matchers": [{"type":"CONTAINS", "value":"初始资金为 10000"}]}},
      {"stdout": {"matchers": [{"type":"CONTAINS", "value":"初始资金为 20000"}]}}
    ],
    "stderr": {"matchers": [{"type":"EXACT", "value":""}]}
  }
}
```

> 【2.0新增】 多实例用例的 `expected.process.instances` 必须与 `launch.instances` 等长。第 `i` 项只断言第 `i` 个进程的输出，并覆盖通用 `stdout`/`stderr` 断言中对应的流；未写的流仍使用通用断言。这样可核对不同资金、角色组合和命令不会串到其他游戏进程。

### 16.2 匹配器

| type | 含义 |
| --- | --- |
| EXACT | 归一化后全文相等 |
| CONTAINS | 包含子串 |
| NOT_CONTAINS | 不包含子串 |
| REGEX | ECMAScript正则；测试文件中反斜杠按JSON转义 |
| LINE_EQUALS | 至少一行逐字相等 |

### 16.3 归一化

line_endings缺省LF；strip_ansi缺省true；trim_trailing_spaces缺省false。测试终端布局时必须显式写strip_ansi和trim_trailing_spaces，且可使用terminal.columns固定宽度。不得依赖平台默认终端宽度。

## 17. 测试报告

> 【2.0变更】 活动测试集只包含正向用例。规范不定义expected_outcome、expected_error、expected_errors、expected_error_code或expected_result字段；运行器不得把“程序按预期报错”判定为PASS。

| 报告result | 含义 |
| --- | --- |
| PASS | 程序执行完成且Expected匹配 |
| FAIL | 程序执行完成但Expected不匹配 |
| ERROR | 测试资产/运行环境自身非法，无法完成比较 |

## 18. 错误码

| 错误码 | 含义 | 版本标记 |
| --- | --- | --- |
| INVALID_JSON | JSON无法解析 | 沿用 |
| UNSUPPORTED_VERSION | schema_version不支持 | 沿用 |
| UNSUPPORTED_MODE | mode不支持 | 新增 |
| INVALID_PRESET | Preset缺失、越界或状态冲突 | 沿用 |
| INVALID_MAP | 地图错误或含删除类型 | 沿用/变更 |
| INVALID_COMMAND | 命令不支持 | 沿用 |
| INVALID_PARAMS | 参数缺失、类型错误或越界 | 沿用 |
| INVALID_PHASE | 当前阶段不允许Action | 沿用 |
| RANDOM_SEQUENCE_EMPTY | 确定性随机流耗尽 | 新增 |
| RANDOM_VALUE_OUT_OF_RANGE | 随机流值越界 | 新增 |
| PROCESS_TIMEOUT | 进程/输出等待超时 | 新增 |
| OUTPUT_ASSERT_FAILED | 输出检查点不匹配 | 新增 |
| ACTION_AFTER_END | 结束后仍有Action | 沿用 |
| ASSERT_NOT_EQUAL | Actual与Expected不同 | 沿用 |
| ASSERT_NOT_FOUND | 要求对象不存在 | 沿用 |
| ASSERT_NOT_ABSENT | 应不存在的对象实际存在 | 沿用 |

## 19. 执行和比较流程

1. 读取UTF-8 JSON，确定是单用例还是测试集。

2. 按schema_version=2.0校验外层结构、mode和必填字段。

3. STATE模式校验map.json和Preset；进程模式校验launch。

4. 完整重置测试运行环境和所有随机流。

5. 按顺序执行Actions；不得绕过正式游戏逻辑。

6. STATE导出完整Actual；进程模式收集输出、退出码和检查点。

7. 比较Expected；活动测试集不支持预期错误判定。

8. 生成PASS/FAIL/ERROR JSON和可选JUnit报告。

## 20. 一键自动化与命令行契约

项目必须提供一键编译脚本和一键测试脚本。脚本返回码：全部PASS为0，存在FAIL或ERROR为非0。

```text
build.bat
run_tests.bat

run_tests --program <被测程序> --cases <文件或目录> \
          --map <map.json> --out <results.json> --junit <junit.xml>
```

STATE适配器调用约定继续为：<program> <single_case.json> <map.json>，stdout输出一个Actual JSON。PROCESS/INTERACTIVE由运行器按launch直接启动被测程序。

## 21. 完整示例

### 21.1 财神候选回退

```json
{
  "schema_version":"2.0",
  "case_id":"TC-FORTUNE-FALLBACK-001",
  "case_name":"候选为道具屋时改用下一合格位置",
  "mode":"STATE",
  "map_file":"map.json",
  "preset":{
    "users":["A","Q"],"current_user":"A","phase":"COMMAND",
    "game_status":"RUNNING","turn_number":10,
    "players":[
      {"id":"A","fund":1000,"credit":0,"position":14,"status":"NORMAL",
       "items":{"BLOCK":0,"ROBOT":0},"god_of_wealth_rounds":0},
      {"id":"Q","fund":1000,"credit":0,"position":49,"status":"NORMAL",
       "items":{"BLOCK":0,"ROBOT":0},"god_of_wealth_rounds":0}
    ],
    "properties":[],"map_items":[],
    "fortune":{"position":null,"spawned_after_turn":null,
               "remaining_map_turns":0,"next_spawn_after_turn":10},
    "random_control":{"mode":"SEQUENCE",
      "streams":{"FORTUNE_POSITION":[28,20]}}
  },
  "actions":[{"command":"ADVANCE_TURN","params":{}}],
  "expected":{
    "turn_number":11,
    "fortune":{"position":20,"symbol":"F","remaining_map_turns":5}
  }
}
```

### 21.2 经过财神后当轮免租

```json
{
  "schema_version":"2.0","case_id":"TC-FORTUNE-RENT-001",
  "case_name":"经过F后落在他人地产立即免租","mode":"STATE",
  "map_file":"map.json",
  "preset":{
    "users":["Q","A"],"current_user":"Q","phase":"COMMAND",
    "game_status":"RUNNING","turn_number":11,
    "players":[
      {"id":"Q","fund":1000,"credit":0,"position":4,"status":"NORMAL",
       "items":{"BLOCK":0,"ROBOT":0},"god_of_wealth_rounds":0},
      {"id":"A","fund":1000,"credit":0,"position":20,"status":"NORMAL",
       "items":{"BLOCK":0,"ROBOT":0},"god_of_wealth_rounds":0}
    ],
    "properties":[{"position":6,"owner":"A","level":0}],
    "map_items":[],
    "fortune":{"position":5,"spawned_after_turn":10,
               "remaining_map_turns":5,"next_spawn_after_turn":null}
  },
  "actions":[{"command":"STEP","params":{"steps":2}}],
  "expected":{
    "players":[{"id":"Q","position":6,"fund":1000,"god_of_wealth_rounds":5},
               {"id":"A","fund":1000}],
    "fortune":{"position":null}
  }
}
```

### 21.3 交互帮助与删除命令

```json
{
  "schema_version":"2.0","case_id":"TC-HELP-001",
  "case_name":"帮助中不再显示炸弹命令","mode":"INTERACTIVE",
  "launch":{"args":[],"encoding":"UTF-8","timeout_ms":5000,
            "terminal":{"columns":100,"rows":30,"ansi":false}},
  "preset":{},
  "actions":[
    {"command":"INPUT","params":{"text":"Help","newline":true}},
    {"command":"EXPECT_OUTPUT","params":{"stream":"stdout","match":"CONTAINS",
                                            "value":"BLOCK"}},
    {"command":"EXPECT_OUTPUT","params":{"stream":"stdout","match":"NOT_CONTAINS",
                                            "value":"BOMB"}},
    {"command":"INPUT","params":{"text":"Quit","newline":true}}
  ],
  "expected":{"process":{"exit_code":0}}
}
```

## 附录A：1.0到2.0迁移清单

| 1.0内容 | 2.0处理 | 迁移动作 |
| --- | --- | --- |
| schema_version=1.0 | 改为2.0 | 按2.0重新校验，不做字符串替换式升级 |
| 单用例文件 | 继续支持；增加测试集包装 | 可合并到tests数组 |
| dice_sequence | 由random_control.streams.DICE取代 | 迁移数组并声明SEQUENCE |
| BOMB | 删除 | 删除相关案例和字段 |
| HOSPITAL/JAIL/remaining_rounds | 删除 | 删除Preset字段与轮空断言 |
| MAGIC_HOUSE | 改为PARK | map.json位置14/49/63写PARK |
| STEP 0推进时间 | 非法 | 改用ADVANCE_TURN |
| STEP最大69步 | 取消上限 | 允许正int32；steps>70时先对70取余 |
| 大小写严格的command | 输入不区分ASCII大小写 | 输出枚举仍保持大写 |
| 终端文字默认不比较 | PROCESS/INTERACTIVE可显式比较 | 增加launch和output断言 |
| 随机行为不可复现 | 具名随机流 | 优先使用SEQUENCE |

## 附录B：规范性约束摘要

- 不得为使测试通过而修改Expected或在游戏逻辑中按case_id特判。

- 无法由2.0字段无歧义表达的自然语言案例，应先补充案例数据或修订规范，不得猜测。

- 地图、随机流、Action和Preset在执行前必须校验；非法测试资产不得进入游戏逻辑。

- 2.0新增字段由测试运行器和开发侧适配器共同实现；尚未实现时测试应保持红灯，而不是删除断言。

- 同一份用例在不同语言实现上必须得到相同业务状态和相同规范化比较结果。

—— 规范结束 ——
