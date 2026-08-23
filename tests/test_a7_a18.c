#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/query.h"
#include "monopoly/runtime.h"
#include "monopoly/step.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int assertions = 0;

#define CHECK(condition, description) do { \
    assertions++; \
    if (!(condition)) { \
        failures++; \
        fprintf(stderr, "[FAIL] %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static void test_query_independent_formatter(void) {
    QueryPlayerState state;
    char message[8192];
    (void)memset(&state, 0, sizeof(state));
    state.player_id = 1;
    state.player_name = "钱夫人";
    state.symbol = 'Q';
    state.money = 12345;
    state.points = 260;
    state.position = 18;
    state.property_count = 2U;
    state.properties[0].position = 5;
    state.properties[0].land_price = 200;
    state.properties[0].building_level = 2;
    state.properties[1].position = 31;
    state.properties[1].land_price = 500;
    state.properties[1].building_level = 3;
    state.item_counts[QUERY_ITEM_BLOCK] = 2;
    state.item_counts[QUERY_ITEM_ROBOT] = 1;
    state.item_counts[QUERY_ITEM_BOMB] = 3;
    state.fortune_turns = 4;
    state.hospital_turns = 2;
    state.prison_turns = 1;

    CHECK(query_format_player(&state, message, sizeof(message)) == 0,
          "Query独立格式化应成功");
    CHECK(strstr(message, "资金：12345 元") != NULL, "Query显示资金");
    CHECK(strstr(message, "点数：260 点") != NULL, "Query显示点数");
    CHECK(strstr(message, "位置5：洋房（等级2") != NULL, "Query显示洋房");
    CHECK(strstr(message, "位置31：摩天楼（等级3") != NULL, "Query显示摩天楼");
    CHECK(strstr(message, "房产总数：2") != NULL, "Query显示房产数量");
    CHECK(strstr(message, "剩余道具：6/10") != NULL, "Query显示剩余道具总数");
    CHECK(strstr(message, "路障：2 个") != NULL, "Query显示路障");
    CHECK(strstr(message, "机器娃娃：1 个") != NULL, "Query显示机器娃娃");
    CHECK(strstr(message, "炸弹：3 个") != NULL, "Query显示炸弹");
    CHECK(strstr(message, "财神：4 回合") != NULL, "Query显示财神轮数");
    CHECK(strstr(message, "住院：2 回合") != NULL, "Query显示住院轮数");
    CHECK(strstr(message, "监狱：1 回合") != NULL, "Query显示监狱轮数");
}

static void test_query_runtime_integration(void) {
    Game game;
    char message[8192];
    game_init(&game);
    CHECK(game_start(&game), "游戏应启动");
    game.runtime = runtime_create(2, 10000);
    CHECK(game.runtime != NULL, "运行时应创建");
    CHECK(runtime_begin(game.runtime, message, sizeof(message)) == 0,
          "回合应开始");
    CHECK(runtime_set_player_money(game.runtime, 1, 12000) == 0,
          "资金接口可更新");
    CHECK(runtime_add_player_points(game.runtime, 1, 250) == 0,
          "点数接口可更新");
    CHECK(runtime_set_player_item_count(
              game.runtime, 1, QUERY_ITEM_BLOCK, 2) == 0,
          "道具接口可更新");
    CHECK(runtime_set_player_fortune_turns(game.runtime, 1, 5) == 0,
          "财神接口可更新");
    CHECK(runtime_assign_property(game.runtime, 1, 5, 2) == 0,
          "房产接口可分配土地");
    CHECK(command_execute(&game, "QuErY", message, sizeof(message)) == COMMAND_OK,
          "Query命令忽略大小写");
    CHECK(strstr(message, "资金：12000 元") != NULL, "集成Query显示资金");
    CHECK(strstr(message, "点数：250 点") != NULL, "集成Query显示点数");
    CHECK(strstr(message, "位置5：洋房") != NULL, "集成Query显示房产");
    CHECK(strstr(message, "财神：5 回合") != NULL, "集成Query显示财神");
    CHECK(command_execute(&game, "query extra", message, sizeof(message)) ==
              COMMAND_INVALID,
          "Query拒绝多余参数");
    runtime_destroy(game.runtime);
}

static void test_step_parser_and_integration(void) {
    Game game;
    char message[8192];
    int steps = 0;
    game_init(&game);
    CHECK(step_parse_argument("143", &steps) == STEP_PARSE_OK && steps == 143,
          "Step解析任意大于6的正整数");
    CHECK(step_parse_argument("0", &steps) == STEP_PARSE_OUT_OF_RANGE,
          "Step 0不会退化为随机Roll");
    CHECK(step_parse_argument("-1", &steps) == STEP_PARSE_OUT_OF_RANGE,
          "Step拒绝负数");
    CHECK(step_parse_argument("5 extra", &steps) == STEP_PARSE_EXTRA_ARGUMENT,
          "Step拒绝多余参数");
    CHECK(step_parse_argument("abc", &steps) == STEP_PARSE_NOT_INTEGER,
          "Step拒绝非整数");

    CHECK(game_start(&game), "Step测试游戏应启动");
    game.runtime = runtime_create(2, 10000);
    CHECK(game.runtime != NULL, "Step测试运行时应创建");
    CHECK(runtime_begin(game.runtime, message, sizeof(message)) == 0,
          "Step测试回合应开始");
    CHECK(command_execute(&game, "StEp 143", message, sizeof(message)) == COMMAND_OK,
          "Step命令忽略大小写");
    CHECK(strstr(message, "指定 143 步") != NULL,
          "Step明确显示遥控指定步数");
    CHECK(strstr(message, "未使用随机点数") != NULL,
          "Step明确未使用随机点数");
    CHECK(strstr(message, "移动到位置 3") != NULL,
          "Step 143从起点绕两圈后到位置3");

    CHECK(command_execute(&game, "Step 70", message, sizeof(message)) == COMMAND_OK,
          "下一玩家可指定70步");
    CHECK(command_execute(&game, "Query", message, sizeof(message)) == COMMAND_OK,
          "回到第一玩家后可查询");
    CHECK(strstr(message, "当前位置：3") != NULL,
          "Step移动结果保存在玩家状态中");
    runtime_destroy(game.runtime);
}

int main(void) {
    test_query_independent_formatter();
    test_query_runtime_integration();
    test_step_parser_and_integration();
    if (failures == 0) {
        printf("[PASS] A7/A18: %d assertions passed.\n", assertions);
        return 0;
    }
    fprintf(stderr, "[FAIL] A7/A18: %d/%d assertions failed.\n",
            failures, assertions);
    return 1;
}
