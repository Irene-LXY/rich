#ifndef MONOPOLY_STARTUP_H
#define MONOPOLY_STARTUP_H

#include <stddef.h>
#include "monopoly/game.h"
#include "monopoly/command.h"

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

/* 与 rich-main-v1_1 主干一致的开局引导入口。 */
CommandResult startup_handle_input(
    Game *game,
    const char *input,
    char *message,
    size_t message_size
);

#endif
