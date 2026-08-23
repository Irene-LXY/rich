# Story A8「Roll 掷骰子移动」接口文档

> 所属项目：大富翁（集成开发 · 敏捷迭代）
> 文件位置：`roll/roll.h`（接口声明）、`roll/new.c`（实现）

## 1. 职责边界

A8 只负责掷骰子移动这一条链路上的逻辑，具体为：

> 掷骰子 → 沿路径逐步移动 → 途中判定**路障 / 炸弹** → 判定**落点类型** → 更新玩家位置与回合状态 → **切换到下一玩家**。

落点后的具体业务（购买空地、升级房产、进入道具屋/魔法屋/礼品屋、矿地结算等）**不属于 A8**，A8 通过返回值 `MoveResult.land` 暴露落点类型，由对应 Story 对接处理。

## 2. 基础约定

| 项目 | 值 | 说明 |
|------|----|------|
| 棋盘格数 | `BOARD_SIZE = 66` | 位置下标 `0 ~ 65` |
| 起点 | `START_POS = 0` | 经过起点记为绕圈 |
| 玩家 | `MAX_PLAYERS = 4` | 角色 `Q / A / S / J`，顺序切换 |
| 骰子 | `1 ~ 6` | `dice` 传 0 表示随机 |

## 3. 数据结构（`roll.h`）

```c
typedef enum { PLAYER_Q=0, PLAYER_A=1, PLAYER_S=2, PLAYER_J=3 } PlayerId;

typedef enum { CELL_START=0, CELL_EMPTY, CELL_HOUSE, CELL_PROP,
               CELL_MAGIC, CELL_MINE, CELL_GIFT, CELL_HOSPITAL, CELL_PRISON } CellType;

typedef struct {
    CellType type;
    int owner;       /* 房产所有者(玩家下标)，-1 无主 */
    int value;       /* 房产价值 */
    int level;       /* 房产等级 */
    int has_barrier; /* 路障 1/0 */
    int has_bomb;    /* 炸弹 1/0 */
} Cell;

typedef struct {
    char id;
    int  position;      /* 当前位置 0~65 */
    int  money;
    int  prison_days;   /* 监狱剩余天数 */
    int  hospital_days; /* 医院剩余天数 */
} Player;

typedef struct {
    Cell   board[BOARD_SIZE];
    Player players[MAX_PLAYERS];
    int    current_player; /* 当前玩家下标 0~3 */
    int    has_moved;      /* 本回合是否已移动 1/0 */
} GameState;
```

## 4. 接口函数

### 4.1 `int roll_dice_move(GameState *gs, int dice, MoveResult *result)`

A8 **主接口**：掷骰子并移动。底层函数，**不校验「本回合是否已移动」**。

| 参数 | 类型 | 说明 |
|------|------|------|
| `gs` | `GameState*` | 游戏状态（棋盘与玩家需已初始化） |
| `dice` | `int` | `1~6` 固定点数（测试注入），`0` 表示随机 |
| `result` | `MoveResult*` | 输出移动详情，不可为 NULL |

返回 `ROLL_OK` 成功；`ROLL_ERR_INVALID_CMD` 参数非法。

### 4.2 `int roll_handle_command(GameState *gs, const char *cmd, int dice, MoveResult *result)`

A8 **命令入口**：解析命令并执行（推荐上层调用此函数）。

| 返回码 | 含义 |
|--------|------|
| `ROLL_OK` | 成功移动 |
| `ROLL_ERR_INVALID_CMD` | 命令错误（如 `abc`），不移动、不切换 |
| `ROLL_ERR_ALREADY_MOVED` | 本回合已掷过骰子，不移动、不切换 |

### 4.3 辅助函数

```c
int roll_dice(void);                              /* 掷骰 1~6 */
int calc_toll(int house_value);                   /* 过路费 = value / 2 */
int find_cell_pos(const GameState *gs, CellType type); /* 查格子位置，-1 未找到 */
```

## 5. 返回结构 `MoveResult`

```c
typedef struct {
    int        dice;         /* 掷出点数 1~6 */
    int        from_pos;     /* 出发位置 */
    int        to_pos;       /* 最终落点 */
    int        passed_start; /* 是否经过起点 1/0 */
    MoveEvent  event;        /* EV_NONE / EV_BARRIER / EV_BOMB */
    LandAction land;         /* 落点动作类型 */
    int        toll;         /* 过路费金额（他人房产） */
} MoveResult;
```

### 5.1 `LandAction` 落点动作 → 其它 Story 对接

| `land` 值 | 落点 | A8 行为 | 留给哪个 Story |
|-----------|------|---------|----------------|
| `LAND_EMPTY` | 无主空地 | 仅标记 | 房产购买 Story：提示是否购买 |
| `LAND_OWN_HOUSE` | 自己房产 | 仅标记 | 房产升级 Story：提示是否升级 |
| `LAND_OTHER_HOUSE` | 他人房产 | **已扣款**（`toll` 已算） | 无（A8 已结算过路费） |
| `LAND_PROP` | 道具屋 | 仅标记 | 道具屋 Story：进入购买流程 |
| `LAND_MAGIC` | 魔法屋 | 仅标记 | 魔法屋 Story：进入施展流程 |
| `LAND_MINE` | 矿地 | 仅标记 | 矿地 Story：获取对应点数 |
| `LAND_GIFT` | 礼品屋 | 仅标记 | 礼品屋 Story：选礼物流程 |
| `LAND_HOSPITAL` | 医院 | 无事件 | — |
| `LAND_PRISON` | 监狱 | **已设置** `prison_days=2` | — |
| `LAND_START` | 起点 | 无事件 | — |

### 5.2 `MoveEvent` 路径事件（A8 内部已处理）

| `event` 值 | 场景 | A8 行为 |
|------------|------|---------|
| `EV_BARRIER` | 途中踩到路障 | 停在该格、路障消失、**不触发落点业务** |
| `EV_BOMB` | 途中踩到炸弹 | 送往医院、炸弹消失、`hospital_days=3` |

> 路障优先级高于炸弹：同一路径上先遇到路障即被拦截。

## 6. 使用示例

```c
#include "roll.h"
#include <stdio.h>

int main(void) {
    GameState gs = {0};
    MoveResult r;

    /* 初始化棋盘（地图初始化由对应 Story 负责，这里示意） */
    gs.board[0].type  = CELL_START;
    gs.board[3].type  = CELL_EMPTY;   /* 位置 3：无主空地 */
    gs.board[48].type = CELL_HOSPITAL;
    gs.board[54].type = CELL_PRISON;
    /* ... 其余格子 ... */

    /* 初始化玩家 Q/A/S/J 从起点出发 */
    for (int i = 0; i < MAX_PLAYERS; i++) {
        gs.players[i].id = PLAYER_IDS[i];
        gs.players[i].position = 0;
    }

    int code = roll_handle_command(&gs, "Roll", 3, &r); /* 固定掷 3 点 */
    if (code == ROLL_OK) {
        printf("掷出 %d 点：%d -> %d，落点动作=%d\n",
               r.dice, r.from_pos, r.to_pos, r.land);
    }
    return 0;
}
```

## 7. 测试用例覆盖

`Case_A8_001 ~ Case_A8_022` 全部由 `roll_handle_command` 覆盖：

| 分类 | 用例 | 关键校验点 |
|------|------|-----------|
| 命令错误 | 001 | `ROLL_ERR_INVALID_CMD`，不移动不切换 |
| 重复掷骰 | 002 | `ROLL_ERR_ALREADY_MOVED`，不移动不切换 |
| 无主空地 | 003/013~018 | `land = LAND_EMPTY` |
| 自己房产 | 004 | `land = LAND_OWN_HOUSE` |
| 他人房产 | 005 | `land = LAND_OTHER_HOUSE`，`toll = value/2` |
| 道具/魔法/矿/礼品屋 | 006~009 | `land = LAND_PROP/MAGIC/MINE/GIFT` |
| 医院/监狱 | 010/011 | `land = LAND_HOSPITAL/PRISON`，监狱扣 2 天 |
| 经过起点 | 012/022 | `passed_start = 1` |
| 路障 | 019/021 | `event = EV_BARRIER`，路障消失 |
| 炸弹 | 020 | `event = EV_BOMB`，住院 3 天 |

> 说明：测试时通过 `dice` 参数注入固定点数即可精确到达期望落点；棋盘格子的类型/障碍需在测试前置中按用例设置。

## 8. 依赖与编译

```bash
gcc -o test_roll test_roll.c roll/new.c -I roll
```

- 仅依赖 C 标准库（`stdlib.h`、`string.h`）。
- 使用随机骰子前请调用 `srand((unsigned)time(NULL))` 播种。
