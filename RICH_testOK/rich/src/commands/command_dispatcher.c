#include "monopoly/command.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_BUFFER_SIZE 256

static void write_message(char *message, size_t size, const char *text) {
    if (message != 0 && size > 0) {
        (void)snprintf(message, size, "%s", text);
    }
}

static char *trim(char *text) {
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static bool equals_ignore_case(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

CommandResult command_execute(
    Game *game,
    const char *input,
    char *message,
    size_t message_size
) {
    char buffer[COMMAND_BUFFER_SIZE];
    char *text;
    char *arguments;
    char *separator;

    if (game == 0 || input == 0 || strlen(input) >= sizeof(buffer)) {
        write_message(message, message_size, "命令无效。\n");
        return COMMAND_INVALID;
    }
    if (game->phase == GAME_ENDED) {
        write_message(message, message_size, "游戏已经结束，后续命令全部无效。\n");
        return COMMAND_GAME_ENDED;
    }

    (void)memcpy(buffer, input, strlen(input) + 1);
    text = trim(buffer);
    if (*text == '\0') {
        write_message(message, message_size, "命令不能为空。\n");
        return COMMAND_INVALID;
    }

    separator = text;
    while (*separator != '\0' && !isspace((unsigned char)*separator)) {
        separator++;
    }
    if (*separator == '\0') {
        arguments = separator;
    } else {
        *separator = '\0';
        arguments = trim(separator + 1);
    }

    /* 引导阶段：运行时尚未创建，交给开局引导处理。 */
    if (game->runtime == 0) {
        if (equals_ignore_case(text, "quit")) {
            return quit_command_execute(game, arguments, message, message_size);
        }
        return startup_handle_input(game, text, message, message_size);
    }

    if (equals_ignore_case(text, "quit")) {
        return quit_command_execute(game, arguments, message, message_size);
    }
    if (equals_ignore_case(text, "roll")) {
        if (arguments[0] != '\0') {
            write_message(message, message_size, "Roll 命令不接受参数。\n");
            return COMMAND_INVALID;
        }
        (void)runtime_roll(game->runtime, message, message_size);
        return COMMAND_OK;
    }
    if (equals_ignore_case(text, "query")) {
        (void)runtime_query(game->runtime, message, message_size);
        return COMMAND_OK;
    }
    if (equals_ignore_case(text, "map")) {
        write_message(message, message_size, "地图已实时显示在上方，无需手动查看。\n");
        return COMMAND_OK;
    }
    if (equals_ignore_case(text, "help")) {
        (void)runtime_help(game->runtime, message, message_size);
        return COMMAND_OK;
    }

    write_message(message, message_size, "无效或尚未实现的命令。\n");
    return COMMAND_INVALID;
}

