#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

#include "character.h"

/* 玩家人数合法范围：仅允许 2、3、4 名玩家 */
#define MIN_PLAYERS 2
#define MAX_PLAYERS 4

/* 玩家状态 */
typedef enum {
    PLAYER_STATUS_NORMAL   = 0,  /* 正常 */
    PLAYER_STATUS_BANKRUPT = 1   /* 已破产（资金低于 0 时宣布） */
} PlayerStatus;

/*
 * 玩家数据结构。
 * 注意：Player 只在“所有玩家完成角色选择并确认”之后才创建；
 *       fund 在“初始资金确认”之前为 0，确认后由 game_set_initial_fund()
 *       一次性统一写入——任何中间状态都不存在半初始化的玩家数据。
 */
typedef struct {
    int          number;    /* 玩家序号 1~N（即选择顺序 / 回合顺序） */
    CharacterId  character; /* 所选角色编号 */
    int32_t      fund;      /* 资金（元）；低于 0 时宣布破产 */
    PlayerStatus status;    /* 玩家状态 */
} Player;

/*
 * 校验一组角色选择是否合法：
 *   - 人数必须在 2~4 之间
 *   - 每个角色编号必须合法（1~4）
 *   - 同一局内角色不得重复
 * 合法返回 1，否则返回 0。
 */
int player_selection_valid(int count, const int chosen[]);

/*
 * 按已确认的选择结果一次性创建完整初始化的玩家数组。
 *   count : 玩家人数（2~4）
 *   chosen: 长度为 count 的角色编号数组，chosen[i] 为 玩家(i+1) 所选角色
 * 成功返回玩家数组指针（由 player_destroy 释放）；参数非法或内存不足返回 NULL，
 * 此时不产生任何玩家数据。
 */
Player *player_create_all(int count, const int chosen[]);

/* 释放玩家数组 */
void player_destroy(Player *players);

#endif /* PLAYER_H */
