#ifndef MONOPOLY_STEP_H
#define MONOPOLY_STEP_H

typedef enum StepParseResult {
    STEP_PARSE_OK = 0,
    STEP_PARSE_MISSING,
    STEP_PARSE_NOT_INTEGER,
    STEP_PARSE_OUT_OF_RANGE,
    STEP_PARSE_EXTRA_ARGUMENT
} StepParseResult;

/*
 * 只解析Step后的参数，不依赖地图或随机骰子。
 * 步数允许任意int范围内的正整数，不限制为1～6。
 */
StepParseResult step_parse_argument(const char *arguments, int *steps);
const char *step_parse_result_message(StepParseResult result);

#endif
