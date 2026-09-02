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

static void test_bomb_command_removed(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(game.runtime != NULL, "Case_New_Bomb", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    CHECK(execute(&game, "Bomb 1", message, sizeof(message)) == COMMAND_INVALID,
          "Case_New_Bomb", "Bomb 命令应被删除");
    CHECK(execute(&game, "Help", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "Bomb") == NULL && strstr(message, "炸弹") == NULL,
          "Case_New_Bomb", "帮助中不应再显示炸弹功能");
    runtime_destroy(game.runtime);
}

static void test_block_stops_on_cell(void)
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
    CHECK(runtime_player_position(game.runtime, 0) == 31,
          "Case_A4_010", "玩家应停在31号路障所在位置");
    CHECK(strstr(message, "停在 31 号位置") != NULL,
          "Case_A4_010", "提示应显示路障所在格");
    CHECK(strstr(message, "到达无主空地 31") != NULL,
          "Case_A4_010", "应继续处理路障所在格的落点事件");
    if (game.context == CONTEXT_BUY_CONFIRM) {
        (void)execute(&game, "N", message, sizeof(message));
    }
    (void)execute(&game, "Quit", message, sizeof(message));
    runtime_destroy(game.runtime);
}

static void test_replaced_cells_are_parks(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(execute(&game, "Step 14", message, sizeof(message)) == COMMAND_OK,
          "Case_New_Park", "应到达14号公园");
    CHECK(strstr(message, "到达公园，无特殊事件") != NULL,
          "Case_New_Park", "14号公园不应触发处理");
    CHECK(execute(&game, "Step 49", message, sizeof(message)) == COMMAND_OK,
          "Case_New_Park", "应到达49号公园");
    CHECK(strstr(message, "到达公园，无特殊事件") != NULL,
          "Case_New_Park", "49号公园不应触发处理");
    CHECK(execute(&game, "Step 49", message, sizeof(message)) == COMMAND_OK,
          "Case_New_Park", "应到达63号公园");
    CHECK(runtime_player_position(game.runtime, 0) == 63 &&
          strstr(message, "到达公园，无特殊事件") != NULL,
          "Case_New_Park", "63号公园不应触发处理");
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

static void test_tool_shop_pdf_boundaries(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(execute(&game, "Step 28", message, sizeof(message)) == COMMAND_OK,
          "Case_A13_PDF", "零点数玩家应到达道具屋");
    CHECK(strstr(message, "欢迎光临") != NULL &&
          strstr(message, "自动退出") != NULL,
          "Case_A13_PDF", "低于30点应提示后自动退出");
    CHECK(game.context == CONTEXT_TURN_START &&
          strcmp(runtime_current_player_name(game.runtime), "阿土伯") == 0,
          "Case_A13_PDF", "自动退出后应切换玩家");
    runtime_destroy(game.runtime);

    game = make_running_game();
    CHECK(execute(&game, "Step 66", message, sizeof(message)) == COMMAND_OK,
          "Case_A14_001", "钱夫人应取得40点");
    CHECK(execute(&game, "Step 35", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "1", message, sizeof(message)) == COMMAND_OK,
          "Case_A14_001", "阿土伯应完成礼品屋回合");
    CHECK(execute(&game, "Step 32", message, sizeof(message)) == COMMAND_OK &&
          game.context == CONTEXT_TOOL_SHOP,
          "Case_A14_001", "钱夫人应带40点进入道具屋");
    CHECK(execute(&game, "3", message, sizeof(message)) == COMMAND_INVALID,
          "Case_A14_001", "道具屋应删除旧炸弹选项");
    CHECK(strstr(message, "输入无效，请输入 1/2 或 F") != NULL &&
          game.context == CONTEXT_TOOL_SHOP,
          "Case_A14_001", "旧选项无效后应留在道具屋");
    runtime_destroy(game.runtime);
}

static void test_query_segment_and_help_arguments(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "Y", message, sizeof(message)) == COMMAND_OK,
          "Case_A7_002", "钱夫人应购买1号地");
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK,
          "Case_A7_002", "阿土伯应到达1号地并完成交租");
    CHECK(execute(&game, "Query", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "位置1：地段1") != NULL,
          "Case_A7_002", "Query应显示房产地段编号");
    CHECK(execute(&game, "Help extra", message, sizeof(message)) == COMMAND_INVALID &&
          strstr(message, "不接受参数") != NULL,
          "Case_A6_004", "Help应拒绝多余参数");
    runtime_destroy(game.runtime);
}

int main(void)
{
    test_bomb_command_removed();
    test_block_stops_on_cell();
    test_replaced_cells_are_parks();
    test_post_roll_sell_rejected();
    test_post_roll_help_then_advance();
    test_post_roll_quit_without_next_prompt();
    test_tool_shop_pdf_boundaries();
    test_query_segment_and_help_arguments();
    if (failures == 0) {
        (void)printf("[PASS] A4 workbook regressions: %d assertions passed.\n",
                     assertions);
        return 0;
    }
    (void)fprintf(stderr, "[FAIL] A4 workbook regressions: %d/%d failed.\n",
                  failures, assertions);
    return 1;
}
