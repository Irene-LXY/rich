#include <stdio.h>

#include "character.h"
#include "console.h"
#include "game.h"
#include "player.h"
#include "selection.h"

int main(void)
{
    int   count = 0;
    int   chosen[MAX_PLAYERS] = {0};
    Game *game = NULL;
    int   i;

    console_setup();

    printf("========================================\n");
    printf("   大富翁 RichMan —— 游戏开局角色选择\n");
    printf("========================================\n");
    printf("可选角色一览：\n");
    {
        const Character *table = character_table();
        for (i = 0; i < CHARACTER_COUNT; ++i) {
            printf("  %d. %s%s\033[0m（%s，地图显示 %c）\n",
                   (int)table[i].id, table[i].ansi_color, table[i].name,
                   table[i].color_name, table[i].symbol);
        }
    }

    /*
     * 收集并确认选择。
     * 注意：wizard 只返回“选择意向”，期间不产生任何玩家 / 游戏数据；
     * 选择失败或尚未确认时不会创建半初始化的玩家数据。
     */
    if (!selection_wizard(&count, chosen)) {
        printf("游戏设置未完成，程序结束。\n");
        return 0;
    }

    /* 所有玩家完成选择并已确认 —— 此刻才创建游戏与玩家数据 */
    game = game_create(count, chosen);
    if (game == NULL) {
        fprintf(stderr, "错误：游戏创建失败（内存不足或选择数据非法）。\n");
        return 1;
    }

    /* 进入第一个回合 */
    game_enter_first_round(game);

    /* TODO: 后续模块（地图、回合循环、命令处理等）在此接入 */

    game_destroy(game);
    return 0;
}
