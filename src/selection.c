#include "selection.h"

#include "character.h"
#include "player.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BUF_SIZE 128

#define ANSI_RESET "\033[0m"

/*
 * 读取一行输入。
 * 成功返回 1；遇到 EOF / 读错误返回 0。
 * 行长度超过缓冲区时丢弃本行剩余内容，并视为空输入（非法）。
 */
static int read_line(char *buf, size_t size)
{
    if (fgets(buf, (int)size, stdin) == NULL) {
        return 0; /* EOF */
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
            /* 丢弃超长部分 */
        }
        buf[0] = '\0';
        return 1;
    }
    return 1;
}

/* 去掉首尾空白字符，返回指向有效内容的指针 */
static char *trim(char *s)
{
    char *end;

    while (*s != '\0' && isspace((unsigned char)*s)) {
        ++s;
    }
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return s;
}

/*
 * 解析“整行必须恰好是一个整数”。
 * 成功返回 1 并写入 *out；含任何非数字字符、空串均返回 0。
 */
static int parse_int(const char *s, long *out)
{
    char *end = NULL;
    long  v;

    if (*s == '\0') {
        return 0;
    }
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0') {
        return 0; /* 含非法字符 */
    }
    *out = v;
    return 1;
}

/* 第一步：输入玩家人数（2~4）。成功返回 1，EOF 返回 0。 */
static int input_player_count(int *out_count)
{
    char buf[LINE_BUF_SIZE];
    long value;

    for (;;) {
        printf("\n请输入玩家人数（%d~%d 人）：", MIN_PLAYERS, MAX_PLAYERS);
        fflush(stdout);

        if (!read_line(buf, sizeof buf)) {
            printf("\n输入已结束，游戏设置取消。\n");
            return 0;
        }

        if (!parse_int(trim(buf), &value)) {
            printf("输入无效：请输入整数形式的人数，合法范围为 %d~%d 人。\n",
                   MIN_PLAYERS, MAX_PLAYERS);
            continue;
        }
        if (value < MIN_PLAYERS || value > MAX_PLAYERS) {
            printf("输入无效：本游戏仅支持 %d~%d 名玩家，请重新输入。\n",
                   MIN_PLAYERS, MAX_PLAYERS);
            continue;
        }
        *out_count = (int)value;
        return 1;
    }
}

/* 显示当前选择进度（已完成的玩家及其角色） */
static void show_progress(const int chosen[], int filled)
{
    int i;

    if (filled == 0) {
        return;
    }
    printf("已完成选择：");
    for (i = 0; i < filled; ++i) {
        const Character *c = character_by_id(chosen[i]);
        printf("玩家%d→%s%s%s(%c)%s",
               i + 1, c->ansi_color, c->name, ANSI_RESET, c->symbol,
               (i + 1 < filled) ? "，" : "\n");
    }
}

/* 显示剩余可选角色（已被选择的角色不再出现在可选项中） */
static void show_available_characters(const int taken[])
{
    const Character *table = character_table();
    int i;

    printf("剩余可选角色：\n");
    for (i = 0; i < CHARACTER_COUNT; ++i) {
        if (taken[table[i].id]) {
            continue;
        }
        printf("  %d. %s%s%s（%s，地图显示 %c）\n",
               (int)table[i].id,
               table[i].ansi_color, table[i].name, ANSI_RESET,
               table[i].color_name, table[i].symbol);
    }
}

/*
 * 第二步：引导玩家 player_no 选择一个角色。
 *   taken[]          : taken[id] 非 0 表示角色 id 已被选
 *   chosen[], filled : 已完成的角色选择（用于提示“被谁选走”）
 * 成功返回 1 并写入 *out_id；EOF 返回 0。
 */
static int input_character_for_player(int player_no,
                                      const int taken[],
                                      const int chosen[], int filled,
                                      int *out_id)
{
    char buf[LINE_BUF_SIZE];
    long value;
    int  i;

    for (;;) {
        printf("\n----------------------------------------\n");
        printf("当前操作：玩家%d\n", player_no);
        show_progress(chosen, filled);
        show_available_characters(taken);
        printf("玩家%d，请输入角色编号选择角色：", player_no);
        fflush(stdout);

        if (!read_line(buf, sizeof buf)) {
            printf("\n输入已结束，游戏设置取消。\n");
            return 0;
        }

        if (!parse_int(trim(buf), &value)) {
            printf("选择失败：请输入数字形式的角色编号（1~4），不接受其他字符，请重新选择。\n");
            continue;
        }
        if (!character_id_valid(value)) {
            printf("选择失败：不存在编号为 %ld 的角色，可选编号为 1~4，请重新选择。\n", value);
            continue;
        }
        if (taken[value]) {
            const Character *c = character_by_id((int)value);
            int who = 0;
            for (i = 0; i < filled; ++i) {
                if (chosen[i] == (int)value) {
                    who = i + 1;
                    break;
                }
            }
            printf("选择失败：角色「%s」已被 玩家%d 选择，同一局内角色不得重复，请从剩余角色中重新选择。\n",
                   c->name, who);
            continue;
        }

        {
            const Character *c = character_by_id((int)value);
            printf("玩家%d 选择了 %s%s%s（%s，地图显示 %c）。\n",
                   player_no, c->ansi_color, c->name, ANSI_RESET,
                   c->color_name, c->symbol);
        }
        *out_id = (int)value;
        return 1;
    }
}

/*
 * 第三步：展示选择结果并等待确认。
 * 返回 1 = 确认；0 = 放弃本次选择、重新设置；-1 = 输入结束（EOF）。
 */
static int confirm_selection(int count, const int chosen[])
{
    char buf[LINE_BUF_SIZE];
    int  i;

    for (;;) {
        printf("\n========== 选择结果确认 ==========\n");
        for (i = 0; i < count; ++i) {
            const Character *c = character_by_id(chosen[i]);
            printf("  玩家%d：%s%s%s（%s，地图显示 %c）\n",
                   i + 1, c->ansi_color, c->name, ANSI_RESET,
                   c->color_name, c->symbol);
        }
        printf("确认以上选择并创建游戏？(Y/N)：");
        fflush(stdout);

        if (!read_line(buf, sizeof buf)) {
            printf("\n输入已结束，游戏设置取消。\n");
            return -1;
        }

        {
            char *s = trim(buf);
            if (strlen(s) == 1 && (s[0] == 'Y' || s[0] == 'y')) {
                return 1;
            }
            if (strlen(s) == 1 && (s[0] == 'N' || s[0] == 'n')) {
                return 0;
            }
            printf("输入无效：请输入 Y（确认）或 N（重新选择）。\n");
        }
    }
}

int selection_wizard(int *out_count, int out_chosen[])
{
    int count;
    int chosen[MAX_PLAYERS];
    int taken[CHARACTER_COUNT + 1];
    int i, r;

    if (out_count == NULL || out_chosen == NULL) {
        return 0;
    }

    for (;;) {
        /* 1. 玩家人数 */
        if (!input_player_count(&count)) {
            return 0;
        }

        /* 2. 按 玩家1~玩家N 顺序依次选择角色 */
        memset(taken, 0, sizeof taken);
        for (i = 0; i < count; ++i) {
            if (!input_character_for_player(i + 1, taken, chosen, i, &chosen[i])) {
                return 0;
            }
            taken[chosen[i]] = 1;
        }

        /* 3. 确认 */
        r = confirm_selection(count, chosen);
        if (r < 0) {
            return 0; /* EOF */
        }
        if (r == 0) {
            /* 未确认：丢弃本次选择意向，不生成任何玩家数据，重新设置 */
            printf("\n已取消本次选择，请重新设置游戏。\n");
            continue;
        }

        /* 已确认：把选择结果交还调用方，由调用方创建游戏 */
        *out_count = count;
        memcpy(out_chosen, chosen, sizeof chosen);
        return 1;
    }
}
