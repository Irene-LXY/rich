#include <stdio.h>
#include <string.h>

#include "character.h"
#include "cli.h"
#include "console.h"
#include "demo.h"
#include "fund.h"
#include "game.h"
#include "help.h"
#include "player.h"
#include "selection.h"

int main(int argc, char *argv[])
{
    int   count = 0;
    int   chosen[MAX_PLAYERS] = {0};
    Game *game = NULL;
    int   i;

    console_setup();

    /* 帮助直查：bin\A6.exe --help（无需进入游戏，随时可查看完整指令集） */
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        help_print();
        return 0;
    }

    /* 破产机制演示模式：bin\A6.exe --demo */
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        return demo_bankruptcy_run();
    }

    printf("========================================\n");
    printf("   大富翁 RichMan —— 游戏开局设置\n");
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
     * 第一步：角色选择向导。
     * 只返回“选择意向”，期间不产生任何玩家 / 游戏数据。
     */
    if (!selection_wizard(&count, chosen)) {
        printf("游戏设置未完成，程序结束。\n");
        return 0;
    }

    /* 角色选择确认后：创建游戏与玩家（资金为 0，待初始资金确认后写入） */
    game = game_create(count, chosen);
    if (game == NULL) {
        fprintf(stderr, "错误：游戏创建失败（内存不足或选择数据非法）。\n");
        return 1;
    }

    /*
     * 第二步：初始资金设置。
     * 确认后才统一写入所有玩家资产；取消/输入结束时不修改任何玩家资金。
     */
    if (!fund_wizard(game)) {
        printf("初始资金未确认，游戏未开始（未写入任何玩家资金）。\n");
        game_destroy(game);
        return 0;
    }

    /* 进入第一个回合 */
    game_enter_first_round(game);

    /*
     * 指令输入循环（A6）：
     * 对局进行中可随时输入 HELP（忽略大小写）打开帮助界面查看完整指令集；
     * 其余命令仅识别，功能由对应模块实现。
     */
    cli_command_loop(game);

    game_destroy(game);
    return 0;
}
