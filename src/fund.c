#include "fund.h"

#include "character.h"
#include "input.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int initial_fund_valid(int64_t fund)
{
    return fund >= INITIAL_FUND_MIN && fund <= INITIAL_FUND_MAX;
}

/*
 * 第一步：输入初始资金。
 * 合法输入：纯数字整数（允许首尾空格），范围 1000~50000；空行 = 默认值 10000。
 * 成功返回 1 并写入 *out_fund（*out_used_default 标记是否采用默认值）；EOF 返回 0。
 */
static int input_initial_fund(int32_t *out_fund, int *out_used_default)
{
    char buf[INPUT_LINE_MAX];

    for (;;) {
        char *s;
        char *p;
        int   all_digits;
        long long v;
        char *end = NULL;

        printf("\n请输入初始资金（%d~%d 元的整数，直接回车使用默认值 %d 元）：",
               INITIAL_FUND_MIN, INITIAL_FUND_MAX, INITIAL_FUND_DEFAULT);
        fflush(stdout);

        if (!input_read_line(buf, sizeof buf)) {
            printf("\n输入已结束，初始资金设置取消。\n");
            return 0;
        }

        s = input_trim(buf); /* 首尾空格容忍，去掉后再判断 */

        /* 空输入：采用默认初始资金 */
        if (*s == '\0') {
            *out_fund         = INITIAL_FUND_DEFAULT;
            *out_used_default = 1;
            printf("未输入数值，将采用默认初始资金 %d 元。\n", INITIAL_FUND_DEFAULT);
            return 1;
        }

        /* 纯数字校验：拒绝小数、文字以及空格以外的任何非法字符 */
        all_digits = 1;
        for (p = s; *p != '\0'; ++p) {
            if (!isdigit((unsigned char)*p)) {
                all_digits = 0;
                break;
            }
        }
        if (!all_digits) {
            printf("输入无效：初始资金必须为纯数字整数（不支持小数、文字或其他字符）；\n"
                   "合法格式：%d~%d 的整数，例如 10000；直接回车使用默认值 %d。请重新输入。\n",
                   INITIAL_FUND_MIN, INITIAL_FUND_MAX, INITIAL_FUND_DEFAULT);
            continue;
        }

        /* 全数字串解析（strtoll 溢出会得到 LLONG_MAX/LLONG_MIN，范围检查必然拦截） */
        v = strtoll(s, &end, 10);
        if (!initial_fund_valid(v)) {
            printf("输入无效：%lld 超出范围，初始资金合法范围为 %d~%d 元，请重新输入。\n",
                   v, INITIAL_FUND_MIN, INITIAL_FUND_MAX);
            continue;
        }

        *out_fund         = (int32_t)v;
        *out_used_default = 0;
        return 1;
    }
}

/*
 * 第二步：显示最终采用的初始资金数值并等待确认。
 * 返回 1 = 确认；0 = 取消本次设置、重新输入；-1 = 输入结束（EOF）。
 */
static int confirm_initial_fund(const Game *game, int32_t fund, int used_default)
{
    char buf[INPUT_LINE_MAX];

    for (;;) {
        printf("\n========== 初始资金确认 ==========\n");
        if (used_default) {
            printf("  （未输入数值，采用默认值）\n");
        }
        printf("  最终采用的初始资金：每名玩家 %ld 元\n", (long)fund);
        printf("  本局共 %d 名玩家，所有玩家的初始资金相同。\n", game->player_count);
        printf("确认发放初始资金？(Y/N)：");
        fflush(stdout);

        if (!input_read_line(buf, sizeof buf)) {
            printf("\n输入已结束，初始资金设置取消。\n");
            return -1;
        }

        {
            char *s = input_trim(buf);
            if (strlen(s) == 1 && (s[0] == 'Y' || s[0] == 'y')) {
                return 1;
            }
            if (strlen(s) == 1 && (s[0] == 'N' || s[0] == 'n')) {
                return 0;
            }
            printf("输入无效：请输入 Y（确认）或 N（重新设置）。\n");
        }
    }
}

int fund_wizard(Game *game)
{
    int32_t fund;
    int     used_default;
    int     r, i;

    if (game == NULL) {
        return 0;
    }

    for (;;) {
        /* 1. 输入并校验初始资金（非法输入留在本步骤重新输入） */
        if (!input_initial_fund(&fund, &used_default)) {
            return 0; /* EOF：未写入任何资金 */
        }

        /* 2. 显示最终采用的数值并确认 */
        r = confirm_initial_fund(game, fund, used_default);
        if (r < 0) {
            return 0; /* EOF */
        }
        if (r == 0) {
            /* 取消：未写入任何玩家资金，重新设置 */
            printf("\n已取消本次初始资金设置（未修改任何玩家资金），请重新设置。\n");
            continue;
        }

        /* 3. 确认后：原子写入所有玩家资产 */
        if (!game_set_initial_fund(game, fund)) {
            printf("错误：初始资金写入失败（数值非法），未修改任何玩家资金。\n");
            continue;
        }

        printf("\n初始资金已统一发放：");
        for (i = 0; i < game->player_count; ++i) {
            const Character *c = character_by_id(game->players[i].character);
            printf("玩家%d（%s）%ld 元%s",
                   game->players[i].number, c->name, (long)game->players[i].fund,
                   (i + 1 < game->player_count) ? "，" : "。\n");
        }
        return 1;
    }
}
