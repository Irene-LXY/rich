#ifndef RICH_GAME_ENGINE_H
#define RICH_GAME_ENGINE_H

#include "cJSON.h"

/*
 * 游戏引擎统一入口。
 *
 * 输入：
 *   preset  : 测试用例里的 "preset" 对象（不能为 NULL）
 *   actions : 测试用例里的 "actions" 数组（不能为 NULL）
 *
 * 输出：
 *   成功时返回一个由调用方负责 cJSON_Delete 的 actual 对象。
 *   失败时返回 NULL，并通过 error_code / error_message 说明原因。
 *
 * 注意：
 *   测试执行器只负责读 JSON、调用本函数、比较 actual 与 expected。
 *   这里面的“如何重置状态、如何逐格移动、如何处理落点”全部由游戏逻辑实现。
 */
cJSON *game_engine_execute(
    const cJSON *preset,
    const cJSON *actions,
    char **error_code,
    char **error_message);

#endif
