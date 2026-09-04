#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "monopoly/automation.h"

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

static Game make_owned_land_game(int position, int owner_index)
{
    Game game;
    AutomationPreset preset = {0};
    preset.player_count = 2;
    preset.players[0].symbol = 'Q';
    preset.players[0].fund = 1000;
    preset.players[1].symbol = 'A';
    preset.players[1].fund = 1000;
    preset.property_count = 1;
    preset.properties[0].position = position;
    preset.properties[0].owner_index = owner_index;
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_load_preset(&preset);
    return game;
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
    /* N 后已经切换玩家；Query 只查询，不再推进回合。 */
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
    AutomationSnapshot snapshot;
    CHECK(execute(game, "Step 1", message, message_size) == COMMAND_OK,
          "Case_A4_023", "应进入购买确认");
    CHECK(execute(game, "N", message, message_size) == COMMAND_OK,
          "Case_A4_023", "应完成不购买处理");
    CHECK(strstr(message, "放弃购买") != NULL &&
          strstr(message, "轮到玩家 阿土伯（2 号）") != NULL,
          "Case_Decline_Prompt", "放弃购买的同一条输出应立即提示下一玩家");
    runtime_snapshot(game->runtime, &snapshot);
    CHECK(game->context == CONTEXT_TURN_START &&
          snapshot.current_user_index == 1 &&
          snapshot.phase == AUTOMATION_PHASE_COMMAND,
          "Case_Decline_Prompt", "实际输入权也必须切换到下一玩家");
}

static void test_next_player_can_sell_after_decline(void)
{
    Game game = make_owned_land_game(2, 1);
    char message[2048];
    CHECK(game.runtime != NULL, "Case_A4_023", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    prepare_declined_purchase(&game, message, sizeof(message));
    CHECK(execute(&game, "Sell 2", message, sizeof(message)) == COMMAND_OK,
          "Case_Decline_Sell", "下一玩家应可直接出售自己的房产");
    CHECK(runtime_player_money(game.runtime, 1) == 1400 &&
          runtime_player_money(game.runtime, 0) == 1000,
          "Case_Decline_Sell", "卖房收入应归属新回合玩家");
    runtime_destroy(game.runtime);
}

static void test_help_query_do_not_advance_after_decline(void)
{
    Game game = make_running_game();
    char message[4096];
    CHECK(game.runtime != NULL, "Case_A4_024", "应创建运行时");
    if (game.runtime == NULL) {
        return;
    }
    prepare_declined_purchase(&game, message, sizeof(message));
    CHECK(execute(&game, "Help", message, sizeof(message)) == COMMAND_OK,
          "Case_A4_024", "下一玩家应允许查看帮助");
    CHECK(strstr(message, "可用命令") != NULL,
          "Case_A4_024", "应显示完整帮助");
    CHECK(execute(&game, "Query", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "玩家：阿土伯") != NULL &&
          strcmp(runtime_current_player_name(game.runtime), "阿土伯") == 0,
          "Case_Decline_Query", "帮助和查询均不得额外切换玩家");
    CHECK(execute(&game, "Step 14", message, sizeof(message)) == COMMAND_OK &&
          runtime_player_position(game.runtime, 1) == 14 &&
          runtime_player_position(game.runtime, 0) == 1 &&
          strstr(message, "轮到玩家 钱夫人") != NULL,
          "Case_Decline_Move", "下一次移动应由新玩家执行，完成后正常换人");
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

static void test_declined_upgrade_advances_immediately(void)
{
    Game game = make_owned_land_game(1, 0);
    AutomationSnapshot snapshot;
    char message[2048];
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          game.context == CONTEXT_UPGRADE_CONFIRM,
          "Case_Upgrade_Decline", "应进入自有房产升级确认");
    CHECK(execute(&game, "n", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "放弃升级") != NULL &&
          strstr(message, "轮到玩家 阿土伯") != NULL,
          "Case_Upgrade_Decline", "放弃升级也应立即提示下一玩家");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(snapshot.current_user_index == 1 &&
          snapshot.properties[0].level == 0 &&
          runtime_player_money(game.runtime, 0) == 1000,
          "Case_Upgrade_Decline", "拒绝升级不改变资产，且输入权确实切换");
    runtime_destroy(game.runtime);
}

static void test_invalid_answer_keeps_current_turn(void)
{
    Game game = make_running_game();
    char message[2048];
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "maybe", message, sizeof(message)) == COMMAND_INVALID,
          "Case_Invalid_Answer", "无效确认输入应被拒绝");
    CHECK(game.context == CONTEXT_BUY_CONFIRM &&
          strcmp(runtime_current_player_name(game.runtime), "钱夫人") == 0,
          "Case_Invalid_Answer", "无效输入不得提前结束当前回合");
    CHECK(execute(&game, "N", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "轮到玩家 阿土伯") != NULL,
          "Case_Invalid_Answer", "随后有效拒绝才切换玩家");
    runtime_destroy(game.runtime);
}

static void test_four_player_decline_rotation(void)
{
    Game game;
    const int chosen_roles[4] = {1, 2, 3, 4};
    const char *names[4] = {"钱夫人", "阿土伯", "孙小美", "金贝贝"};
    AutomationSnapshot snapshot;
    char message[2048];
    int i;
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_create(4, 1000, chosen_roles);
    CHECK(runtime_begin(game.runtime, message, sizeof(message)) == 0,
          "Case_Decline_Rotation", "四人游戏应正常开始");
    for (i = 0; i < 4; ++i) {
        CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
              execute(&game, "N", message, sizeof(message)) == COMMAND_OK,
              "Case_Decline_Rotation", "每名玩家均应正常完成放弃购买");
        runtime_snapshot(game.runtime, &snapshot);
        CHECK(snapshot.current_user_index == (i + 1) % 4 &&
              strstr(message, "轮到玩家") != NULL &&
              strstr(message, names[(i + 1) % 4]) != NULL,
              "Case_Decline_Rotation", "每次拒绝都应正确换人，包括最后一人回到第一人");
    }
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

static void test_tool_shop_continuous_purchase(void)
{
    Game game;
    AutomationPreset preset = {0};
    AutomationSnapshot snapshot;
    char message[4096];

    preset.player_count = 2;
    preset.players[0].symbol = 'Q';
    preset.players[0].fund = 1000;
    preset.players[0].credit = 100;
    preset.players[0].position = 27;
    preset.players[1].symbol = 'A';
    preset.players[1].fund = 1000;
    preset.players[1].position = 0;

    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_load_preset(&preset);
    CHECK(game.runtime != NULL,
          "Case_ToolShop_Continuous", "应创建道具屋测试运行时");
    if (game.runtime == NULL) {
        return;
    }

    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "2", message, sizeof(message)) == COMMAND_OK,
          "Case_ToolShop_Continuous", "首次购买机器娃娃应成功");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(game.context == CONTEXT_TOOL_SHOP &&
          snapshot.current_user_index == 0 &&
          snapshot.players[0].credit == 70 &&
          snapshot.players[0].robot == 1 &&
          strstr(message, "请输入 1/2 选择道具") != NULL,
          "Case_ToolShop_Continuous", "仍能购买时应留在道具屋并继续显示菜单");

    CHECK(execute(&game, "1", message, sizeof(message)) == COMMAND_OK,
          "Case_ToolShop_Continuous", "第二次购买路障应成功");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(game.context == CONTEXT_TURN_START &&
          snapshot.current_user_index == 1 &&
          snapshot.players[0].credit == 20 &&
          snapshot.players[0].block == 1 &&
          strstr(message, "自动退出道具屋") != NULL,
          "Case_ToolShop_Continuous", "剩余点数不足30时应自动退出并切换玩家");

    runtime_destroy(game.runtime);
}

static void test_property_notice_uses_role_name(void)
{
    Game game = make_owned_land_game(1, 1);
    char message[2048];

    CHECK(game.runtime != NULL,
          "Case_RoleName_Notice", "应创建房产提示测试运行时");
    if (game.runtime == NULL) {
        return;
    }
    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          strstr(message, "到达阿土伯的房产") != NULL &&
          strstr(message, "玩家 2") == NULL,
          "Case_RoleName_Notice", "房产提示应显示所选角色名而不是内部玩家编号");
    runtime_destroy(game.runtime);
}

static void test_full_tool_bag_requires_manual_exit(void)
{
    Game game;
    AutomationPreset preset = {0};
    AutomationSnapshot snapshot;
    char message[4096];

    preset.player_count = 2;
    preset.players[0].symbol = 'Q';
    preset.players[0].fund = 1000;
    preset.players[0].credit = 100;
    preset.players[0].position = 27;
    preset.players[0].block = 9;
    preset.players[1].symbol = 'A';
    preset.players[1].fund = 1000;

    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_load_preset(&preset);
    CHECK(game.runtime != NULL,
          "Case_ToolShop_Full", "应创建满背包测试运行时");
    if (game.runtime == NULL) {
        return;
    }

    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "2", message, sizeof(message)) == COMMAND_OK,
          "Case_ToolShop_Full", "第10个道具应购买成功");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(game.context == CONTEXT_TOOL_SHOP &&
          snapshot.current_user_index == 0 &&
          snapshot.players[0].block == 9 &&
          snapshot.players[0].robot == 1,
          "Case_ToolShop_Full", "买满10个后应留在道具屋且不切换玩家");

    CHECK(execute(&game, "1", message, sizeof(message)) == COMMAND_INVALID &&
          strstr(message, "购买失败：道具数量已达上限 10") != NULL,
          "Case_ToolShop_Full", "满背包再次购买应明确提示失败");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(game.context == CONTEXT_TOOL_SHOP &&
          snapshot.current_user_index == 0,
          "Case_ToolShop_Full", "购买失败后应继续等待玩家手动退出");

    CHECK(execute(&game, "F", message, sizeof(message)) == COMMAND_OK,
          "Case_ToolShop_Full", "F 应能手动退出道具屋");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(game.context == CONTEXT_TURN_START &&
          snapshot.current_user_index == 1,
          "Case_ToolShop_Full", "手动退出后应结束回合并切换玩家");
    runtime_destroy(game.runtime);
}

static void test_step_zero_is_legal(void)
{
    Game game = make_running_game();
    AutomationSnapshot snapshot;
    char message[2048];

    CHECK(game.runtime != NULL,
          "Case_Step_Zero", "应创建 Step 0 测试运行时");
    if (game.runtime == NULL) {
        return;
    }
    CHECK(execute(&game, "Step 0", message, sizeof(message)) == COMMAND_OK,
          "Case_Step_Zero", "Step 0 应为合法命令");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(snapshot.players[0].position == 0 &&
          snapshot.current_user_index == 1 &&
          strstr(message, "掷出 0 点") != NULL,
          "Case_Step_Zero", "Step 0 应原地结算并结束当前回合");
    runtime_destroy(game.runtime);
}

static void test_tenth_tool_with_low_points_auto_exits(void)
{
    Game game;
    AutomationPreset preset = {0};
    AutomationSnapshot snapshot;
    char message[4096];

    preset.player_count = 2;
    preset.players[0].symbol = 'Q';
    preset.players[0].fund = 1000;
    preset.players[0].credit = 30;
    preset.players[0].position = 27;
    preset.players[0].block = 9;
    preset.players[1].symbol = 'A';
    preset.players[1].fund = 1000;

    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_load_preset(&preset);
    CHECK(game.runtime != NULL,
          "Case_ToolShop_Tenth_LowPoints", "应创建第10个道具边界测试运行时");
    if (game.runtime == NULL) {
        return;
    }

    CHECK(execute(&game, "Step 1", message, sizeof(message)) == COMMAND_OK &&
          execute(&game, "2", message, sizeof(message)) == COMMAND_OK,
          "Case_ToolShop_Tenth_LowPoints", "第10个道具应购买成功");
    runtime_snapshot(game.runtime, &snapshot);
    CHECK(snapshot.players[0].block == 9 &&
          snapshot.players[0].robot == 1 &&
          snapshot.players[0].credit == 0 &&
          game.context == CONTEXT_TURN_START &&
          snapshot.current_user_index == 1 &&
          strstr(message, "自动退出道具屋") != NULL,
          "Case_ToolShop_Tenth_LowPoints",
          "刚好买满10个且点数低于30时应自动退出并切换玩家");
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
    test_next_player_can_sell_after_decline();
    test_help_query_do_not_advance_after_decline();
    test_post_roll_quit_without_next_prompt();
    test_declined_upgrade_advances_immediately();
    test_invalid_answer_keeps_current_turn();
    test_four_player_decline_rotation();
    test_tool_shop_pdf_boundaries();
    test_tool_shop_continuous_purchase();
    test_property_notice_uses_role_name();
    test_full_tool_bag_requires_manual_exit();
    test_step_zero_is_legal();
    test_tenth_tool_with_low_points_auto_exits();
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
