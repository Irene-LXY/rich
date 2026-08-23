#include "cli.h"

#include "character.h"
#include "help.h"
#include "input.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* 提取命令名：从行首到第一个空白字符为止（原样，不变换大小写） */
static void command_name(const char *line, char *out, size_t out_size)
{
    size_t i = 0;

    while (line[i] != '\0' && !isspace((unsigned char)line[i]) && i + 1 < out_size) {
        out[i] = line[i];
        ++i;
    }
    out[i] = '\0';
}

/*
 * 已识别但本版本未实现功能的命令（指令集中除 HELP/QUIT 外的命令）。
 * 按接口规范，正式命令统一使用大写，故此处按大写精确匹配。
 */
static int command_known(const char *name)
{
    static const char *const known[] = { "QUERY", "ROLL", "STEP" };
    size_t i;

    for (i = 0; i < sizeof(known) / sizeof(known[0]); ++i) {
        if (strcmp(name, known[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void cli_command_loop(Game *game)
{
    char buf[INPUT_LINE_MAX];
    char name[INPUT_LINE_MAX];

    if (game == NULL) {
        return;
    }

    printf("\n---------------- 指令输入 ----------------\n");
    printf("对局进行中可随时输入 HELP 打开帮助界面（忽略大小写）。\n");

    for (;;) {
        const Player    *p = &game->players[game->current_index];
        const Character *c = character_by_id(p->character);
        char *s;

        printf("\n[第 %d 回合] 玩家%d（%s）> ", game->round, p->number, c->name);
        fflush(stdout);

        if (!input_read_line(buf, sizeof buf)) {
            printf("\n输入结束，退出指令输入。\n");
            return;
        }

        s = input_trim(buf);
        if (*s == '\0') {
            printf("请输入命令；输入 HELP 查看完整指令集。\n");
            continue;
        }

        /* HELP：忽略大小写 —— 本版本完整实现的功能 */
        if (help_is_help_command(s)) {
            help_print();
            continue;
        }

        command_name(s, name, sizeof name);

        /* QUIT：演示性强制结束（完整 Quit 功能由对应模块实现） */
        if (strcmp(name, "QUIT") == 0) {
            printf("QUIT：强制结束，整局游戏结束。\n");
            game->status = GAME_STATUS_FINISHED;
            return;
        }

        /* 已识别、但未实现功能的命令 */
        if (command_known(name)) {
            printf("命令 %s 已识别；其功能由对应模块实现，本版本仅实现 HELP。\n", name);
            printf("输入 HELP 查看完整指令集。\n");
            continue;
        }

        printf("未知命令：%s。输入 HELP 查看完整指令集（忽略大小写）。\n", s);
    }
}
