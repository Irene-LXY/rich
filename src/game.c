#include "game.h"

#include <stdio.h>
#include <stdlib.h>

Game *game_create(int count, const int chosen[])
{
    Game *game;

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

    game->player_count  = count;
    game->round         = 1;
    game->current_index = 0;
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

void game_enter_first_round(Game *game)
{
    const Player    *p;
    const Character *c;

    if (game == NULL) {
        return;
    }

    game->round         = 1;
    game->current_index = 0;

    p = &game->players[0];
    c = character_by_id(p->character);

    printf("\n========================================\n");
    printf(" 游戏创建成功，进入第 %d 回合！\n", game->round);
    printf(" 当前行动玩家：玩家%d（%s%s\033[0m，地图显示 %c）\n",
           p->number, c->ansi_color, c->name, c->symbol);
    printf("========================================\n");
}
