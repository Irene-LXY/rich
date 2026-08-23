#include "monopoly/startup.h"
#include "monopoly/runtime.h"

#include <stdio.h>
#include <stdlib.h>

static void write_message(char *message, size_t size, const char *text) {
    if (message != 0 && size > 0) {
        (void)snprintf(message, size, "%s", text);
    }
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
            int count = (int)strtol(input, 0, 10);
            if (count < 2 || count > 4) {
                write_message(message, message_size,
                              "玩家数量必须为 2-4，请重新输入：\n");
                return COMMAND_INVALID;
            }
            game->setup_player_count = count;
            game->setup_step = SETUP_INITIAL_MONEY;
            write_message(message, message_size,
                          "请输入每位玩家的初始资金：\n");
            return COMMAND_OK;
        }
        case SETUP_INITIAL_MONEY: {
            int money = (int)strtol(input, 0, 10);
            if (money <= 0) {
                write_message(message, message_size,
                              "初始资金必须为正整数，请重新输入：\n");
                return COMMAND_INVALID;
            }
            game->setup_initial_money = money;
            game->runtime = runtime_create(game->setup_player_count, money);
            if (game->runtime == 0) {
                write_message(message, message_size, "初始化游戏失败。\n");
                return COMMAND_INVALID;
            }
            game->setup_step = SETUP_COMPLETE;
            (void)runtime_begin(game->runtime, message, message_size);
            return COMMAND_OK;
        }
        case SETUP_COMPLETE:
        case SETUP_ROLE_SELECTION:
        default:
            write_message(message, message_size, "游戏已经初始化完成。\n");
            return COMMAND_OK;
    }
}

