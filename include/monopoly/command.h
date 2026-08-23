#ifndef MONOPOLY_COMMAND_H
#define MONOPOLY_COMMAND_H

#include <stddef.h>
#include "monopoly/game.h"

typedef enum {
    COMMAND_OK = 0,
    COMMAND_INVALID,
    COMMAND_NOT_ALLOWED,
    COMMAND_GAME_ENDED
} CommandResult;

CommandResult command_execute(
    Game *game,
    const char *input,
    char *message,
    size_t message_size
);

CommandResult quit_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
);

CommandResult query_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
);

CommandResult step_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
);

#endif
