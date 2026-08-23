#include "monopoly/command.h"
#include "monopoly/runtime.h"

#include <stdio.h>

CommandResult query_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
) {
    if (game == NULL || game->runtime == NULL ||
        message == NULL || message_size == 0U) {
        return COMMAND_NOT_ALLOWED;
    }
    if (arguments != NULL && arguments[0] != '\0') {
        (void)snprintf(message, message_size, "Query 命令不接受参数。\n");
        return COMMAND_INVALID;
    }
    if (runtime_query(game->runtime, message, message_size) != 0) {
        (void)snprintf(message, message_size, "Query 执行失败。\n");
        return COMMAND_INVALID;
    }
    return COMMAND_OK;
}
