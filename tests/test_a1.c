#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
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
    CHECK(game.setup_step == SETUP_INITIAL_MONEY, "Case_A1_002", "启动后首先进入初始资金步骤");
    CHECK(strstr(message, "初始资金 -> 一次性选择角色") != 0,
          "Case_A1_002", "应明确显示完整开局引导顺序");
    CHECK(strstr(message, "请输入每位玩家初始资金") != 0,
          "Case_A1_002", "应自动显示初始资金输入提示");
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

static void test_pdf_setup_flow(void) {
    Game game;
    char message[1024];
    char program[] = "rich";
    char *arguments[] = {program, 0};
    game_init(&game);
    (void)application_start(&game, 1, arguments, message, sizeof(message));
    CHECK(command_execute(&game, "", message, sizeof(message)) == COMMAND_OK,
          "Case_A2_PDF_001", "直接回车应采用默认资金10000");
    CHECK(game.runtime == NULL && strstr(message, "按回车") != NULL,
          "Case_A2_PDF_Confirm", "默认资金应进入确认环节");
    CHECK(command_execute(&game, "", message, sizeof(message)) == COMMAND_OK,
          "Case_A2_PDF_Confirm", "资金确认环节回车应确认");
    CHECK(command_execute(&game, "12", message, sizeof(message)) == COMMAND_OK,
          "Case_A3_PDF_001", "组合角色编号12应创建游戏");
    CHECK(game.runtime != NULL && game.setup_initial_money == 10000 &&
          game.setup_chosen[0] == 1 && game.setup_chosen[1] == 2 &&
          strcmp(runtime_current_player_name(game.runtime), "钱夫人") == 0,
          "Case_A3_PDF_001", "12应按顺序创建钱夫人、阿土伯");
    runtime_destroy(game.runtime);
}

static void test_confirmed_setup_flow(void) {
    Game game;
    char message[1024];
    char program[] = "rich";
    char *arguments[] = {program, 0};
    game_init(&game);
    (void)application_start(&game, 1, arguments, message, sizeof(message));
    CHECK(command_execute(&game, "2", message, sizeof(message)) == COMMAND_INVALID,
          "Case_A3_LowerBound", "2不能再被误认为玩家人数");
    CHECK(command_execute(&game, "20000", message, sizeof(message)) == COMMAND_OK,
          "Case_A3_001", "应接受合法初始资金");
    CHECK(command_execute(&game, "", message, sizeof(message)) == COMMAND_OK &&
          game.setup_step == SETUP_ROLE_SELECTION,
          "Case_A3_EnterConfirm", "资金确认环节应支持回车确认");
    CHECK(command_execute(&game, "324", message, sizeof(message)) == COMMAND_OK &&
          game.runtime != NULL,
          "Case_A2_Confirm", "324应一次性创建3名玩家");
    CHECK(game.setup_player_count == 3 &&
          game.setup_chosen[0] == 3 && game.setup_chosen[1] == 2 &&
          game.setup_chosen[2] == 4 &&
          strcmp(runtime_current_player_name(game.runtime), "孙小美") == 0,
          "Case_A2_Order", "324必须保持角色输入顺序");
    runtime_destroy(game.runtime);
}

int main(void) {
    test_single_command_starts_setup();
    test_invalid_arguments_leave_no_state();
    test_missing_program_identity_is_rejected();
    test_same_instance_cannot_start_twice();
    test_instances_are_isolated();
    test_pdf_setup_flow();
    test_confirmed_setup_flow();
    if (failures == 0) {
        printf("[PASS] A1: %d assertions passed.\n", assertions);
        return 0;
    }
    fprintf(stderr, "[FAIL] A1: %d/%d assertions failed.\n", failures, assertions);
    return 1;
}

