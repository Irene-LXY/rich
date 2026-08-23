#include "help.h"

#include <ctype.h>
#include <stdio.h>

void help_print(void)
{
    printf("\n");
    printf("======================================================\n");
    printf("                  帮助 · 完整指令集\n");
    printf("======================================================\n");

    printf("命令：HELP\n");
    printf("  用途：查看命令帮助——打开本帮助界面，随时查看完整指令集。\n");
    printf("  参数：无。\n");
    printf("  示例：HELP        （忽略大小写：help / Help / hElP 均可）\n");
    printf("\n");

    printf("命令：ROLL\n");
    printf("  用途：掷骰子命令，行走 1~6 步，步数由随机算法产生。\n");
    printf("  参数：无。\n");
    printf("  示例：ROLL\n");
    printf("\n");

    printf("命令：STEP n\n");
    printf("  用途：遥控骰子，按指定步数行走。\n");
    printf("  参数：n —— 指定的步数（正整数）。\n");
    printf("  示例：STEP 5      （向前移动 5 步）\n");
    printf("\n");

    printf("命令：SELL n\n");
    printf("  用途：轮到自己时，出售自己的任意房产，\n");
    printf("        出售价格为投资总成本的 2 倍。\n");
    printf("  参数：n —— 房产在地图上的绝对位置。\n");
    printf("  示例：SELL 23     （出售位于 23 号的房产）\n");
    printf("\n");

    printf("命令：BLOCK n\n");
    printf("  用途：将路障放置到离当前位置前后 10 步的任何位置；\n");
    printf("        任一玩家经过路障，将被拦截。该道具一次有效。\n");
    printf("  参数：n —— 前后的相对距离（-10~10），负数表示后方。\n");
    printf("  示例：BLOCK 3（放到前方 3 步）；BLOCK -2（放到后方 2 步）\n");
    printf("\n");

    printf("命令：BOMB n\n");
    printf("  用途：将炸弹放置到离当前位置前后 10 步的任何位置；任一玩家\n");
    printf("        经过该位置，将被炸伤，送往医院，住院三天。\n");
    printf("  参数：n —— 前后的相对距离（-10~10），负数表示后方。\n");
    printf("  示例：BOMB 5 （放到前方 5 步）；BOMB -1（放到后方 1 步）\n");
    printf("\n");

    printf("命令：ROBOT\n");
    printf("  用途：使用机器人清扫前方路面上 10 步内的任何其他道具，\n");
    printf("        如炸弹、路障。\n");
    printf("  参数：无。\n");
    printf("  示例：ROBOT\n");
    printf("\n");

    printf("命令：QUERY\n");
    printf("  用途：显示自家资产。\n");
    printf("  参数：无。\n");
    printf("  示例：QUERY\n");
    printf("\n");

    printf("命令：QUIT\n");
    printf("  用途：强制退出，整局游戏立即结束。\n");
    printf("  参数：无。\n");
    printf("  示例：QUIT\n");

    printf("======================================================\n");
    printf("提示：对局进行中可随时输入 HELP 重新打开本帮助界面。\n");
}

int help_is_help_command(const char *line)
{
    const char *cmd = "help";

    if (line == NULL) {
        return 0;
    }
    while (*cmd != '\0') {
        if (*line == '\0') {
            return 0; /* 输入比 "help" 短，如 "hel" */
        }
        if (tolower((unsigned char)*line) != *cmd) {
            return 0;
        }
        ++line;
        ++cmd;
    }
    return *line == '\0'; /* 整词匹配："helpme" 不算 */
}
