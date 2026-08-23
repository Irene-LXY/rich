#include "monopoly/command.h"

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

    (void)strcpy(buffer, input);
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

    if (equals_ignore_case(text, "quit")) {
        return quit_command_execute(game, arguments, message, message_size);
    }

    write_message(message, message_size, "无效或尚未实现的命令。\n");
    return COMMAND_INVALID;
}

