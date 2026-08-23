#include "game.h"

#include "fund.h" /* initial_fund_valid */

#include <stdio.h>
#include <stdlib.h>

/* 前向声明：破产处理（本文件内部使用） */
static void game_declare_bankruptcy(Game *game, int index);

Game *game_create(int count, const int chosen[])
{
    Game *game;
    int   i;

    /* 最后一道防线：选择结果必须完整合法，否则不创建任何数据 */
    if (!player_selection_valid(count, chosen)) {
        return NULL;
    }

    game = (Game *)malloc(sizeof(Game));
    if (game == NULL) {
        return NULL;
    }

    game->players = player_create_all(count, chosen);
    if (game->players == NULL) {
        free(game);
        return NULL;
    }

    /* 全部地产初始化为空地（无人拥有，可供购买） */
    for (i = 0; i < MAP_POSITION_COUNT; ++i) {
        game->properties[i].owned       = 0;
        game->properties[i].owner_index = -1;
        game->properties[i].level       = 0;
    }

    game->player_count  = count;
    game->round         = 1;
    game->current_index = 0;
    game->status        = GAME_STATUS_RUNNING;
    return game;
}

void game_destroy(Game *game)
{
    if (game == NULL) {
        return;
    }
    player_destroy(game->players);
    free(game);
}

int game_set_initial_fund(Game *game, int32_t fund)
{
    int i;

    if (game == NULL) {
        return 0;
    }
    /* 校验失败：不修改任何玩家资金 */
    if (!initial_fund_valid(fund)) {
        return 0;
    }

    /*
     * 单一数值 + 单次循环写入：写入过程没有任何可能失败的操作，
     * 要么全部玩家写入成功，要么一个都不写——原子性，杜绝部分修改。
     */
    for (i = 0; i < game->player_count; ++i) {
        game->players[i].fund = fund;
    }

    /* 复核不变式：所有玩家初始资金必须相同 */
    for (i = 0; i < game->player_count; ++i) {
        if (game->players[i].fund != fund) {
            return 0; /* 理论上不可达 */
        }
    }
    return 1;
}

int game_add_property(Game *game, int position, int owner_index, int level)
{
    if (game == NULL) {
        return 0;
    }
    if (position < 0 || position >= MAP_POSITION_COUNT) {
        return 0;
    }
    if (owner_index < 0 || owner_index >= game->player_count) {
        return 0;
    }
    if (level < 0 || level > 3) {
        return 0;
    }
    if (game->properties[position].owned) {
        return 0; /* 已有主地产不能重复登记 */
    }
    game->properties[position].owned       = 1;
    game->properties[position].owner_index = owner_index;
    game->properties[position].level       = level;
    return 1;
}

int game_active_player_count(const Game *game)
{
    int i, n = 0;

    if (game == NULL) {
        return 0;
    }
    for (i = 0; i < game->player_count; ++i) {
        if (game->players[i].status == PLAYER_STATUS_NORMAL) {
            ++n;
        }
    }
    return n;
}

int game_charge(Game *game, int payer_index, int32_t amount, int creditor_index)
{
    Player         *payer;
    const Character *pc;
    int64_t         diff;

    if (game == NULL || game->status != GAME_STATUS_RUNNING) {
        return 0;
    }
    if (payer_index < 0 || payer_index >= game->player_count) {
        return 0;
    }
    payer = &game->players[payer_index];
    if (payer->status != PLAYER_STATUS_NORMAL) {
        return 0; /* 已破产玩家不再参与交易 */
    }
    if (amount <= 0) {
        return 0;
    }
    if (creditor_index < -1 || creditor_index >= game->player_count) {
        return 0;
    }
    if (creditor_index == payer_index) {
        return 0;
    }

    pc = character_by_id(payer->character);
    if (creditor_index >= 0) {
        const Character *cc = character_by_id(game->players[creditor_index].character);
        printf("交易：玩家%d（%s）向 玩家%d（%s）支付 %ld 元。\n",
               payer->number, pc->name,
               game->players[creditor_index].number, cc->name,
               (long)amount);
    } else {
        printf("收费：玩家%d（%s）向系统支付 %ld 元。\n",
               payer->number, pc->name, (long)amount);
    }

    /*
     * 全额扣款（int64 计算防止 int32 溢出）；
     * 债权人为未破产玩家时入账相同金额。
     */
    diff = (int64_t)payer->fund - (int64_t)amount;
    payer->fund = (diff < INT32_MIN) ? INT32_MIN : (int32_t)diff;
    if (creditor_index >= 0 &&
        game->players[creditor_index].status == PLAYER_STATUS_NORMAL) {
        int64_t sum = (int64_t)game->players[creditor_index].fund + (int64_t)amount;
        game->players[creditor_index].fund = (sum > INT32_MAX) ? INT32_MAX : (int32_t)sum;
    }
    printf("玩家%d 当前资金：%ld 元。\n", payer->number, (long)payer->fund);

    /* 资金低于 0：宣布破产；等于 0 不破产 */
    if (payer->fund < 0) {
        game_declare_bankruptcy(game, payer_index);
    }
    return 1;
}

/*
 * 宣布破产（A17）：
 *   1. 玩家状态置为 BANKRUPT；
 *   2. 名下全部土地归还系统，初始化为空地（可被其他玩家重新购买）；
 *   3. 只剩一名未破产玩家时游戏结束（A21），宣布获胜者。
 */
static void game_declare_bankruptcy(Game *game, int index)
{
    Player         *p = &game->players[index];
    const Character *c = character_by_id(p->character);
    int i, returned = 0;

    p->status = PLAYER_STATUS_BANKRUPT;
    printf("\n*** 破产：玩家%d（%s）资金 %ld 元，低于 0，宣布破产！***\n",
           p->number, c->name, (long)p->fund);

    /* 土地归还系统：恢复为空地 */
    for (i = 0; i < MAP_POSITION_COUNT; ++i) {
        if (game->properties[i].owned && game->properties[i].owner_index == index) {
            game->properties[i].owned       = 0;
            game->properties[i].owner_index = -1;
            game->properties[i].level       = 0;
            ++returned;
            printf("    位置 %d 的土地已归还系统，恢复为空地，可供其他玩家重新购买。\n", i);
        }
    }
    if (returned == 0) {
        printf("    （该玩家名下无土地，无需归还）\n");
    }

    /* 只剩一名未破产玩家时游戏结束 */
    if (game_active_player_count(game) == 1) {
        for (i = 0; i < game->player_count; ++i) {
            if (game->players[i].status == PLAYER_STATUS_NORMAL) {
                const Character *w = character_by_id(game->players[i].character);
                game->status = GAME_STATUS_FINISHED;
                printf("\n★ 只剩一名未破产玩家，游戏结束！获胜者：玩家%d（%s）★\n",
                       game->players[i].number, w->name);
                break;
            }
        }
    } else {
        printf("    游戏继续，剩余 %d 名未破产玩家。\n", game_active_player_count(game));
    }
}

void game_enter_first_round(Game *game)
{
    int i;

    if (game == NULL || game->status != GAME_STATUS_RUNNING) {
        return;
    }

    game->round         = 1;
    game->current_index = 0;

    printf("\n========================================\n");
    printf(" 游戏创建成功，进入第 %d 回合！\n", game->round);
    printf(" 本局玩家（初始资金相同）：\n");
    for (i = 0; i < game->player_count; ++i) {
        const Character *c = character_by_id(game->players[i].character);
        printf("   玩家%d：%s%s\033[0m（地图显示 %c），资金 %ld 元\n",
               game->players[i].number, c->ansi_color, c->name,
               c->symbol, (long)game->players[i].fund);
    }
    {
        const Player    *p = &game->players[game->current_index];
        const Character *c = character_by_id(p->character);
        printf(" 当前行动玩家：玩家%d（%s%s\033[0m，地图显示 %c）\n",
               p->number, c->ansi_color, c->name, c->symbol);
    }
    printf("========================================\n");
}
