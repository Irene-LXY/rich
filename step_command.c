#include "monopoly/command.h"
#include "monopoly/runtime.h"
#include "monopoly/step.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

StepParseResult step_parse_argument(const char *arguments, int *steps) {
    const char *cursor;
    char *end;
    long value;
    if (arguments == NULL || steps == NULL) {
        return STEP_PARSE_MISSING;
    }
    cursor = arguments;
    while (isspace((unsigned char)*cursor)) cursor++;
    if (*cursor == '\0') return STEP_PARSE_MISSING;

    errno = 0;
    value = strtol(cursor, &end, 10);
    if (cursor == end) return STEP_PARSE_NOT_INTEGER;
    if (errno == ERANGE || value <= 0L || value > INT_MAX) {
        return STEP_PARSE_OUT_OF_RANGE;
    }
    while (isspace((unsigned char)*end)) end++;
    if (*end != '\0') return STEP_PARSE_EXTRA_ARGUMENT;
    *steps = (int)value;
    return STEP_PARSE_OK;
}

const char *step_parse_result_message(StepParseResult result) {
    switch (result) {
        case STEP_PARSE_OK:             return "成功";
        case STEP_PARSE_MISSING:        return "缺少步数";
        case STEP_PARSE_NOT_INTEGER:    return "步数必须是整数";
        case STEP_PARSE_OUT_OF_RANGE:   return "步数必须是正整数";
        case STEP_PARSE_EXTRA_ARGUMENT: return "存在多余参数";
        default:                        return "参数无效";
    }
}

CommandResult step_command_execute(
    Game *game,
    const char *arguments,
    char *message,
    size_t message_size
) {
    int steps;
    StepParseResult parse_result;
    if (game == NULL || game->runtime == NULL ||
        message == NULL || message_size == 0U) {
        return COMMAND_NOT_ALLOWED;
    }
    parse_result = step_parse_argument(arguments, &steps);
    if (parse_result != STEP_PARSE_OK) {
        (void)snprintf(message, message_size,
                       "Step 命令参数错误：%s。用法：Step n（例如 Step 75）。\n",
                       step_parse_result_message(parse_result));
        return COMMAND_INVALID;
    }
    if (runtime_step(game->runtime, steps, message, message_size) != 0) {
        return COMMAND_NOT_ALLOWED;
    }
    return COMMAND_OK;
}
