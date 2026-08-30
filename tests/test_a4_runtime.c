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

static void test_hospital_visit_does_not_skip(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(execute(&game, "Step 14", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_Hospital", "应到达医院");
    CHECK(strstr(message, "探访，无特殊事件") != NULL,
          "Case_A4_Hospital", "普通到达医院不应住院");
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "N", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "Help", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_Hospital", "下一玩家应正常完成回合");
    CHECK(strcmp(runtime_current_player_name(game.runtime), "钱夫人") == 0,
          "Case_A4_Hospital", "探访医院的玩家下一轮不应被跳过");
    runtime_destroy(game.runtime);
}

static void finish_declined_land_turn(Game *game, char *message,
                                      size_t message_size)
{
    (void)execute(game, "Step 1", message, message_size);
    (void)execute(game, "N", message, message_size);
    (void)execute(game, "Help", message, message_size);
}

static void test_prison_skips_exactly_two_turns(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(execute(&game, "Step 49", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_Prison", "应到达监狱");
    CHECK(strstr(message, "轮空 2 次") != NULL,
          "Case_A4_Prison", "入狱应声明轮空两次");
    finish_declined_land_turn(&game, message, sizeof(message));
    CHECK(strcmp(runtime_current_player_name(game.runtime), "阿土伯") == 0,
          "Case_A4_Prison", "第一次轮空后仍应由另一玩家行动");
    finish_declined_land_turn(&game, message, sizeof(message));
    CHECK(strcmp(runtime_current_player_name(game.runtime), "阿土伯") == 0,
          "Case_A4_Prison", "第二次轮空后仍应由另一玩家行动");
    finish_declined_land_turn(&game, message, sizeof(message));
    CHECK(strcmp(runtime_current_player_name(game.runtime), "钱夫人") == 0,
          "Case_A4_Prison", "两次轮空结束后应恢复行动");
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
          "Case_A14_001", "40点不足购买炸弹");
    CHECK(strstr(message, "已退出道具屋") != NULL &&
          game.context == CONTEXT_TURN_START,
          "Case_A14_001", "点数不足后应退出道具屋");
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
    test_bomb_skip_reason();
    test_block_stops_on_cell();
    test_hospital_visit_does_not_skip();
    test_prison_skips_exactly_two_turns();
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
