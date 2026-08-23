#ifndef PLAYER_H
#define PLAYER_H

#include "character.h"

/* 玩家人数合法范围：仅允许 2、3、4 名玩家 */
#define MIN_PLAYERS 2
#define MAX_PLAYERS 4

/*
 * 玩家数据结构。
 * 注意：Player 只在“所有玩家完成选择并确认”之后才创建，
 *       任何选择中途都不存在半初始化的玩家数据。
 */
typedef struct {
    int          number;    /* 玩家序号 1~N（即选择顺序 / 回合顺序） */
    CharacterId  character; /* 所选角色编号 */
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
