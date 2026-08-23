#ifndef GAME_H
#define GAME_H

#include "player.h"

/*
 * 游戏实例。
 * 只有“所有玩家完成角色选择并确认”之后才允许创建，
 * 创建成功即表示玩家数据已完整初始化，可进入第一个回合。
 */
typedef struct {
    int     player_count;   /* 玩家人数 2~4 */
    Player *players;        /* 完整初始化的玩家数组（按 玩家1~N 顺序） */
    int     round;          /* 当前回合号，从 1 开始 */
    int     current_index;  /* 当前行动玩家在 players 中的下标 */
} Game;

/*
 * 创建游戏。
 * 仅当 count 与 chosen 通过 player_selection_valid 校验时才分配数据；
 * 校验失败或内存不足返回 NULL，不产生任何半初始化的玩家数据。
 */
Game *game_create(int count, const int chosen[]);

/* 销毁游戏，释放全部相关内存 */
void game_destroy(Game *game);

/* 进入第一个回合：打印回合信息与当前行动玩家 */
void game_enter_first_round(Game *game);

#endif /* GAME_H */
