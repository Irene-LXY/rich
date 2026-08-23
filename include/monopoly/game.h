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

typedef struct {
    GamePhase phase;
    GameContext context;
    GameEndReason end_reason;
    SetupStep setup_step;
    unsigned long state_revision;
} Game;

void game_init(Game *game);
bool game_start(Game *game);
bool game_end(Game *game, GameEndReason reason);
bool game_is_running(const Game *game);

#endif
