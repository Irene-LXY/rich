# A4：2 至 4 名玩家顺序回合模块

本目录只实现 User Story A4 的回合调度，不重复实现地图、移动、地产、道具或落地事件。模块采用 C11，能独立测试，并通过回调接入团队其他模块。

## 已实现的 A4 行为

- 玩家严格按初始化顺序行动；第一回合固定为玩家 1，末位玩家结束后循环回到玩家 1。
- 快照始终给出当前玩家编号、角色名、回合/轮次、阶段和可执行命令位图。
- 所有会改变回合状态的 API 都校验 `actor_id`；非当前玩家会得到 `A4_TURN_ERR_WRONG_PLAYER`。
- 掷骰前允许卖房、放置道具等前置操作；掷骰后这些操作会被拒绝。
- 每回合只允许一次正常 `Roll` 或测试 `Step n`。移动回调失败时会回滚，不消耗本次掷骰机会。
- 落地事件未完成时停留在 `A4_TURN_PHASE_RESOLVING_LANDING`，禁止重复掷骰和强行结束回合。
- 移动和落地事件完成后自动切换下一玩家。
- 支持住院、入狱和自定义跳过状态，显示原因和剩余次数，并自动切换下一玩家。
- 支持玩家破产退出；仅剩一名参与者时自动结束游戏。

## 状态流转

```text
未开始
   |
   v
掷骰前 --前置操作--> 掷骰前
   |
 Roll / Step n
   v
移动中 --失败--> 掷骰前（不消耗掷骰机会）
   |
   +--落地已完成----------------------+
   |                                  |
   +--需要购买/交租/特殊事件--> 处理中 --complete_landing--+
                                                      |
                                                      v
                                                下一玩家掷骰前

轮到住院/入狱玩家：跳过提示 -> 扣减剩余次数 -> 下一玩家
```

## 预留接口与模块归属

| A4 接口 | 对接模块 | 约定 |
|---|---|---|
| `A4TurnHooks.roll_and_move` | A8 掷骰与移动 | `forced_steps == 0` 为普通 Roll；大于 0 为测试 Step。返回是否还有落地事件待处理。 |
| `a4_turn_manager_complete_landing` | 地产/交租/商店/特殊格事件 | 只有事件全部处理成功后才能调用；调用后 A4 自动换人。 |
| `A4TurnHooks.run_pre_roll_action` | 卖房、路障、炸弹、机器娃娃 | 只会在掷骰前被 A4 放行；业务失败返回 `false`。 |
| `A4TurnHooks.on_state_changed` | A5 地图与终端 UI | 必须使用同一快照同步刷新当前玩家提示、地图高亮、命令列表。 |
| `a4_turn_manager_set_skip` | 医院、监狱及其他事件 | 住院按项目书可传 3，入狱可传 2；每轮到一次自动减 1。 |
| `a4_turn_manager_mark_player_out` | 破产/胜负模块 | 将玩家移出后续回合；仅剩一人时触发结束回调。 |
| `a4_turn_manager_finish` | Quit/主控制器 | 强制结束整局；胜者未知时传 0。 |

### A8 移动回调的返回值

- `A4_MOVE_RESOLVED`：移动和落地事件都已完成，A4 立即换人。
- `A4_MOVE_LANDING_PENDING`：需要进一步输入（购买、交租、礼品选择等）；处理完成后调用 `a4_turn_manager_complete_landing`。
- `A4_MOVE_GAME_OVER`：移动直接导致游戏结束。
- `A4_MOVE_FAILED`：移动失败；A4 回到掷骰前，允许重试。

`actual_steps` 是实际移动步数，可以小于骰子点数（例如被路障提前截停），但不能为负数。

### UI 同步约束

`on_state_changed` 是唯一的回合 UI 同步边界。不要在 UI 层自行保存“当前玩家”副本；每次回调都从 `A4TurnSnapshot` 读取：

- `current_player_id` / `current_role_name`：提示与地图高亮；
- `phase`：当前回合阶段；
- `available_operations`：允许显示和接收的命令；
- `turn_number` / `round_number`：全局回合序号与循环轮次。

所有回调均为同步回调，不应在回调内部重入调用同一个 `A4TurnManager`。如果 UI 或事件处理是异步的，应保存自己的业务上下文，并在异步工作完成后由主循环调用 `a4_turn_manager_complete_landing`。

## 文件说明

- `include/a4_turn_manager.h`：公开类型、状态码、生命周期 API 和全部预留接口。
- `src/a4_turn_manager.c`：A4 状态机实现。
- `tests/test_a4_turn_manager.c`：覆盖玩家顺序、越权操作、重复掷骰、事件阻塞、跳过回合、循环和破产结束。
- `examples/demo.c`：A5/A8 对接的最小示例。

## 构建与测试

### Windows 一键构建（推荐）

项目附带的脚本会自动定位 Visual Studio 自带的 CMake、Ninja 和 MSVC，不要求将 `cmake` 加入系统 `PATH`：

```powershell
.\build.cmd
```

构建并同时运行示例：

```powershell
.\build.cmd -RunDemo
```

发布配置：

```powershell
.\build.cmd -Configuration Release
```

脚本要求 Visual Studio 2022 已安装“使用 C++ 的桌面开发”组件。

### 已配置 CMake PATH 的环境

如果终端已经能识别 `cmake`，也可以使用标准命令：

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

运行示例：

```powershell
.\build\a4_demo.exe
```

在 Visual Studio 多配置生成器下，可执行文件通常位于 `build\Debug\a4_demo.exe`。

## 主程序接入顺序

1. A2 完成 2 至 4 名玩家选择后，按玩家顺序构造 `A4PlayerConfig`。
2. 填写 `A4TurnHooks`，至少接入 A8 的 `roll_and_move`；终端项目建议同时接入 `on_state_changed` 和 `on_notice`。
3. 调用 `a4_turn_manager_init`，确认成功后再创建/提交完整游戏状态。
4. 调用 `a4_turn_manager_begin`，玩家 1 进入第一个回合。
5. 命令层根据快照的 `available_operations` 路由命令，不要绕过 A4 直接换人。

角色名称在初始化时被复制到模块内部；`A4TurnSnapshot` 中的字符串指针在对应 `A4TurnManager` 存活期间有效。`A4TurnManager` 字段公开只是为了允许静态分配，业务模块不得直接修改字段。
