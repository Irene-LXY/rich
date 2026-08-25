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

static int is_confirmation(const char *input) {
    return strcmp(input, "确认") == 0 || strcmp(input, "Y") == 0 ||
           strcmp(input, "y") == 0 || strcmp(input, "Yes") == 0 ||
           strcmp(input, "yes") == 0;
}

static int is_cancellation(const char *input) {
    return strcmp(input, "取消") == 0 || strcmp(input, "N") == 0 ||
           strcmp(input, "n") == 0 || strcmp(input, "No") == 0 ||
           strcmp(input, "no") == 0;
}

/* PDF 规定角色可以一次输入，例如“12”。 */
static int parse_combined_roles(const char *input, int *roles, int *count) {
    size_t length;
    size_t i;
    int seen[CHARACTER_MAX_ID + 1] = {0};

    if (input == 0 || roles == 0 || count == 0) {
        return 0;
    }
    length = strlen(input);
    if (length < 2U || length > 4U) {
        return 0;
    }
    for (i = 0; i < length; ++i) {
        int id;
        if (!isdigit((unsigned char)input[i])) {
            return 0;
        }
        id = input[i] - '0';
        if (id < CHARACTER_MIN_ID || id > CHARACTER_MAX_ID || seen[id]) {
            return 0;
        }
        seen[id] = 1;
        roles[i] = id;
    }
    *count = (int)length;
    return 1;
}

static CommandResult create_runtime_and_begin(Game *game,
                                               char *message,
                                               size_t message_size) {
    game->runtime = runtime_create(game->setup_player_count,
                                   game->setup_initial_money,
                                   game->setup_chosen);
    if (game->runtime == 0) {
        write_message(message, message_size, "初始化游戏失败。\n");
        return COMMAND_INVALID;
    }
    game->setup_step = SETUP_COMPLETE;
    (void)runtime_begin(game->runtime, message, message_size);
    return COMMAND_OK;
}

/* 按 PDF 一次显示全部角色，并接收一个保持顺序的编号串。 */
static void write_combined_role_prompt(char *message, size_t size)
{
    const Character *table = character_table();
    char buf[1024];
    size_t used = 0;
    int i;
    int written;

    written = snprintf(buf, sizeof(buf),
                       "请选择 2~4 位不重复玩家，一次输入角色编号：\n");
    if (written > 0) {
        used = (size_t)written;
    }
    for (i = 0; i < CHARACTER_COUNT; ++i) {
        if (used < sizeof(buf)) {
            written = snprintf(buf + used, sizeof(buf) - used,
                               " %d.%s(%c,%s)", (int)table[i].id,
                               table[i].name, table[i].symbol,
                               table[i].color_name);
            if (written > 0) {
                used += (size_t)written;
            }
        }
    }
    (void)snprintf(buf + used, sizeof(buf) - used,
                   "\n编号顺序就是玩家顺序：12 表示玩家1=1、玩家2=2；"
                   "324 表示玩家1=3、玩家2=2、玩家3=4。\n");
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
        write_message(message, message_size,
                      "启动失败：本程序不接受启动参数，请直接运行 rich.exe。\n");
        return STARTUP_INVALID_ARGUMENT;
    }

    /* 所有检查通过后才一次性改变游戏状态，避免失败时留下半初始化数据。 */
    if (!game_start(game)) {
        write_message(message, message_size, "启动失败：无法初始化游戏流程。\n");
        return STARTUP_INTERNAL_ERROR;
    }

    write_message(
        message,
        message_size,
        "大富翁启动成功。\n"
        "开局顺序：初始资金 -> 一次性选择角色。\n"
        "请输入每位玩家初始资金（1000~50000，直接回车使用默认 10000）：\n"
    );
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
            /* 兼容旧状态值，但按 PDF 统一转入资金设置。 */
            game->setup_step = SETUP_INITIAL_MONEY;
            return startup_handle_input(game, input, message, message_size);
        }
        case SETUP_INITIAL_MONEY: {
            long parsed = 0;

            if (game->setup_initial_money > 0 &&
                (input[0] == '\0' || is_confirmation(input))) {
                game->setup_step = SETUP_ROLE_SELECTION;
                write_combined_role_prompt(message, message_size);
                return COMMAND_OK;
            }
            if (is_cancellation(input)) {
                game->setup_initial_money = 0;
                write_message(message, message_size,
                              "已取消资金设置。请输入初始资金（1000~50000，直接回车默认 10000）：\n");
                return COMMAND_OK;
            }
            if (input[0] == '\0') {
                parsed = 10000;
            } else if (!parse_strict_int(input, &parsed)) {
                write_message(message, message_size,
                              "无效命令或资金输入。初始资金必须为整数 1000~50000，请重新输入：\n");
                return COMMAND_INVALID;
            }
            if (parsed < 1000 || parsed > 50000) {
                write_message(message, message_size,
                              "初始资金必须在 1000~50000 之间，请重新输入：\n");
                return COMMAND_INVALID;
            }
            game->setup_initial_money = (int)parsed;
            (void)snprintf(message, message_size,
                           "每位玩家初始资金为 %d 元。按回车或输入“确认”继续；输入“取消”重新设置：\n",
                           game->setup_initial_money);
            return COMMAND_OK;
        }
        case SETUP_ROLE_SELECTION: {
            int count = 0;
            if (!parse_combined_roles(input, game->setup_chosen, &count)) {
                write_combined_role_prompt(message, message_size);
                return COMMAND_INVALID;
            }
            game->setup_player_count = count;
            game->setup_choosing = count;
            return create_runtime_and_begin(game, message, message_size);
        }
        case SETUP_COMPLETE:
        default:
            write_message(message, message_size, "游戏已经初始化完成。\n");
            return COMMAND_OK;
    }
}

