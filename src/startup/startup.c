#include "monopoly/startup.h"
#include "monopoly/runtime.h"
#include "monopoly/character.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_message(char *message, size_t size, const char *text) {
    if (message != 0 && size > 0) {
        (void)snprintf(message, size, "%s", text);
    }
}

/*
 * 解析“整行必须恰好是一个纯数字整数”。
 * 空串或含任何非数字字符（小数点、正负号 +/-、字母、指数、逗号、
 * 中间空格等）均返回 0；成功返回 1 并写入 *value。
 * 玩家人数、初始资金、角色编号统一只接受纯数字输入。
 */
static int parse_strict_int(const char *input, long *value) {
    const char *cursor;

    if (*input == '\0') {
        return 0;
    }

    /* 纯数字校验：拒绝小数、文字、正负号或其他字符
       （strtol 本身会容忍 '+'/'-' 号，必须先拦住） */
    for (cursor = input; *cursor != '\0'; ++cursor) {
        if (!isdigit((unsigned char)*cursor)) {
            return 0;
        }
    }

    /* 全数字串解析（strtol 溢出会得到 LONG_MAX，调用方的范围检查必然拦截） */
    *value = strtol(input, 0, 10);
    return 1;
}

/*
 * 解析“组合选择输入”：一行 2~4 位数字，
 * 位数即玩家人数，第 i 位数字是 玩家i 选择的角色编号（1~4）。
 *   例："21"   → 玩家1=2号阿土伯，玩家2=1号钱夫人
 *       "312"  → 玩家1=3号孙小美，玩家2=1号钱夫人，玩家3=2号阿土伯
 *       "4321" → 4 名玩家依次为 4/3/2/1 号角色
 *
 * 校验顺序（任一失败即写入带原因的提示并返回 0，不产生任何数据）：
 *   1. 字符校验：必须全部为数字——小数点、正负号、字母、中间空格等一律拒绝；
 *   2. 长度校验：位数即人数，仅允许 2~4 位（空串 / 1 位 / 5 位以上都在这里拦）；
 *   3. 编号校验：每位必须是已存在的角色编号 1~4（0、5~9 报“不存在”）；
 *   4. 重复校验：同一局内角色不得重复。
 * 合法返回 1 并写入 *player_count / chosen[]。
 */
static int parse_selection_line(const char *input, int *player_count, int chosen[],
                                char *message, size_t message_size)
{
    size_t length = strlen(input);
    size_t i, j;
    int    taken[CHARACTER_COUNT + 1] = {0};

    /* 1. 字符校验：必须全部是数字（垃圾字符优先提示，与资金模块口径一致） */
    for (i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)input[i])) {
            write_message(message, message_size,
                          "输入无效：请输入数字形式的角色编号（1~4），不接受正负号、小数点、字母或其他字符，请重新输入。\n");
            return 0;
        }
    }

    /* 2. 长度校验：位数即玩家人数，仅允许 2~4 位 */
    if (length < 2 || length > 4) {
        write_message(message, message_size,
                      "输入无效：本游戏仅支持 2~4 名玩家，请输入 2~4 位角色编号组合（如 21、312、4321），请重新输入。\n");
        return 0;
    }

    /* 3. 编号校验：每位必须是已存在的角色编号（1~4） */
    for (i = 0; i < length; ++i) {
        int id = input[i] - '0';
        if (!character_id_valid(id)) {
            char reason[256];
            (void)snprintf(reason, sizeof(reason),
                           "输入无效：不存在编号为 %d 的角色（第 %d 位），可选编号为 1~4，请重新输入。\n",
                           id, (int)i + 1);
            write_message(message, message_size, reason);
            return 0;
        }
    }

    /* 4. 重复校验：同一局内角色不得重复 */
    for (i = 0; i < length; ++i) {
        int id = input[i] - '0';
        if (taken[id]) {
            int first = 0;
            char reason[256];
            for (j = 0; j < i; ++j) {
                if (input[j] - '0' == id) {
                    first = (int)j + 1;
                    break;
                }
            }
            (void)snprintf(reason, sizeof(reason),
                           "选择失败：角色「%s」已被 玩家%d 选择，同一局内角色不得重复，请重新输入。\n",
                           character_by_id(id)->name, first);
            write_message(message, message_size, reason);
            return 0;
        }
        taken[id] = 1;
    }

    *player_count = (int)length;
    for (i = 0; i < length; ++i) {
        chosen[i] = input[i] - '0';
    }
    return 1;
}

/* 组合输入提示：列出全部角色编号，引导一行输入“人数+角色”组合。 */
static void write_selection_prompt(char *message, size_t size)
{
    const Character *table = character_table();
    char buf[512];
    size_t used = 0;
    int i;
    int written;

    written = snprintf(buf, sizeof(buf),
                       "请选择2~4位不重复玩家，输入编号组队（");
    if (written > 0) {
        used = (size_t)written;
    }
    for (i = 0; i < CHARACTER_COUNT; ++i) {
        if (used < sizeof(buf)) {
            written = snprintf(buf + used, sizeof(buf) - used, "%s%d.%s",
                               (i > 0) ? " " : "",
                               (int)table[i].id, table[i].name);
            if (written > 0) {
                used += (size_t)written;
            }
        }
    }
    (void)snprintf(buf + used, sizeof(buf) - used, "）：\n");
    write_message(message, size, buf);
}

/* 回显解析结果（玩家i → 角色），并提示输入初始资金。 */
static void write_assignment_echo(char *message, size_t size,
                                  const int *chosen, int player_count)
{
    char buf[512];
    size_t used = 0;
    int i;
    int written;

    written = snprintf(buf, sizeof(buf), "已选择 %d 名玩家：", player_count);
    if (written > 0) {
        used = (size_t)written;
    }
    for (i = 0; i < player_count; ++i) {
        const Character *c = character_by_id(chosen[i]);
        if (used < sizeof(buf)) {
            written = snprintf(buf + used, sizeof(buf) - used, "玩家%d=%s(%c)%s",
                               i + 1, c->name, c->symbol,
                               (i + 1 < player_count) ? "，" : "");
            if (written > 0) {
                used += (size_t)written;
            }
        }
    }
    (void)snprintf(buf + used, sizeof(buf) - used,
                   "\n请输入每位玩家的初始资金：\n");
    write_message(message, size, buf);
}

StartupResult application_start(
    Game *game,
    int argument_count,
    char *const arguments[],
    char *message,
    size_t message_size
) {
    if (game == 0) {
        write_message(message, message_size, "启动失败：游戏状态对象无效。\n");
        return STARTUP_INTERNAL_ERROR;
    }
    if (game->phase != GAME_NOT_STARTED) {
        write_message(message, message_size, "启动失败：当前游戏实例已经启动。\n");
        return STARTUP_ALREADY_STARTED;
    }
    if (argument_count != 1 || arguments == 0 || arguments[0] == 0 || arguments[0][0] == '\0') {
        write_message(message, message_size, "启动失败：本程序不接受启动参数，请直接运行 monopoly。\n");
        return STARTUP_INVALID_ARGUMENT;
    }

    /* 所有检查通过后才一次性改变游戏状态，避免失败时留下半初始化数据。 */
    if (!game_start(game)) {
        write_message(message, message_size, "启动失败：无法初始化游戏流程。\n");
        return STARTUP_INTERNAL_ERROR;
    }

    {
        char prompt[512];
        char buf[640];
        write_selection_prompt(prompt, sizeof(prompt));
        (void)snprintf(buf, sizeof(buf),
                       "大富翁启动成功。\n开局引导顺序：角色组队 -> 初始资金。\n%s",
                       prompt);
        write_message(message, message_size, buf);
    }
    return STARTUP_OK;
}

CommandResult startup_handle_input(
    Game *game,
    const char *input,
    char *message,
    size_t message_size
)
{
    if (game == 0 || input == 0 || message == 0 || message_size == 0) {
        return COMMAND_INVALID;
    }

    switch (game->setup_step) {
        case SETUP_PLAYER_COUNT: {
            /* 组合输入：一行同时确定玩家人数与每名玩家的角色 */
            int count = 0;
            int chosen[4] = {0};
            if (!parse_selection_line(input, &count, chosen,
                                      message, message_size)) {
                return COMMAND_INVALID;
            }
            game->setup_player_count = count;
            game->setup_choosing = count;
            {
                int i;
                for (i = 0; i < count; ++i) {
                    game->setup_chosen[i] = chosen[i];
                }
            }
            game->setup_step = SETUP_INITIAL_MONEY;
            write_assignment_echo(message, message_size,
                                  game->setup_chosen, count);
            return COMMAND_OK;
        }
        case SETUP_INITIAL_MONEY: {
            long parsed = 0;
            if (!parse_strict_int(input, &parsed) || parsed < 1000 || parsed > 50000) {
                write_message(message, message_size,
                              "初始资金必须在 1000~50000 之间，请重新输入：\n");
                return COMMAND_INVALID;
            }
            game->setup_initial_money = (int)parsed;
            game->runtime = runtime_create(game->setup_player_count,
                                           game->setup_initial_money,
                                           game->setup_chosen);
            if (game->runtime == 0) {
                write_message(message, message_size,
                              "初始化游戏失败。\n");
                return COMMAND_INVALID;
            }
            game->setup_step = SETUP_COMPLETE;
            (void)runtime_begin(game->runtime, message, message_size);
            return COMMAND_OK;
        }
        case SETUP_ROLE_SELECTION:
        case SETUP_COMPLETE:
        default:
            write_message(message, message_size, "游戏已经初始化完成。\n");
            return COMMAND_OK;
    }
}

