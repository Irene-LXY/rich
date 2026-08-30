#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/gift.h"
#include "monopoly/runtime.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;
static int tests_run;
static const int test_roles[2] = {1, 2};

#define CHECK(condition, description) do { \
    ++tests_run; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "[FAIL] A15: %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static Game make_running_game(void)
{
    Game game;
    char message[256];
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_create(2, 1000, test_roles);
    if (game.runtime != NULL) {
        (void)runtime_begin(game.runtime, message, sizeof(message));
    }
    return game;
}

static void test_gift_money_through_dispatcher(void)
{
    Game game = make_running_game();
    char message[1024];
    CHECK(game.runtime != NULL, "应创建主工程 GameRuntime");
    CHECK(command_execute(&game, "Step 35", message, sizeof(message)) == COMMAND_OK,
          "Step 35 应准确到达礼品屋");
    CHECK(game.context == CONTEXT_GIFT_HOUSE,
          "到达第 35 格后 Game.context 应进入礼品屋");
    CHECK(command_execute(&game, "1", message, sizeof(message)) == COMMAND_OK,
          "输入 1 应领取奖金");
    CHECK(runtime_player_money(game.runtime, 0) == 3000,
          "奖金应令玩家资金增加 2000");
    CHECK(game.context == CONTEXT_TURN_START,
          "领取礼品后应结束落地并切换回合");
    CHECK(strcmp(runtime_current_player_name(game.runtime), "阿土伯") == 0,
          "A4 应在礼品选择完成后切换到下一位玩家");
    runtime_destroy(game.runtime);
}

static void test_points_and_god_of_wealth(void)
{
    GameRuntime *runtime = runtime_create(2, 1000, test_roles);
    char message[1024];
    CHECK(runtime != NULL, "应创建运行时");
    CHECK(runtime_begin(runtime, message, sizeof(message)) == 0, "应开始回合");
    CHECK(runtime_step(runtime, 35, message, sizeof(message)) == 0,
          "应到达礼品屋");
    CHECK(runtime_answer(runtime, "2", message, sizeof(message)) == 0,
          "应领取点数卡");
    CHECK(runtime_player_points(runtime, 0) == 200,
          "点数卡应增加 200 点");
    runtime_destroy(runtime);

    runtime = runtime_create(2, 1000, test_roles);
    CHECK(runtime_begin(runtime, message, sizeof(message)) == 0, "应开始新运行时");
    CHECK(runtime_step(runtime, 35, message, sizeof(message)) == 0,
          "应再次到达礼品屋");
    CHECK(runtime_answer(runtime, "3", message, sizeof(message)) == 0,
          "应领取财神");
    CHECK(runtime_player_god_rounds(runtime, 0) == 5,
          "获得财神的当前回合不应消耗第一轮");
    runtime_destroy(runtime);
}

static void test_help_preserves_gift_choice(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(command_execute(&game, "Step 35", message, sizeof(message)) == COMMAND_OK,
          "应到达礼品屋");
    CHECK(command_execute(&game, "hElP", message, sizeof(message)) == COMMAND_OK,
          "礼品屋内应允许大小写混合的Help");
    CHECK(game.context == CONTEXT_GIFT_HOUSE &&
          strstr(message, "可用命令") != NULL,
          "Help后必须保留礼品选择上下文");
    CHECK(command_execute(&game, "2", message, sizeof(message)) == COMMAND_OK &&
          runtime_player_points(game.runtime, 0) == 200,
          "Help后仍应能正常领取礼品");
    runtime_destroy(game.runtime);
}

static void test_invalid_choice_and_overflow(void)
{
    GiftShopState gift;
    int money[2] = {1000, 1000};
    gift_shop_init(&gift, 2);
    CHECK(gift_shop_begin(&gift, 0) == GIFT_OK, "礼品屋应打开");
    CHECK(gift_shop_answer(&gift, money, 2, "abc") == GIFT_INVALID_CHOICE,
          "错误输入应放弃礼品");
    CHECK(!gift.is_open && money[0] == 1000,
          "错误输入后应关闭且不改变资产");

    money[0] = INT_MAX;
    CHECK(gift_shop_begin(&gift, 0) == GIFT_OK, "应可再次打开礼品屋");
    CHECK(gift_shop_answer(&gift, money, 2, "1") == GIFT_ERR_OVERFLOW,
          "资金溢出时应拒绝礼品");
    CHECK(gift.is_open && money[0] == INT_MAX,
          "溢出拒绝不得破坏资金或关闭场景");
}

int main(void)
{
    test_gift_money_through_dispatcher();
    test_points_and_god_of_wealth();
    test_help_preserves_gift_choice();
    test_invalid_choice_and_overflow();
    if (failures == 0) {
        printf("[PASS] A15: %d assertions passed.\n", tests_run);
        return 0;
    }
    fprintf(stderr, "[FAIL] A15: %d/%d assertions failed.\n",
            failures, tests_run);
    return 1;
}
