# A7 Query / A18 Step 团队接口文档

本目录存放项目实际编译使用的 Query 和 Step 根本功能代码，不是代码副本。本文档说明其他团队成员应如何调用接口。

## 一、文件与职责

```text
A7_A18/
├─ monopoly/
│  ├─ query.h          Query数据结构和格式化接口
│  └─ step.h           Step参数解析接口
├─ query_format.c      Query文本生成实现
├─ query_command.c     Query命令入口
└─ step_command.c      Step命令解析及运行时调用
```

公共集成代码仍位于原项目结构中：

```text
include/monopoly/command.h          Query和Step命令入口声明
include/monopoly/runtime.h          资产更新、Query和Step运行时接口
src/commands/command_dispatcher.c   统一命令分发
src/runtime/runtime.c               玩家、回合、地图和本模块的连接层
```

## 二、引用头文件

CMake已经把 `A7_A18` 加入公共头文件搜索路径。其他模块使用：

```c
#include "monopoly/query.h"
#include "monopoly/step.h"
#include "monopoly/runtime.h"
```

不要使用相对路径引用，例如不要写 `../../A7_A18/monopoly/query.h`。

## 三、Query接口

### 3.1 在游戏中执行Query

命令层通常直接调用统一分发器：

```c
char message[8192];
CommandResult result = command_execute(
    &game, "Query", message, sizeof(message));
```

也可以由集成层直接调用：

```c
char message[8192];
int result = runtime_query(runtime, message, sizeof(message));
```

返回值：

- `0`：成功，`message` 中是完整资产信息；
- 非 `0`：运行时无效、当前玩家无效或输出缓冲区不足。

Query可能列出最多58处房产，建议输出缓冲区至少为8192字节。

### 3.2 独立格式化Query数据

不使用游戏运行时的模块，可以自行填充 `QueryPlayerState`：

```c
QueryPlayerState state = {0};
char message[8192];

state.player_id = 1;
state.player_name = "钱夫人";
state.symbol = 'Q';
state.money = 10000;
state.points = 200;
state.position = 18;
state.fortune_turns = 4;

if (query_format_player(&state, message, sizeof(message)) == 0) {
    printf("%s", message);
}
```

核心接口：

```c
int query_format_player(const QueryPlayerState *state,
                        char *message,
                        size_t message_size);
```

此接口不依赖地图、回合管理器或随机骰子。调用者负责保证 `state` 在调用期间有效；函数只读取数据，不保存其中的指针。

### 3.3 Query显示字段

Query当前显示：

- 玩家名称、字符和编号；
- 资金、点数和当前位置；
- 每处房产的位置、建筑等级、建筑名称和土地价格；
- 房产总数；
- 路障、机器娃娃和炸弹数量，以及道具总数；
- 财神、住院和监狱剩余回合；
- 是否破产。

房屋等级约定：

```text
0：空地
1：茅屋
2：洋房
3：摩天楼
```

## 四、供其他业务模块更新资产的接口

这些接口声明在 `include/monopoly/runtime.h`。成功返回 `0`，失败返回非 `0`。

### 4.1 资金

```c
runtime_set_player_money(runtime, player_id, money);
```

直接设置玩家资金。允许设置为负数，破产模块可以据此判断破产。

### 4.2 点数和矿地

```c
runtime_add_player_points(runtime, player_id, points);
```

给玩家增加非负点数。矿地模块应调用该接口，不要把矿地点数加入资金。

### 4.3 道具

```c
runtime_set_player_item_count(runtime, player_id, QUERY_ITEM_BLOCK, count);
runtime_set_player_item_count(runtime, player_id, QUERY_ITEM_ROBOT, count);
runtime_set_player_item_count(runtime, player_id, QUERY_ITEM_BOMB, count);
```

道具类型：

```text
QUERY_ITEM_BLOCK：路障
QUERY_ITEM_ROBOT：机器娃娃
QUERY_ITEM_BOMB：炸弹
```

`count` 必须非负，三类道具总数不能超过10。购买或消耗道具后，应传入新的剩余数量。

### 4.4 财神

```c
runtime_set_player_fortune_turns(runtime, player_id, turns);
```

`turns` 必须非负。获得财神时设置为5；每完成一个有效回合后由回合/礼品模块递减。

### 4.5 医院和监狱

```c
runtime_set_player_hospital_turns(runtime, player_id, turns);
runtime_set_player_prison_turns(runtime, player_id, turns);
```

`turns` 为0时清除状态。医院通常设置3回合，监狱通常设置2回合。这两个接口会同步A4回合管理器的跳过状态，不能同时保留医院和监狱状态；后一次设置会覆盖前一次状态。

### 4.6 房产

购买或升级土地：

```c
runtime_assign_property(runtime, player_id, position, building_level);
```

出售、破产或归还土地：

```c
runtime_release_property(runtime, position);
```

限制：

- `position` 必须对应地图上的土地格；
- `player_id` 必须对应现有玩家；
- `building_level` 必须为0～3；
- 释放后所有者恢复为系统，建筑等级恢复为0。

### 4.7 破产标记

```c
runtime_set_player_bankrupt(runtime, player_id, 1); /* 已破产 */
runtime_set_player_bankrupt(runtime, player_id, 0); /* 未破产 */
```

此接口负责更新Query显示状态。完整破产模块还需要调用回合管理器的玩家退出接口，并逐一释放该玩家房产。

## 五、Step n接口

### 5.1 执行命令

推荐通过统一命令分发器调用：

```c
char message[2048];
CommandResult result = command_execute(
    &game, "Step 143", message, sizeof(message));
```

也可以在已验证步数时直接调用运行时：

```c
int result = runtime_step(runtime, 143, message, sizeof(message));
```

`runtime_step` 的步数必须为正整数，不限制为1～6。成功后和普通Roll一样执行逐格移动、落地事件和回合切换。

### 5.2 只解析Step参数

```c
int steps;
StepParseResult result = step_parse_argument("143", &steps);

if (result != STEP_PARSE_OK) {
    printf("%s\n", step_parse_result_message(result));
}
```

可能结果：

```text
STEP_PARSE_OK               解析成功
STEP_PARSE_MISSING          缺少步数
STEP_PARSE_NOT_INTEGER      不是整数
STEP_PARSE_OUT_OF_RANGE     不是int范围内的正整数
STEP_PARSE_EXTRA_ARGUMENT   存在多余参数
```

### 5.3 不使用随机点数的保证

普通 `Roll` 调用：

```c
a4_turn_manager_roll(..., 0);
```

`Step n` 调用：

```c
a4_turn_manager_roll(..., n);
```

当 `forced_steps > 0` 时，移动实现直接采用该数值，不调用 `dice_roll`。因此 `Step 143` 必定移动143步。

`Step 0` 会被拒绝，因为回合管理器约定 `forced_steps == 0` 表示普通随机Roll；拒绝0可以防止遥控骰子意外退化为随机骰子。

## 六、命令返回值

`query_command_execute` 和 `step_command_execute` 返回 `CommandResult`：

```text
COMMAND_OK          执行成功
COMMAND_INVALID     命令参数错误
COMMAND_NOT_ALLOWED 当前状态不能执行
COMMAND_GAME_ENDED  游戏已经结束
```

所有命令忽略大小写，例如 `query`、`QuErY`、`step 5` 和 `StEp 5` 都有效。

## 七、典型团队接入示例

```c
/* 玩家1经过矿地，获得80点。 */
runtime_add_player_points(runtime, 1, 80);

/* 玩家1购买5号土地并升级为洋房。 */
runtime_assign_property(runtime, 1, 5, 2);

/* 玩家1拥有2个路障。 */
runtime_set_player_item_count(runtime, 1, QUERY_ITEM_BLOCK, 2);

/* 玩家1获得5回合财神。 */
runtime_set_player_fortune_turns(runtime, 1, 5);

/* 此时Query会自动显示以上最新状态。 */
runtime_query(runtime, message, sizeof(message));
```

## 八、构建和测试

CMake直接编译本目录中的三个 `.c` 文件：

```text
A7_A18/query_format.c
A7_A18/query_command.c
A7_A18/step_command.c
```

构建并运行全部测试：

```text
cmake -S . -B build_a7_a18 -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build_a7_a18
ctest --test-dir build_a7_a18 --output-on-failure
```

A7/A18专用测试位于 `tests/test_a7_a18.c`。
