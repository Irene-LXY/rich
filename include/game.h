#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#include "player.h"

/* 地图位置总数（接口规范 §3.2：合法编号 0~69） */
#define MAP_POSITION_COUNT 70

/* 游戏状态 */
typedef enum {
    GAME_STATUS_RUNNING  = 0,  /* 进行中 */
    GAME_STATUS_FINISHED = 1   /* 已结束（只剩一名未破产玩家） */
} GameStatus;

/*
 * 地产（按地图位置 0~69 索引）。
 * owned=0 表示空地（无人拥有，可供购买）；
 * 玩家破产时其名下全部地产归还系统并恢复为空地。
 */
typedef struct {
    int owned;        /* 0=空地 / 1=已有主 */
    int owner_index;  /* 拥有者在 players 中的下标；空地时为 -1 */
    int level;        /* 等级 0~3；空地时为 0 */
} Property;

/*
 * 游戏实例。
 * 只有“所有玩家完成角色选择并确认”之后才允许创建；
 * 初始资金在“资金设置确认”之后才统一写入玩家资产。
 */
typedef struct {
    int        player_count;                  /* 玩家人数 2~4 */
    Player    *players;                       /* 玩家数组（按 玩家1~N 顺序） */
    Property   properties[MAP_POSITION_COUNT];/* 地产表，创建时全部为空地 */
    int        round;                         /* 当前回合号，从 1 开始 */
    int        current_index;                 /* 当前行动玩家在 players 中的下标 */
    GameStatus status;                        /* 游戏状态 */
} Game;

/*
 * 创建游戏。
 * 仅当 count 与 chosen 通过 player_selection_valid 校验时才分配数据；
 * 校验失败或内存不足返回 NULL，不产生任何半初始化的玩家数据。
 * 创建成功时：所有地产为空地，所有玩家资金为 0（待初始资金确认后写入）。
 */
Game *game_create(int count, const int chosen[]);

/* 销毁游戏，释放全部相关内存 */
void game_destroy(Game *game);

/*
 * 初始资金统一写入（原子操作）。
 * 仅当 fund 通过 initial_fund_valid() 校验时才执行写入：
 * 单次循环把同一个数值写入所有玩家——要么全部写入，要么一个都不写，
 * 不存在“部分玩家已修改、部分未修改”的中间状态。
 * 成功返回 1；参数非法返回 0（不修改任何玩家资金）。
 */
int game_set_initial_fund(Game *game, int32_t fund);

/*
 * 交易 / 收费：从 payer 资金中扣除 amount，creditor（>=0 时）入账相同金额；
 * creditor_index 传 -1 表示付款给系统（银行）。
 *
 * 扣款后若付款玩家资金低于 0：
 *   1. 宣布该玩家破产（status = PLAYER_STATUS_BANKRUPT）；
 *   2. 该玩家名下全部土地归还系统，初始化为空地（可被重新购买）；
 *   3. 若此时只剩一名未破产玩家，游戏结束（status = GAME_STATUS_FINISHED）。
 * 资金恰好等于 0 不破产。
 *
 * 成功执行返回 1；参数非法 / 游戏已结束 / 付款人已破产返回 0（不做任何修改）。
 */
int game_charge(Game *game, int payer_index, int32_t amount, int creditor_index);

/*
 * 登记地产归属（购买地产；地图模块接入前的资产登记接口，也用于演示/测试）。
 * position 0~69，owner_index 为 players 下标，level 0~3；空地才允许登记。
 * 成功返回 1，否则返回 0。
 */
int game_add_property(Game *game, int position, int owner_index, int level);

/* 统计未破产玩家数量 */
int game_active_player_count(const Game *game);

/* 进入第一个回合：打印回合信息、各玩家资产与当前行动玩家 */
void game_enter_first_round(Game *game);

#endif /* GAME_H */
