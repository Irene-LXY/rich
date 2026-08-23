#include "monopoly/command.h"

#include <stdio.h>

static void write_message(char *message, size_t size, const char *text) {
    if (message != 0 && size > 0) {
        (void)snprintf(message, size, "%s", text);
    }
}

CommandResult quit_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
) {
    if (game == 0) {
        write_message(message, message_size, u8"游戏状态无效。\n");
        return COMMAND_INVALID;
    }
    if (game->phase == GAME_NOT_STARTED) {
        write_message(message, message_size, u8"游戏尚未开始，不能执行 Quit。\n");
        return COMMAND_NOT_ALLOWED;
    }
    if (game->phase == GAME_ENDED) {
        write_message(message, message_size, u8"游戏已经结束，命令不再生效。\n");
        return COMMAND_GAME_ENDED;
    }
    if (arguments != 0 && arguments[0] != '\0') {
        write_message(message, message_size, u8"Quit 命令不接受参数。\n");
        return COMMAND_INVALID;
    }

    if (!game_end(game, END_REASON_USER_QUIT)) {
        write_message(message, message_size, u8"游戏结束失败。\n");
        return COMMAND_INVALID;
    }
    write_message(message, message_size, u8"玩家选择强制退出，整局游戏已结束。\n");
    return COMMAND_OK;
}
