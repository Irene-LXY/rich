#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/startup.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int assertions = 0;

#define CHECK(condition, case_id, description) do { \
    assertions++; \
    if (!(condition)) { \
        failures++; \
        fprintf(stderr, "[FAIL] %s: %s (line %d)\n", case_id, description, __LINE__); \
    } \
} while (0)

static void test_single_command_starts_setup(void) {
    Game game;
    char message[256];
    char program[] = "monopoly";
    char *arguments[] = {program, 0};
    game_init(&game);
    CHECK(application_start(&game, 1, arguments, message, sizeof(message)) == STARTUP_OK,
          "Case_A1_001", "单一命令应成功启动");
    CHECK(game.phase == GAME_RUNNING, "Case_A1_001", "成功后游戏实例进入运行状态");
    CHECK(game.setup_step == SETUP_PLAYER_COUNT, "Case_A1_002", "启动后首先进入玩家人数步骤");
    CHECK(strstr(message, "玩家人数 -> 初始资金 -> 角色选择") != 0,
          "Case_A1_002", "应明确显示完整开局引导顺序");
    CHECK(strstr(message, "请输入玩家人数") != 0,
          "Case_A1_002", "应自动显示玩家人数输入提示");
}

static void test_invalid_arguments_leave_no_state(void) {
    Game game;
    char message[256];
    char program[] = "monopoly";
    char bad[] = "badArg";
    char *arguments[] = {program, bad, 0};
    game_init(&game);
    CHECK(application_start(&game, 2, arguments, message, sizeof(message)) == STARTUP_INVALID_ARGUMENT,
          "Case_A1_006", "非法启动参数应被拒绝");
    CHECK(game.phase == GAME_NOT_STARTED, "Case_A1_007", "失败后不得留下已启动状态");
    CHECK(game.state_revision == 0, "Case_A1_007", "失败后不得写入任何游戏状态");
    CHECK(strstr(message, "不接受启动参数") != 0,
          "Case_A1_006", "应说明参数问题及正确运行方法");
}

static void test_missing_program_identity_is_rejected(void) {
    Game game;
    char message[256];
    char *arguments[] = {0};
    game_init(&game);
    CHECK(application_start(&game, 1, arguments, message, sizeof(message)) == STARTUP_INVALID_ARGUMENT,
          "Case_A1_005", "缺少可执行程序标识时启动失败");
    CHECK(game.phase == GAME_NOT_STARTED, "Case_A1_005", "失败时不生成半初始化状态");
}

static void test_same_instance_cannot_start_twice(void) {
    Game game;
    char message[256];
    char program[] = "monopoly";
    char *arguments[] = {program, 0};
    game_init(&game);
    (void)application_start(&game, 1, arguments, message, sizeof(message));
    CHECK(application_start(&game, 1, arguments, message, sizeof(message)) == STARTUP_ALREADY_STARTED,
          "Case_A1_008", "同一个游戏实例不能重复初始化");
    CHECK(game.state_revision == 1, "Case_A1_008", "重复启动不得再次修改状态");
}

static void test_instances_are_isolated(void) {
    Game first;
    Game second;
    char message[256];
    char program[] = "monopoly";
    char *arguments[] = {program, 0};
    game_init(&first);
    game_init(&second);
    (void)application_start(&first, 1, arguments, message, sizeof(message));
    (void)application_start(&second, 1, arguments, message, sizeof(message));
    (void)command_execute(&first, "quit", message, sizeof(message));
    CHECK(first.phase == GAME_ENDED, "Case_A1_009", "第一个实例可独立结束");
    CHECK(second.phase == GAME_RUNNING, "Case_A1_009", "第二个实例不受第一个实例影响");
    CHECK(second.end_reason == END_REASON_NONE, "Case_A1_009", "实例间结束原因也必须隔离");
}

int main(void) {
    test_single_command_starts_setup();
    test_invalid_arguments_leave_no_state();
    test_missing_program_identity_is_rejected();
    test_same_instance_cannot_start_twice();
    test_instances_are_isolated();
    if (failures == 0) {
        printf("[PASS] A1: %d assertions passed.\n", assertions);
        return 0;
    }
    fprintf(stderr, "[FAIL] A1: %d/%d assertions failed.\n", failures, assertions);
    return 1;
}

