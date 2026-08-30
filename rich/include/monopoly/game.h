#ifndef MONOPOLY_GAME_H
#define MONOPOLY_GAME_H

#include <stdbool.h>

typedef enum {
    GAME_NOT_STARTED = 0,
    GAME_RUNNING,
    GAME_ENDED
} GamePhase;

/*
 * 当前交互位置。A20 的“随时退出”要求 quit 在任何运行中场景均有效。
 * 后续 Story 可以继续增加场景，但不能绕开统一命令分发器。
 */
typedef enum {
    CONTEXT_TURN_START = 0,
    CONTEXT_BUY_CONFIRM,
    CONTEXT_UPGRADE_CONFIRM,
    CONTEXT_TOOL_SHOP,
    CONTEXT_GIFT_HOUSE,
    CONTEXT_MAGIC_HOUSE,
    CONTEXT_HOSPITAL,
    CONTEXT_PRISON,
    CONTEXT_TOLL_SETTLEMENT
} GameContext;

typedef enum {
    END_REASON_NONE = 0,
    END_REASON_USER_QUIT,
    END_REASON_LAST_PLAYER
} GameEndReason;

typedef enum {
    SETUP_PLAYER_COUNT = 0,
    SETUP_INITIAL_MONEY,
    SETUP_ROLE_SELECTION,
    SETUP_COMPLETE
} SetupStep;

/* 集成层运行时句柄；由 src/runtime 提供，命令层与引导层通过它访问
 * 地图、玩家、回合管理、骰子等游戏状态。使用不透明指针以保持实例隔离。 */
struct GameRuntime;

typedef struct {
    GamePhase phase;
    GameContext context;
    GameEndReason end_reason;
    SetupStep setup_step;
    unsigned long state_revision;
    struct GameRuntime *runtime;   /* 集成层运行时，NULL 表示尚未初始化 */
    int setup_player_count;        /* 引导阶段临时：玩家数量 */
    int setup_initial_money;       /* 引导阶段临时：初始资金 */
    int setup_chosen[4];           /* 引导阶段：已选角色编号（1~4），0 表示未选 */
    int setup_choosing;            /* 引导阶段：当前选到第几个玩家（0-based） */
} Game;

void game_init(Game *game);
bool game_start(Game *game);
bool game_end(Game *game, GameEndReason reason);
bool game_is_running(const Game *game);

#endif
