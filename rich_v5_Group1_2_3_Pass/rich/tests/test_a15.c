#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/gift.h"
#include "monopoly/runtime.h"
#include "monopoly/automation.h"

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

static void test_points_and_fortune_gift(void)
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
    CHECK(runtime_player_god_rounds(runtime, 0) == 0,
          "点数卡不应附带财神效果");
    runtime_destroy(runtime);

    runtime = runtime_create(2, 1000, test_roles);
    CHECK(runtime_begin(runtime, message, sizeof(message)) == 0, "应开始新运行时");
    CHECK(runtime_step(runtime, 35, message, sizeof(message)) == 0,
          "应再次到达礼品屋");
    CHECK(strstr(message, "3 财神") != NULL,
          "礼品屋菜单应恢复第3项财神");
    CHECK(runtime_answer(runtime, " 3 ", message, sizeof(message)) == 0,
          "礼品屋应接受财神选择及首尾空白");
    CHECK(runtime_player_god_rounds(runtime, 0) == 5 &&
          runtime_player_god_rounds(runtime, 1) == 0,
          "领取当回合不扣次数，且只给领取者发放5回合财神");
    CHECK(runtime_player_money(runtime, 0) == 1000 &&
          runtime_player_points(runtime, 0) == 0,
          "选择财神不得额外增加资金或点数");
    CHECK(runtime_context(runtime) == RUNTIME_CONTEXT_TURN_START &&
          strstr(message, "轮到玩家 阿土伯") != NULL,
          "领取财神后应完成落地事件并提示下一玩家");
    runtime_destroy(runtime);
}

static void test_gift_fortune_exempts_rent(void)
{
    AutomationPreset preset = {0};
    Game game;
    char message[4096];
    preset.player_count = 2;
    preset.players[0].symbol = 'Q';
    preset.players[0].position = 34;
    preset.players[0].fund = 1000;
    preset.players[1].symbol = 'A';
    preset.players[1].fund = 1000;
    preset.property_count = 1;
    preset.properties[0].position = 36;
    preset.properties[0].owner_index = 1;
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_load_preset(&preset);
    CHECK(game.runtime != NULL, "应创建财神免租测试运行时");
    if (game.runtime == NULL) return;
    CHECK(command_execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          game.context == CONTEXT_GIFT_HOUSE, "应进入礼品屋");
    CHECK(command_execute(&game, "Help", message, sizeof(message)) == COMMAND_OK &&
          command_execute(&game, "3", message, sizeof(message)) == COMMAND_OK,
          "Help后仍应能从正式命令入口领取财神");
    CHECK(command_execute(&game, "Step 70", message, sizeof(message)) == COMMAND_OK,
          "另一玩家应完成一次行动");
    CHECK(command_execute(&game, "Query", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "财神：5 回合") != NULL,
          "其他玩家行动不应扣除领取者的财神次数");
    CHECK(command_execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "财神附身，可免过路费") != NULL,
          "礼品屋领取的财神应正确免租");
    CHECK(runtime_player_money(game.runtime, 0) == 1000 &&
          runtime_player_money(game.runtime, 1) == 1000 &&
          runtime_player_god_rounds(game.runtime, 0) == 4,
          "免租后双方资金不变，领取者完成一次行动后仅扣一次效果");
    runtime_destroy(game.runtime);
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

    CHECK(gift_shop_begin(&gift, 0) == GIFT_OK &&
          gift_shop_answer(&gift, money, 2, "3") == GIFT_FORTUNE_SELECTED,
          "第3项应通知运行时发放财神");
    CHECK(!gift.is_open && money[0] == 1000 && gift_shop_points(&gift, 0) == 0,
          "选择财神应关闭场景但不改变钱与点数");

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
    test_points_and_fortune_gift();
    test_gift_fortune_exempts_rent();
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
