# A7 Query 与 A18 Step 接口说明

## Query

独立数据视图定义在 `A7_A18/monopoly/query.h`：

```c
int query_format_player(const QueryPlayerState *state,
                        char *message,
                        size_t message_size);
```

格式化模块不读取地图和回合管理器。集成层通过 `runtime_query` 收集当前玩家数据，包括：

- 资金、点数和当前位置；
- 所有房产的绝对位置、地价和建筑等级；
- 路障、机器娃娃和炸弹数量；
- 财神、住院和监狱剩余回合；
- 破产状态。

后续业务模块可使用 `runtime.h` 中的资产更新接口：

```c
runtime_set_player_money(...);
runtime_add_player_points(...);
runtime_set_player_item_count(...);
runtime_set_player_fortune_turns(...);
runtime_set_player_hospital_turns(...);
runtime_set_player_prison_turns(...);
runtime_set_player_bankrupt(...);
runtime_assign_property(...);
runtime_release_property(...);
```

## Step n

参数解析定义在 `A7_A18/monopoly/step.h`：

```c
StepParseResult step_parse_argument(const char *arguments, int *steps);
```

解析器不依赖地图、玩家或骰子。集成入口为：

```c
int runtime_step(GameRuntime *runtime,
                 int steps,
                 char *message,
                 size_t message_size);
```

`runtime_step` 把正整数步数传给回合管理器的 `forced_steps` 参数。移动回调检测到 `forced_steps > 0` 时直接采用该值，不会调用 `dice_roll`。步数不限制为1～6，但必须是 `int` 范围内的正整数。

Step完成移动和落地处理后，与普通Roll一样自动切换到下一位玩家。
