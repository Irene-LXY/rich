/*
 * new.c  —— 大富翁 Story A8「Roll 掷骰子移动」实现
 * 依赖：roll.h
 */
#include "roll.h"
#include <stdlib.h>   /* rand */
#include <string.h>   /* strcmp */

/* 玩家角色标识字符，顺序 Q -> A -> S -> J */
const char PLAYER_IDS[MAX_PLAYERS] = { 'Q', 'A', 'S', 'J' };

/* 掷一次骰子，返回 1~6 */
int roll_dice(void)
{
    return DICE_MIN + rand() % (DICE_MAX - DICE_MIN + 1);
}

/* 过路费 = 房产价值 / 2（整除） */
int calc_toll(int house_value)
{
    return house_value / 2;
}

/* 查找指定类型格子的位置，找不到返回 -1 */
int find_cell_pos(const GameState *gs, CellType type)
{
    int i;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (gs->board[i].type == type) {
            return i;
        }
    }
    return -1;
}

/* 根据落点格子的类型映射出落点动作 */
static LandAction cell_to_land(const GameState *gs, int pos)
{
    const Cell *c = &gs->board[pos];
    switch (c->type) {
        case CELL_START:    return LAND_START;
        case CELL_EMPTY:    return LAND_EMPTY;
        case CELL_HOUSE:
            return (c->owner == gs->current_player) ? LAND_OWN_HOUSE
                                                    : LAND_OTHER_HOUSE;
        case CELL_PROP:     return LAND_PROP;
        case CELL_MAGIC:    return LAND_MAGIC;
        case CELL_MINE:     return LAND_MINE;
        case CELL_GIFT:     return LAND_GIFT;
        case CELL_HOSPITAL: return LAND_HOSPITAL;
        case CELL_PRISON:   return LAND_PRISON;
        default:            return LAND_NONE;
    }
}

/*
 * A8 主接口：掷骰子并移动。
 * 说明：本函数为底层移动逻辑，不校验「本回合是否已移动」，
 *       命令层面的校验在 roll_handle_command 中完成。
 */
int roll_dice_move(GameState *gs, int dice, MoveResult *result)
{
    int  cur, pos, step;
    Player *p;
    MoveEvent ev = EV_NONE;

    if (gs == NULL || result == NULL) {
        return ROLL_ERR_INVALID_CMD;
    }

    cur = gs->current_player;
    p   = &gs->players[cur];

    /* 1. 掷骰：dice 在 1~6 之间视为测试注入值，否则随机 */
    result->dice = (dice >= DICE_MIN && dice <= DICE_MAX)
                       ? dice
                       : roll_dice();

    /* 2. 初始化结果 */
    result->from_pos     = p->position;
    result->to_pos       = p->position;
    result->passed_start = 0;
    result->event        = EV_NONE;
    result->land         = LAND_NONE;
    result->toll         = 0;

    pos = p->position;

    /* 3. 逐步移动，途中判定路障 / 炸弹（路障优先于炸弹） */
    for (step = 0; step < result->dice; step++) {
        pos = (pos + 1) % BOARD_SIZE;
        if (pos == START_POS) {
            result->passed_start = 1;   /* 经过起点 */
        }
        if (gs->board[pos].has_barrier) {
            gs->board[pos].has_barrier = 0;   /* 路障消失 */
            ev = EV_BARRIER;
            break;
        }
        if (gs->board[pos].has_bomb) {
            gs->board[pos].has_bomb = 0;      /* 炸弹消失 */
            ev = EV_BOMB;
            break;
        }
    }

    /* 4. 处理路径事件 */
    if (ev == EV_BOMB) {
        /* 被炸伤 -> 送往医院住院 3 天 */
        int hosp = find_cell_pos(gs, CELL_HOSPITAL);
        if (hosp >= 0) {
            pos = hosp;
        }
        p->hospital_days = 3;
        result->event    = EV_BOMB;
    } else if (ev == EV_BARRIER) {
        result->event = EV_BARRIER;
    }

    /* 5. 更新玩家位置 */
    p->position    = pos;
    result->to_pos = pos;

    /* 6. 落点动作判定（被路障/炸弹拦截时不触发落点业务） */
    if (ev == EV_NONE) {
        result->land = cell_to_land(gs, pos);
        switch (result->land) {
            case LAND_OTHER_HOUSE: {
                /* 支付过路费 = 房产价值 / 2 */
                int toll  = calc_toll(gs->board[pos].value);
                int owner = gs->board[pos].owner;
                result->toll = toll;
                p->money -= toll;
                if (owner >= 0 && owner < MAX_PLAYERS) {
                    gs->players[owner].money += toll;
                }
                break;
            }
            case LAND_PRISON:
                p->prison_days = 2;   /* 被扣留 2 天 */
                break;
            case LAND_HOSPITAL:
            case LAND_START:
                break;                /* 无特殊事件 */
            default:
                /* 空地购买 / 房产升级 / 道具屋 / 魔法屋 / 矿地 / 礼品屋
                 * 由对应 Story 依据 result->land 自行处理 */
                break;
        }
    }

    /* 7. 回合结束：标记已行动并切换到下一玩家（新玩家回合重置标记） */
    gs->has_moved       = 1;
    gs->current_player  = (cur + 1) % MAX_PLAYERS;
    gs->has_moved       = 0;

    return ROLL_OK;
}

/*
 * A8 命令入口：解析命令并执行。
 * 命令错误或本回合已移动时，不移动、不切换玩家。
 */
int roll_handle_command(GameState *gs, const char *cmd, int dice, MoveResult *result)
{
    if (gs == NULL || result == NULL || cmd == NULL) {
        return ROLL_ERR_INVALID_CMD;
    }
    if (strcmp(cmd, "Roll") != 0) {
        return ROLL_ERR_INVALID_CMD;   /* 命令错误，不移动，不切换 */
    }
    if (gs->has_moved) {
        return ROLL_ERR_ALREADY_MOVED; /* 本回合已掷过，不移动，不切换 */
    }
    return roll_dice_move(gs, dice, result);
}
