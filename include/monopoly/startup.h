#ifndef MONOPOLY_STARTUP_H
#define MONOPOLY_STARTUP_H

#include <stddef.h>
#include "monopoly/command.h"
#include "monopoly/game.h"

typedef enum {
    STARTUP_OK = 0,
    STARTUP_INVALID_ARGUMENT,
    STARTUP_ALREADY_STARTED,
    STARTUP_INTERNAL_ERROR
} StartupResult;

StartupResult application_start(
    Game *game,
    int argument_count,
    char *const arguments[],
    char *message,
    size_t message_size
);

/* 引导阶段的输入处理：玩家人数 -> 初始资金 -> 自动分配角色并创建运行时。
 * 返回 COMMAND_OK / COMMAND_INVALID。 */
CommandResult startup_handle_input(
    Game *game,
    const char *input,
    char *message,
    size_t message_size
);

#endif

