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
    printf("  用途：打开本帮助界面，随时查看完整指令集。\n");
    printf("  参数：无。\n");
    printf("  示例：HELP        （忽略大小写：help / Help / hElP 均可）\n");
    printf("\n");

    printf("命令：QUERY\n");
    printf("  用途：查询当前玩家状态——资金、点数、房产、剩余道具，\n");
    printf("        以及财神等状态的剩余轮数。\n");
    printf("  参数：无。\n");
    printf("  示例：QUERY\n");
    printf("\n");

    printf("命令：ROLL\n");
    printf("  用途：掷骰子，随机生成 1~6 步，并按点数移动。\n");
    printf("  参数：无。\n");
    printf("  示例：ROLL\n");
    printf("\n");

    printf("命令：STEP n\n");
    printf("  用途：遥控骰子，指定任意行走步数，不使用随机点数。\n");
    printf("  参数：n —— 行走步数（正整数）。\n");
    printf("  示例：STEP 5      （向前移动 5 步）\n");
    printf("\n");

    printf("命令：QUIT\n");
    printf("  用途：强制结束，整局游戏立即结束。\n");
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
