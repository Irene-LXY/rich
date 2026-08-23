# 测试人员需要和开发人员沟通的内容

下面这些内容，建议你在开发开始前，和开发人员逐条确认并记录下来。

## 1. 最重要的一个接口

自动化测试环境不依赖开发人员的内部代码，只依赖一个函数：

```c
cJSON *game_engine_execute(
    const cJSON *preset,
    const cJSON *actions,
    char **error_code,
    char **error_message);
```

请开发人员：

- 新建 `src/game_engine.c`，实现这个函数；
- 包含 `src/game_engine.h`；
- 编译时把 `game_engine_stub.c` 换成 `game_engine.c`。

函数输入是测试用例里的 `preset` 和 `actions`，成功返回 `actual`，失败返回 `NULL` 并设置错误码。

## 2. 统一 JSON 字段和版本

请开发人员确认以下内容以接口规范 v1.1 为准：

- `schema_version` 当前为 `"1.0"`；
- 资产字段叫 `fund` 和 `credit`，不要叫 `money`、`points`；
- 命令统一大写；
- 输出 UTF-8 无 BOM；
- `players` 按 `id` 排序输出；
- `properties`、`map_items`、`display_players` 按 `position` 升序输出；
- 不输出语言特有字段。

## 3. map.json 必须先冻结

测试用例里的 `map_file` 都指向 `map.json`。

请开发人员提供并固定：

- 70 个位置（0..69）的类型；
- 各地段价格；
- 起点、道具屋、礼品屋、魔法屋、医院、监狱、矿地的位置；
- 每个矿地的点数。

`map.json` 一变，测试用例的预期值就可能要改。

## 4. 回合和状态语义必须一致

请开发人员确认并实现：

- `ROLL` 从 `dice_sequence` 顺序取值；
- `STEP` 与 `ROLL` 使用同一套逐格移动逻辑；
- 移动过程中检查路障和炸弹；
- `phase` 只有 `COMMAND`、`PROMPT`、`ENDED`；
- `HOSPITAL` 初始 `remaining_rounds=3`，`JAIL` 初始 `remaining_rounds=2`；
- 破产玩家不再获得回合；
- 只剩一名未破产玩家时 `game_status` 变为 `FINISHED`。

## 5. 六个 Story 与测试方式

| Story | 当前测试方式 | 需要和开发人员确认 |
| --- | --- | --- |
| 1 启动游戏 | JSON 状态初始化 | 是否新增 `START` 命令 |
| 2 选择玩家 | JSON 校验 `users/players` | 是否新增 `SELECT_PLAYER` 命令 |
| 3 依次进行回合 | JSON `STEP/ROLL` + `current_user` | 回合切换和轮空规则 |
| 4 显示地图 | JSON `display_players` | 是否新增地图显示命令或字段 |
| 5 掷骰子移动 | JSON `ROLL` + `dice_sequence` | 途中道具和落点规则 |
| 6 Quit 退出 | JSON `QUIT` + `phase/game_status` | QUIT 后状态是 `ENDED/FINISHED` |

## 6. Story 1、2、4 的缺口要提前说清楚

接口规范 v1.1 主要是“状态机”测试，没有完整覆盖：

- 启动游戏的交互流程；
- 玩家轮流选择角色的交互流程；
- 完整终端地图字符画。

本项目要求只使用 JSON 格式进行自动化测试，因此建议：

扩展 JSON 接口，增加 `START`、`SELECT_PLAYER`、`DISPLAY` 等命令。

在开发开始前必须定下来，否则 Story 1、2、4 的自动化测试范围不明确。

## 7. QUIT 和错误码要确认

测试用例 `story06_quit.json` 假设：

- `QUIT` 后 `phase` 为 `ENDED`；
- `game_status` 为 `FINISHED`。

请开发人员确认这个假设是否正确。

同时确认错误码：

- `INVALID_PRESET`
- `INVALID_COMMAND`
- `INVALID_PARAMS`
- `INVALID_PHASE`
- `DICE_SEQUENCE_EMPTY`
- `ACTION_AFTER_END`

## 8. TDD 协作规则

请开发人员遵守：

- 测试用例是契约，不能为了让测试通过而修改 `expected`；
- 现在测试运行结果为 `ERROR` 是正常的红阶段；
- 实现游戏逻辑后，测试逐步变绿；
- 每次提交后让 CI 自动跑测试，而不是只在本地跑。

## 9. 交付清单

开发人员需要交付：

1. `src/game_engine.c`；
2. `spec/map.json`；
3. `rich.exe` 的可编译工程；
4. 对 Story 1、2、4 所需新增 JSON 命令的确认结果；
5. 对 QUIT 结束状态的确认结果。
