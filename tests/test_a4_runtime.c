#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"

#include <stdio.h>
#include <string.h>

static int assertions;
static int failures;
static const int roles[2] = {1, 2};

#define CHECK(condition, case_id, description) do { \
    ++assertions; \
    if (!(condition)) { \
        ++failures; \
        (void)fprintf(stderr, "[FAIL] %s: %s (line %d)\n", \
                      case_id, description, __LINE__); \
    } \
} while (0)

static Game make_running_game(void)
{
    Game game;
    char message[256];
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_create(2, 1000, roles);
    if (game.runtime != NULL) {
        (void)runtime_begin(game.runtime, message, sizeof(message));
    }
    return game;
}

static CommandResult execute(Game *game, const char *command,
                             char *message, size_t message_size)
{
    return command_execute(game, command, message, message_size);
}

static int reach_tool_shop_and_buy(Game *game, const char *choice,
                                   char *message, size_t message_size)
{
    if (execute(game, "Step 64", message, message_size) != COMMAND_OK ||
        execute(game, "Step 1", message, message_size) != COMMAND_OK ||
        execute(game, "N", message, message_size) != COMMAND_OK ||
        execute(game, "Step 34", message, message_size) != COMMAND_OK ||
        execute(game, choice, message, message_size) != COMMAND_OK ||
        execute(game, "Step 1", message, message_size) != COMMAND_OK ||
        execute(game, "N", message, message_size) != COMMAND_OK) {
        return 0;
    }
    /* N 后保留一次回合末尾命令窗口；Query 完成过渡且不改变资产。 */
    return execute(game, "Query", message, message_size) == COMMAND_OK;
}

static void test_bomb_skip_reason(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(game.runtime != NULL, "Case_A4_001", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    CHECK(reach_tool_shop_and_buy(&game, "3", message, sizeof(message)),
          "Case_A4_001", "应先获得并购买炸弹");
    CHECK(execute(&game, "Bomb 1", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_001", "应成功放置炸弹");
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_001", "应触发炸弹并住院");
    CHECK(runtime_player_position(game.runtime, 0) == 14,
          "Case_A4_001", "炸弹应把玩家送到医院");

    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_001", "下一玩家应完成移动");
    CHECK(execute(&game, "N", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_001", "下一玩家应完成落点事件");
    CHECK(runtime_post_roll_transition_pending(game.runtime),
          "Case_A4_001", "应进入回合末尾窗口");
    CHECK(execute(&game, "Help", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_001", "帮助命令应完成回合切换并触发住院跳过");
    CHECK(strstr(message, "因炸弹住院跳过本回合") != NULL,
          "Case_A4_001", "跳过提示应保留炸弹住院原因");
    CHECK(strstr(message, "轮到玩家 阿土伯") != NULL,
          "Case_A4_001", "住院玩家跳过后应切回下一名可行动玩家");
    (void)execute(&game, "Quit", message, sizeof(message));
    runtime_destroy(game.runtime);
}

static void test_block_stops_before_cell(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(game.runtime != NULL, "Case_A4_010", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    CHECK(reach_tool_shop_and_buy(&game, "1", message, sizeof(message)),
          "Case_A4_010", "应先获得并购买路障");
    CHECK(execute(&game, "Block 3", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_010", "应在31号位置放置路障");
    CHECK(execute(&game, "Step 6", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_010", "移动应被路障中断");
    CHECK(runtime_player_position(game.runtime, 0) == 30,
          "Case_A4_010", "玩家应停在31号路障前的30号位置");
    CHECK(strstr(message, "停在 30 号位置") != NULL,
          "Case_A4_010", "提示应显示路障前的停止位置");
    CHECK(strstr(message, "到达无主空地 31") == NULL,
          "Case_A4_010", "不得继续处理路障所在格的落点事件");
    if (game.context == CONTEXT_BUY_CONFIRM) {
        (void)execute(&game, "N", message, sizeof(message));
    }
    (void)execute(&game, "Quit", message, sizeof(message));
    runtime_destroy(game.runtime);
}

static void prepare_declined_purchase(Game *game, char *message,
                                      size_t message_size)
{
    CHECK(execute(game, "Step 1", message, message_size) == COMMAND_OK,
          "Case_A4_023", "应进入购买确认");
    CHECK(execute(game, "N", message, message_size) == COMMAND_OK,
          "Case_A4_023", "应完成不购买处理");
    CHECK(runtime_post_roll_transition_pending(game->runtime),
          "Case_A4_023", "应保留回合末尾命令窗口");
    CHECK(strstr(message, "轮到玩家 阿土伯") == NULL,
          "Case_A4_025", "不购买后不应提前显示下一玩家提示");
}

static void test_post_roll_sell_rejected(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(game.runtime != NULL, "Case_A4_023", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    prepare_declined_purchase(&game, message, sizeof(message));
    CHECK(execute(&game, "Sell 1", message, sizeof(message)) ==
              COMMAND_NOT_ALLOWED,
          "Case_A4_023", "掷骰后应拒绝卖房");
    CHECK(strstr(message, "掷骰后只能处理当前格事件") != NULL,
          "Case_A4_023", "应给出掷骰后操作限制原因");
    CHECK(strstr(message, "轮到玩家 阿土伯") != NULL,
          "Case_A4_023", "拒绝操作后应正式进入下一玩家回合");
    runtime_destroy(game.runtime);
}

static void test_post_roll_help_then_advance(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(game.runtime != NULL, "Case_A4_024", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    prepare_declined_purchase(&game, message, sizeof(message));
    CHECK(execute(&game, "Help", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_024", "掷骰后应允许查看帮助");
    CHECK(strstr(message, "可用命令") != NULL,
          "Case_A4_024", "应显示完整帮助");
    CHECK(strstr(message, "轮到玩家 阿土伯") != NULL,
          "Case_A4_024", "帮助显示后应正式进入下一玩家回合");
    CHECK(!runtime_post_roll_transition_pending(game.runtime),
          "Case_A4_024", "帮助后应完成回合末尾过渡");
    runtime_destroy(game.runtime);
}

static void test_post_roll_quit_without_next_prompt(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(game.runtime != NULL, "Case_A4_025", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    prepare_declined_purchase(&game, message, sizeof(message));
    CHECK(execute(&game, "Quit", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_025", "应允许立即退出");
    CHECK(game.phase == GAME_ENDED,
          "Case_A4_025", "Quit 应立即结束整局游戏");
    CHECK(strstr(message, "轮到玩家") == NULL,
          "Case_A4_025", "退出消息不得包含下一玩家输入提示");
    runtime_destroy(game.runtime);
}

int main(void)
{
    test_bomb_skip_reason();
    test_block_stops_before_cell();
    test_post_roll_sell_rejected();
    test_post_roll_help_then_advance();
    test_post_roll_quit_without_next_prompt();
    if (failures == 0) {
        (void)printf("[PASS] A4 workbook regressions: %d assertions passed.\n",
                     assertions);
        return 0;
    }
    (void)fprintf(stderr, "[FAIL] A4 workbook regressions: %d/%d failed.\n",
                  failures, assertions);
    return 1;
}
