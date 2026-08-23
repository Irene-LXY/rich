#include "monopoly/command.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    if (equals_ignore_case(text, "step")) {
        char *end;
        long steps = strtol(arguments, &end, 10);
        if (end == arguments || *end != '\0' || steps < 0 || steps > 1000) {
            write_message(message, message_size, "用法：Step n，n 为 0-1000 的测试移动步数。\n");
            return COMMAND_INVALID;
        }
        return runtime_step(game->runtime, (int)steps, message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "query")) {
        (void)runtime_query(game->runtime, message, message_size);
        return COMMAND_OK;
    }
    if (equals_ignore_case(text, "map")) {
        (void)runtime_render(game->runtime, message, message_size);
        return COMMAND_OK;
    }
    if (equals_ignore_case(text, "help")) {
        (void)runtime_help(game->runtime, message, message_size);
        return COMMAND_OK;
    }
    if (text[0] >= '1' && text[0] <= '3' && text[1] == '\0') {
        int choice = text[0] - '0';
        if (runtime_select_gift(game->runtime, choice, message, message_size) == 0) {
            return COMMAND_OK;
        }
        return runtime_select_shop_item(game->runtime, choice, message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "shop")) {
        return runtime_tool_shop(game->runtime, message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "buy")) {
        char *end;
        long tool = strtol(arguments, &end, 10);
        if (end == arguments || *end != '\0' || tool < 1 || tool > 3) {
            write_message(message, message_size, "用法：Buy 1（路障）、Buy 2（机器娃娃）、Buy 3（炸弹）。\n");
            return COMMAND_INVALID;
        }
        return runtime_buy_tool(game->runtime, (int)tool, message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "sell")) {
        char *end;
        long position = strtol(arguments, &end, 10);
        if (end == arguments || *end != '\0' || position < 0 || position >= 70) {
            write_message(message, message_size, "用法：Sell n，n 为 0-69 的房产编号。\n");
            return COMMAND_INVALID;
        }
        return runtime_sell_property(game->runtime, (int)position,
                                     message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "y") || equals_ignore_case(text, "n")) {
        if (arguments[0] != '\0') {
            write_message(message, message_size, "Y/N 命令不接受参数。\n");
            return COMMAND_INVALID;
        }
        return runtime_resolve_landing(game->runtime,
            equals_ignore_case(text, "y"), message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "block") || equals_ignore_case(text, "bomb")) {
        char *end;
        long distance = strtol(arguments, &end, 10);
        int tool = equals_ignore_case(text, "block") ? 1 : 3;
        if (end == arguments || *end != '\0' || distance < -10 || distance > 10) {
            write_message(message, message_size, "用法：Block n / Bomb n，n 范围为 -10 至 10。\n");
            return COMMAND_INVALID;
        }
        return runtime_place_tool(game->runtime, tool, (int)distance,
                                  message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }
    if (equals_ignore_case(text, "robot")) {
        if (arguments[0] != '\0') {
            write_message(message, message_size, "Robot 不接受参数。\n");
            return COMMAND_INVALID;
        }
        return runtime_use_robot(game->runtime, message, message_size) == 0
            ? COMMAND_OK : COMMAND_NOT_ALLOWED;
    }

    write_message(message, message_size, "无效或尚未实现的命令。\n");
    return COMMAND_INVALID;
}
