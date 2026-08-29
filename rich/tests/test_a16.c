#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/magic.h"
#include "monopoly/runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct EffectLog {
    int calls;
    size_t caster;
    int effect_id;
    MagicTarget target;
    int reject;
} EffectLog;

static int failures;
static int tests_run;
static const int test_roles[2] = {1, 2};

#define CHECK(condition, description) do { \
    ++tests_run; \
    if (!(condition)) { \
        ++failures; \
        fprintf(stderr, "[FAIL] A16: %s (line %d)\n", description, __LINE__); \
    } \
} while (0)

static int record_effect(size_t caster_index,
                         int effect_id,
                         const MagicTarget *target,
                         void *context)
{
    EffectLog *log = (EffectLog *)context;
    ++log->calls;
    log->caster = caster_index;
    log->effect_id = effect_id;
    log->target = *target;
    return log->reject;
}

static void test_extensible_magic_module(void)
{
    MagicHouseState house;
    MagicTarget target = {MAGIC_TARGET_PLAYER, 1};
    EffectLog log = {0, 99U, 0, {MAGIC_TARGET_NONE, 0}, 0};
    MagicEffect effects[] = {
        {1, "测试魔法一", record_effect, &log},
        {2, "测试魔法二", record_effect, &log}
    };

    magic_house_init(&house, 2);
    CHECK(magic_house_begin(&house, 0) == MAGIC_OK, "魔法屋应打开");
    CHECK(magic_house_answer(&house, "9", effects, 2, &target) ==
          MAGIC_INVALID_CHOICE, "未注册编号应被拒绝");
    CHECK(house.is_open && log.calls == 0, "无效输入后应继续等待");
    CHECK(magic_house_answer(&house, "2", effects, 2, &target) == MAGIC_OK,
          "已注册魔法应执行成功");
    CHECK(log.calls == 1 && log.caster == 0U && log.effect_id == 2,
          "回调应收到正确施法者和魔法编号");
    CHECK(log.target.type == MAGIC_TARGET_PLAYER && log.target.value == 1,
          "回调应收到原样目标参数");

    CHECK(magic_house_begin(&house, 1) == MAGIC_OK, "魔法屋应能再次打开");
    log.reject = 1;
    CHECK(magic_house_answer(&house, "1", effects, 2, &target) ==
          MAGIC_EFFECT_REJECTED, "回调可以拒绝当前魔法");
    CHECK(house.is_open, "魔法被拒绝后应继续等待");
    CHECK(magic_house_answer(&house, "F", effects, 2, &target) == MAGIC_EXIT,
          "F 应退出魔法屋");
}

static void test_magic_through_dispatcher(void)
{
    Game game;
    GameRuntime *runtime;
    EffectLog log = {0, 99U, 0, {MAGIC_TARGET_NONE, 0}, 0};
    MagicEffect effect = {7, "集成测试魔法", record_effect, &log};
    char message[1024];

    game_init(&game);
    (void)game_start(&game);
    runtime = runtime_create(2, 1000, test_roles);
    game.runtime = runtime;
    CHECK(runtime != NULL, "应创建主工程 GameRuntime");
    CHECK(runtime_register_magic_effect(runtime, &effect) == 0,
          "运行时应接受具体魔法注册");
    CHECK(runtime_begin(runtime, message, sizeof(message)) == 0, "应开始回合");
    CHECK(command_execute(&game, "Step 63", message, sizeof(message)) == COMMAND_OK,
          "Step 63 应准确到达魔法屋");
    CHECK(game.context == CONTEXT_MAGIC_HOUSE,
          "到达第 63 格后 Game.context 应进入魔法屋");
    CHECK(command_execute(&game, "99", message, sizeof(message)) == COMMAND_INVALID,
          "错误魔法编号应报告无效");
    CHECK(game.context == CONTEXT_MAGIC_HOUSE,
          "错误编号后应仍停留在魔法屋");
    CHECK(command_execute(&game, "7", message, sizeof(message)) == COMMAND_OK,
          "已注册魔法应通过统一命令入口执行");
    CHECK(log.calls == 1 && log.caster == 0U && log.effect_id == 7,
          "集成回调参数应正确");
    CHECK(game.context == CONTEXT_TURN_START,
          "施法完成后应结束落地并切换回合");
    CHECK(strcmp(runtime_current_player_name(runtime), "阿土伯") == 0,
          "A4 应切换到下一位玩家");
    runtime_destroy(runtime);
}

int main(void)
{
    test_extensible_magic_module();
    test_magic_through_dispatcher();
    if (failures == 0) {
        printf("[PASS] A16: %d assertions passed.\n", tests_run);
        return 0;
    }
    fprintf(stderr, "[FAIL] A16: %d/%d assertions failed.\n",
            failures, tests_run);
    return 1;
}
