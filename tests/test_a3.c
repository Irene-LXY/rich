#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int assertions;

#define CHECK(condition, description) do { \
    ++assertions; \
    if (!(condition)) { \
        ++failures; \
        (void)fprintf(stderr, \
            "[FAIL] Case_A3_001: %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static void test_empty_money_input_uses_default(void)
{
    Game game;
    char message[1024];
    char program[] = "monopoly";
    char *arguments[] = {program, NULL};

    game_init(&game);
    CHECK(application_start(&game, 1, arguments, message, sizeof(message)) ==
              STARTUP_OK,
          "游戏应成功进入开局引导");
    CHECK(command_execute(&game, "12", message, sizeof(message)) == COMMAND_OK,
          "合法角色组合应进入初始资金步骤");
    CHECK(game.setup_step == SETUP_INITIAL_MONEY,
          "角色选择后应等待输入初始资金");

    CHECK(command_execute(&game, "", message, sizeof(message)) == COMMAND_OK,
          "初始资金步骤直接回车应被接受");
    CHECK(game.runtime != NULL, "直接回车后应创建游戏运行时");
    CHECK(game.setup_initial_money == 10000,
          "直接回车应采用默认初始资金 10000");
    if (game.runtime != NULL) {
        CHECK(runtime_player_money(game.runtime, 0U) == 10000,
              "玩家1初始资金应为 10000");
        CHECK(runtime_player_money(game.runtime, 1U) == 10000,
              "玩家2初始资金应为 10000");
    }
    CHECK(strstr(message, "命令不能为空") == NULL,
          "默认资金输入不得显示空命令错误");

    runtime_destroy(game.runtime);
}

int main(void)
{
    test_empty_money_input_uses_default();
    if (failures == 0) {
        (void)printf("[PASS] A3: %d assertions passed.\n", assertions);
        return 0;
    }
    (void)fprintf(stderr, "[FAIL] A3: %d/%d assertions failed.\n",
                  failures, assertions);
    return 1;
}
