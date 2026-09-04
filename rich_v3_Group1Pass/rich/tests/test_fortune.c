#include "map/map.h"
#include "monopoly/automation.h"
#include "monopoly/fortune.h"
#include "monopoly/runtime.h"

#include <stdio.h>
#include <string.h>

static int assertions;
static int failures;

#define CHECK(condition, description) do { \
    ++assertions; \
    if (!(condition)) { \
        ++failures; \
        (void)fprintf(stderr, "[FAIL] 财神: %s (line %d)\n", \
                      description, __LINE__); \
    } \
} while (0)

static void test_lifecycle_boundaries(void)
{
    FortuneState state;
    uint64_t round;

    fortune_init(&state, 2U);
    for (round = 1U; round <= 10U; ++round) {
        CHECK(fortune_advance_turn(&state, round) == FORTUNE_TURN_NONE,
              "前10回合不应出现财神");
    }
    CHECK(fortune_advance_turn(&state, 11U) == FORTUNE_TURN_SPAWN_DUE,
          "完成10回合后应在第11回合生成财神");
    CHECK(fortune_place(&state, 12, 11U), "应能把财神放到合法位置");
    for (round = 12U; round <= 15U; ++round) {
        CHECK(fortune_advance_turn(&state, round) == FORTUNE_TURN_NONE,
              "财神应在地图保持5个回合");
    }
    CHECK(fortune_advance_turn(&state, 16U) == FORTUNE_TURN_EXPIRED,
          "未被领取的财神应在第5回合结束后消失");
    CHECK(fortune_position(&state) == FORTUNE_NO_POSITION,
          "失效后地图不应保留财神位置");
    for (round = 17U; round <= 25U; ++round) {
        CHECK(fortune_advance_turn(&state, round) == FORTUNE_TURN_NONE,
              "失效后的10回合冷却期内不得再次出现");
    }
    CHECK(fortune_advance_turn(&state, 26U) == FORTUNE_TURN_SPAWN_DUE,
          "冷却10回合后应允许再次随机出现");
}

static void test_effect_duration(void)
{
    FortuneState state;
    int turn;

    fortune_init(&state, 2U);
    (void)fortune_advance_turn(&state, 11U);
    CHECK(fortune_place(&state, 9, 11U), "应放置财神");
    CHECK(fortune_collect(&state, 9, 0U, 11U), "路过玩家应领取财神");
    CHECK(fortune_effect_rounds(&state, 0U) == 5,
          "领取时应立即获得5回合效果");
    fortune_finish_player_turn(&state, 0U);
    CHECK(fortune_effect_rounds(&state, 0U) == 5,
          "领取当回合不应扣除效果次数");
    for (turn = 0; turn < 5; ++turn) {
        fortune_finish_player_turn(&state, 0U);
    }
    CHECK(fortune_effect_rounds(&state, 0U) == 0,
          "之后完成5个玩家回合时效果应归零");
}

static void test_grant_preserves_map_lifecycle(void)
{
    FortuneState state;
    fortune_init(&state, 2U);
    CHECK(fortune_grant(&state, 0U), "礼品屋应能独立发放财神");
    CHECK(fortune_advance_turn(&state, 10U) == FORTUNE_TURN_NONE &&
          fortune_advance_turn(&state, 11U) == FORTUNE_TURN_SPAWN_DUE,
          "礼品屋发放不应提前或推迟地图财神首次生成");
    CHECK(fortune_place(&state, 9, 11U), "地图应能与礼品屋财神并存");
    fortune_finish_player_turn(&state, 0U);
    fortune_finish_player_turn(&state, 0U);
    CHECK(fortune_grant(&state, 0U) && fortune_effect_rounds(&state, 0U) == 5,
          "重复领取应重置为5回合而非叠加");
    CHECK(fortune_position(&state) == 9 &&
          fortune_advance_turn(&state, 16U) == FORTUNE_TURN_EXPIRED,
          "礼品屋发放不应移除地图财神或重置其寿命");
    CHECK(fortune_grant(&state, 1U) &&
          fortune_advance_turn(&state, 25U) == FORTUNE_TURN_NONE &&
          fortune_advance_turn(&state, 26U) == FORTUNE_TURN_SPAWN_DUE,
          "礼品屋发放不应改变地图财神冷却");
}

static int is_land_position(int position)
{
    return (position >= 1 && position <= 13) ||
           (position >= 15 && position <= 27) ||
           (position >= 29 && position <= 34) ||
           (position >= 36 && position <= 48) ||
           (position >= 50 && position <= 62);
}

static void test_runtime_spawn_pickup_and_immediate_rent_exemption(void)
{
    AutomationPreset preset;
    GameRuntime *runtime;
    char message[4096];
    char map_text[8192];
    int i;
    int fortune_pos;
    int target_steps;
    int money_before;

    (void)memset(&preset, 0, sizeof(preset));
    preset.player_count = 2;
    preset.current_user_index = 0;
    preset.players[0].symbol = 'Q';
    preset.players[1].symbol = 'A';
    preset.players[0].fund = 100000;
    preset.players[1].fund = 100000;
    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        if (is_land_position(i)) {
            preset.properties[preset.property_count].position = i;
            preset.properties[preset.property_count].owner_index = 1;
            preset.properties[preset.property_count].level = 0;
            ++preset.property_count;
        }
    }

    runtime = runtime_load_preset(&preset);
    CHECK(runtime != NULL, "应创建财神集成测试运行时");
    if (runtime == NULL) {
        return;
    }

    for (i = 0; i < 10; ++i) {
        CHECK(runtime_step(runtime, 70, message, sizeof(message)) == 0,
              "两名玩家应能完成前10个游戏回合");
    }
    fortune_pos = runtime_fortune_position(runtime);
    CHECK(fortune_pos >= 0, "第11回合地图上应出现财神");
    CHECK(fortune_pos != 28 && fortune_pos != 35,
          "财神不得出现在道具屋或礼品屋");
    CHECK(runtime_render(runtime, map_text, sizeof(map_text)) == 0,
          "应能渲染含财神的地图");
    CHECK(strchr(map_text, 'F') != NULL, "地图应以F显示财神");

    target_steps = fortune_pos;
    while (!is_land_position(target_steps % RICH_MAP_SIZE)) {
        ++target_steps;
    }
    money_before = runtime_player_money(runtime, 0U);
    CHECK(runtime_step(runtime, target_steps, message, sizeof(message)) == 0,
          "当前玩家应路过财神并落到另一玩家的房产");
    CHECK(runtime_fortune_position(runtime) == FORTUNE_NO_POSITION,
          "财神被领取后应立即从地图消失");
    CHECK(runtime_player_god_rounds(runtime, 0U) == 5,
          "领取当回合应保留完整5回合效果");
    CHECK(strstr(message, "本回合立即生效") != NULL &&
          strstr(message, "财神附身，可免过路费") != NULL,
          "路过财神后同一回合应立即免付租金");
    CHECK(runtime_player_money(runtime, 0U) == money_before,
          "同回合免租不应扣除玩家资金");

    runtime_destroy(runtime);
}

int main(void)
{
    test_lifecycle_boundaries();
    test_effect_duration();
    test_grant_preserves_map_lifecycle();
    test_runtime_spawn_pickup_and_immediate_rent_exemption();
    if (failures == 0) {
        (void)printf("[PASS] 财神: %d assertions passed.\n", assertions);
        return 0;
    }
    (void)fprintf(stderr, "[FAIL] 财神: %d/%d assertions failed.\n",
                  failures, assertions);
    return 1;
}
