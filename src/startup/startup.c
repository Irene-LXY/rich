#include "monopoly/startup.h"
#include "monopoly/runtime.h"
#include "monopoly/character.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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

/* 列出剩余可选角色，并提示下一位玩家输入编号。 */
static void write_available_roles(char *message, size_t size,
                                  const int *chosen, int chosen_count,
                                  int choosing_player)
{
    const Character *table = character_table();
    char buf[1024];
    size_t used = 0;
    int i, j;
    int written;

    written = snprintf(buf, sizeof(buf), "可选角色：");
    if (written > 0) {
        used = (size_t)written;
    }
    for (i = 0; i < CHARACTER_COUNT; ++i) {
        int taken = 0;
        for (j = 0; j < chosen_count; ++j) {
            if (chosen[j] == (int)table[i].id) {
                taken = 1;
                break;
            }
        }
        if (!taken && used < sizeof(buf)) {
            written = snprintf(buf + used, sizeof(buf) - used,
                               " %d.%s(%c)", (int)table[i].id,
                               table[i].name, table[i].symbol);
            if (written > 0) {
                used += (size_t)written;
            }
        }
    }
    (void)snprintf(buf + used, sizeof(buf) - used,
                   "\n请输入玩家 %d 的角色编号：\n", choosing_player);
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

    write_message(
        message,
        message_size,
        "大富翁启动成功。\n开局引导顺序：玩家人数 -> 初始资金 -> 角色选择。\n请输入玩家人数（2-4）：\n"
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
            long parsed = 0;
            if (!parse_strict_int(input, &parsed) || parsed < 2 || parsed > 4) {
                write_message(message, message_size,
                              "玩家数量必须为 2-4，请重新输入：\n");
                return COMMAND_INVALID;
            }
            game->setup_player_count = (int)parsed;
            game->setup_step = SETUP_INITIAL_MONEY;
            write_message(message, message_size,
                          "请输入每位玩家的初始资金：\n");
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
            game->setup_step = SETUP_ROLE_SELECTION;
            game->setup_choosing = 0;
            write_available_roles(message, message_size,
                                  game->setup_chosen, 0, 1);
            return COMMAND_OK;
        }
        case SETUP_ROLE_SELECTION: {
            long parsed = 0;
            int id;
            int i;
            if (!parse_strict_int(input, &parsed) ||
                parsed < CHARACTER_MIN_ID || parsed > CHARACTER_MAX_ID) {
                write_message(message, message_size,
                              "角色编号必须为 1-4，请重新选择。\n");
                return COMMAND_INVALID;
            }
            id = (int)parsed;
            for (i = 0; i < game->setup_choosing; ++i) {
                if (game->setup_chosen[i] == id) {
                    write_message(message, message_size,
                                  "该角色已被选择，请重新选择。\n");
                    return COMMAND_INVALID;
                }
            }
            game->setup_chosen[game->setup_choosing++] = id;
            if (game->setup_choosing < game->setup_player_count) {
                write_available_roles(message, message_size,
                                      game->setup_chosen,
                                      game->setup_choosing,
                                      game->setup_choosing + 1);
            } else {
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
            }
            return COMMAND_OK;
        }
        case SETUP_COMPLETE:
        default:
            write_message(message, message_size, "游戏已经初始化完成。\n");
            return COMMAND_OK;
    }
}

