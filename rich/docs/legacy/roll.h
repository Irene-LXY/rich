/*
 * roll.h  —— 大富翁 Story A8「Roll 掷骰子移动」接口定义
 *
 * 职责边界：
 *   A8 负责「掷骰子 -> 沿路径移动 -> 障碍判定(路障/炸弹) -> 落点类型判定
 *         -> 更新玩家位置/回合状态 -> 切换到下一玩家」。
 *   落点后的具体业务（购买空地、升级房产、进入道具屋/魔法屋/礼品屋、矿地结算）
 *   由对应的其它 Story 实现，A8 通过返回的 MoveResult.land 暴露落点类型供上层对接。
 */
#ifndef RICH_ROLL_H
#define RICH_ROLL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

/* ============ 基础常量 ============ */
#define BOARD_SIZE   66   /* 棋盘格数：位置 0 ~ 65 */
#define START_POS    0    /* 起点位置 */
#define MAX_PLAYERS  4    /* 玩家数量 */
#define DICE_MIN     1
#define DICE_MAX     6

/* 玩家角色编号（顺序即回合顺序 Q -> A -> S -> J） */
typedef enum {
    PLAYER_Q = 0,
    PLAYER_A = 1,
    PLAYER_S = 2,
    PLAYER_J = 3
} PlayerId;

/* 玩家角色标识字符 */
extern const char PLAYER_IDS[MAX_PLAYERS]; /* {'Q','A','S','J'} */

/* ============ 棋盘与格子 ============ */

/* 格子类型 */
typedef enum {
    CELL_START = 0,  /* 起点 */
    CELL_EMPTY,      /* 无主空地 */
    CELL_HOUSE,      /* 房产（有主） */
    CELL_PROP,       /* 道具屋 */
    CELL_MAGIC,      /* 魔法屋 */
    CELL_MINE,       /* 矿地 */
    CELL_GIFT,       /* 礼品屋 */
    CELL_HOSPITAL,   /* 医院 */
    CELL_PRISON      /* 监狱 */
} CellType;

/* 单个格子 */
typedef struct {
    CellType type;        /* 格子类型 */
    int      owner;       /* 房产所有者(玩家下标)，-1 表示无主（CELL_HOUSE 有效） */
    int      value;       /* 房产价值（CELL_HOUSE） */
    int      level;       /* 房产等级（CELL_HOUSE） */
    int      has_barrier; /* 路障：1 有 / 0 无 */
    int      has_bomb;    /* 炸弹：1 有 / 0 无 */
} Cell;

/* ============ 玩家与游戏状态 ============ */

/* 玩家状态 */
typedef struct {
    char id;            /* 角色标识 'Q'/'A'/'S'/'J' */
    int  position;      /* 当前位置（格子下标 0~65） */
    int  money;         /* 现金 */
    int  prison_days;   /* 监狱扣留剩余天数 */
    int  hospital_days; /* 医院住院剩余天数 */
} Player;

/* 游戏整体状态 */
typedef struct {
    Cell    board[BOARD_SIZE];    /* 棋盘 */
    Player  players[MAX_PLAYERS]; /* 玩家 */
    int     current_player;       /* 当前玩家下标 0~3 */
    int     has_moved;            /* 本回合是否已移动 0/1 */
} GameState;

/* ============ A8 接口返回类型 ============ */

/* 落点动作类型（移动结束后应触发的业务） */
typedef enum {
    LAND_NONE = 0,    /* 无特殊（起点/医院） */
    LAND_EMPTY,       /* 无主空地 -> 提示是否购买 */
    LAND_OWN_HOUSE,   /* 自己房产 -> 提示是否升级 */
    LAND_OTHER_HOUSE, /* 他人房产 -> 支付过路费 */
    LAND_PROP,        /* 道具屋 -> 进入购买流程 */
    LAND_MAGIC,       /* 魔法屋 -> 进入施展流程 */
    LAND_MINE,        /* 矿地 -> 获取对应点数 */
    LAND_GIFT,        /* 礼品屋 -> 选礼物流程 */
    LAND_HOSPITAL,    /* 医院 -> 无特殊事件 */
    LAND_PRISON,      /* 监狱 -> 扣留 2 天 */
    LAND_START        /* 起点 -> 无特殊 */
} LandAction;

/* 移动结果码 */
typedef enum {
    ROLL_OK = 0,           /* 成功移动 */
    ROLL_ERR_INVALID_CMD,  /* 命令错误 */
    ROLL_ERR_ALREADY_MOVED /* 本回合已移动 */
} RollCode;

/* 路径事件（移动途中发生） */
typedef enum {
    EV_NONE = 0,   /* 无 */
    EV_BARRIER,    /* 被路障拦截 */
    EV_BOMB        /* 被炸弹炸伤 */
} MoveEvent;

/* 移动结果详情 */
typedef struct {
    int        dice;         /* 掷出的点数 1~6 */
    int        from_pos;     /* 出发位置 */
    int        to_pos;       /* 最终落点位置 */
    int        passed_start; /* 是否经过起点 0/1 */
    MoveEvent  event;        /* 路径事件（路障/炸弹） */
    LandAction land;         /* 落点动作类型 */
    int        toll;         /* 过路费金额（他人房产时 = value/2） */
} MoveResult;

/* ============ A8 接口函数 ============ */

/* 掷一次骰子，返回 1~6 */
int roll_dice(void);

/*
 * A8 主接口：掷骰子并移动。
 *   gs     : 游戏状态（需已初始化棋盘与玩家）
 *   dice   : 1~6 固定点数（测试注入用）；传 0 表示随机掷骰
 *   result : 输出本次移动的详细信息（不可为 NULL）
 * 返回：
 *   ROLL_OK            成功，result 已填充
 *   ROLL_ERR_*         错误，result 不保证有效，游戏状态不变
 */
int roll_dice_move(GameState *gs, int dice, MoveResult *result);

/*
 * A8 命令入口：解析命令并执行。
 *   cmd    : 命令字符串，如 "Roll"
 *   dice   : 1~6 固定点数；0 表示随机
 *   result : 输出移动结果
 * 返回：
 *   ROLL_OK / ROLL_ERR_INVALID_CMD / ROLL_ERR_ALREADY_MOVED
 */
int roll_handle_command(GameState *gs, const char *cmd, int dice, MoveResult *result);

/* 计算过路费 = 房产价值 / 2（整除） */
int calc_toll(int house_value);

/* 查找指定类型格子的位置（如医院/监狱），找不到返回 -1 */
int find_cell_pos(const GameState *gs, CellType type);

#ifdef __cplusplus
}
#endif

#endif /* RICH_ROLL_H */
