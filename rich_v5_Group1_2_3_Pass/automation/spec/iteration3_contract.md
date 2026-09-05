# 迭代三接口兼容说明（已由 v2.0 取代）

本文件不再单独定义接口字段。迭代三测试唯一权威规范为
[`game_test_spec_v2.md`](game_test_spec_v2.md)，地图唯一权威数据为
[`map.json`](map.json)。请勿继续使用本文件历史版本中的 `STEP 0`、
`spawned_turn`、`next_spawn_turn` 或 `next_spawn_in_turns`。

为兼容旧链接，关键迁移规则摘录如下；若与 v2.0 正文冲突，以 v2.0 正文为准。

- 原地推进一个玩家回合使用测试专用 `ADVANCE_TURN {}`；`STEP 0` 非法。
- `STEP.steps` 接受正 int32。步数大于 70 时先计算 `steps % 70`；余数为 0
  时有效移动距离就是0，但该Action仍然合法并按STEP规则完成回合，不能把它当成非法输入
  `STEP 0`。
- 财神 Preset 使用 `spawned_after_turn`、`remaining_map_turns`、
  `next_spawn_after_turn`；Actual 使用同一组字段。
- 财神随机位置由 `random_control` 的具名随机流控制；不得退回旧
  `dice_sequence` 或依赖不可复现的系统随机数。
- 地图 14、49、63 均为 `PARK`；医院、监狱、魔法屋、炸弹和轮空字段均已删除。
- `pending_prompt` 为 `null` 或 `BUY`、`UPGRADE`、`TOOL_SHOP`、
  `GIFT_SHOP` 枚举字符串，不是对象。
