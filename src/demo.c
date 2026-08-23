#include "demo.h"

#include "character.h"
#include "fund.h"
#include "game.h"

#include <stdio.h>

/* 打印所有玩家资金与状态 */
static void show_players(const Game *g)
{
    int i;
    for (i = 0; i < g->player_count; ++i) {
        const Character *c = character_by_id(g->players[i].character);
        printf("    玩家%d（%s）：资金 %ld 元，状态 %s\n",
               g->players[i].number, c->name, (long)g->players[i].fund,
               g->players[i].status == PLAYER_STATUS_NORMAL ? "正常" : "已破产");
    }
}

/* 打印某玩家名下地产 */
static void show_properties_of(const Game *g, int owner_index)
{
    int i, found = 0;
    for (i = 0; i < MAP_POSITION_COUNT; ++i) {
        if (g->properties[i].owned && g->properties[i].owner_index == owner_index) {
            printf("    位置 %d：等级 %d\n", i, g->properties[i].level);
            ++found;
        }
    }
    if (!found) {
        printf("    （无地产）\n");
    }
}

int demo_bankruptcy_run(void)
{
    /* 演示局：3 名玩家（钱夫人 / 阿土伯 / 孙小美），初始资金默认 10000 */
    int   chosen[3] = { CHAR_QIAN_FUREN, CHAR_A_TUBO, CHAR_SUN_XIAOMEI };
    Game *g = game_create(3, chosen);

    if (g == NULL) {
        printf("游戏创建失败。\n");
        return 1;
    }

    printf("================ 破产机制演示（A17 / A21） ================\n");
    printf("本局玩家：玩家1（钱夫人）、玩家2（阿土伯）、玩家3（孙小美）\n");

    /* 初始资金（演示直接使用默认值） */
    if (!game_set_initial_fund(g, INITIAL_FUND_DEFAULT)) {
        printf("初始资金写入失败。\n");
        game_destroy(g);
        return 1;
    }
    printf("\n[初始状态] 初始资金 %d 元：\n", INITIAL_FUND_DEFAULT);
    show_players(g);

    /* 前置：玩家1 购入两块土地 */
    printf("\n[前置] 玩家1 购入位置 6（等级 1）与位置 8（等级 2）的土地：\n");
    game_add_property(g, 6, 0, 1);
    game_add_property(g, 8, 0, 2);
    show_properties_of(g, 0);

    /* 场景 1：交易（付租金）使资金低于 0 → 破产 + 土地归还系统 */
    printf("\n[场景 1] 玩家1 需向 玩家2 支付租金 12000 元（资金 10000 - 12000 < 0）：\n");
    game_charge(g, 0, 12000, 1);
    printf("  玩家2 资金（应收租金入账）：%ld 元\n", (long)g->players[1].fund);
    printf("  玩家1 名下地产：\n");
    show_properties_of(g, 0);

    /* 验证：归还后的空地可被其他玩家重新购买 */
    printf("\n[验证] 归还系统的空地可被其他玩家重新购买：\n");
    if (game_add_property(g, 6, 1, 0)) {
        printf("    玩家2 成功购买位置 6 的空地。\n");
    } else {
        printf("    [异常] 玩家2 购买位置 6 失败！\n");
    }

    /* 场景 2：资金恰好扣到 0 → 不破产 */
    printf("\n[场景 2] 玩家3 向系统缴费 10000 元（资金恰好扣到 0）：\n");
    game_charge(g, 2, 10000, -1);
    printf("  玩家3 状态：%s（资金等于 0 不破产）\n",
           g->players[2].status == PLAYER_STATUS_NORMAL ? "正常" : "已破产");

    /* 场景 3：收费使资金低于 0 → 破产，只剩一名玩家 → 游戏结束 */
    printf("\n[场景 3] 玩家2 向系统缴费 25000 元（资金 22000 - 25000 < 0）：\n");
    game_charge(g, 1, 25000, -1);

    printf("\n[最终状态]\n");
    show_players(g);
    printf("    游戏状态：%s\n",
           g->status == GAME_STATUS_FINISHED ? "已结束" : "进行中");
    printf("===========================================================\n");

    game_destroy(g);
    return 0;
}
