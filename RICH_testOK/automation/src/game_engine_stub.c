#include "game_engine.h"

/*
 * 这是 TDD 红阶段使用的临时实现。
 * 它不会真正执行游戏，只是让测试脚本能够先编译、先运行、先报错。
 *
 * 当开发人员完成游戏逻辑后，请用真正的 game_engine.c 替换本文件，
 * 或者删除本文件并在编译命令中加入真正的实现文件。
 */
cJSON *game_engine_execute(
    const cJSON *preset,
    const cJSON *actions,
    char **error_code,
    char **error_message)
{
    static char code[] = "NOT_IMPLEMENTED";
    static char message[] = "Game engine is not implemented yet (TDD red phase)";

    (void)preset;
    (void)actions;

    if (error_code != NULL) {
        *error_code = code;
    }
    if (error_message != NULL) {
        *error_message = message;
    }

    return NULL;
}
