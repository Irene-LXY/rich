#include "monopoly/command.h"
#include "monopoly/game.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int tests_run = 0;

#define CHECK(condition, case_id, description) do { \
    tests_run++; \
    if (!(condition)) { \
        failures++; \
        fprintf(stderr, "[FAIL] %s: %s (line %d)\n", case_id, description, __LINE__); \
    } \
} while (0)

static Game running_game(GameContext context) {
    Game game;
    game_init(&game);
    (void)game_start(&game);
    game.context = context;
    return game;
}

static void test_quit_ends_whole_game(void) {
    const char *forms[] = {"quit", "Quit", "QUIT", "qUiT", "  quit  \n"};
    size_t index;
    for (index = 0; index < sizeof(forms) / sizeof(forms[0]); index++) {
        Game game = running_game(CONTEXT_TURN_START);
        char message[128];
        CommandResult result = command_execute(&game, forms[index], message, sizeof(message));
        CHECK(result == COMMAND_OK, "Case_A20_001", "Quit 应忽略大小写和首尾空白");
        CHECK(game.phase == GAME_ENDED, "Case_A20_001", "Quit 应结束整局游戏");
        CHECK(game.end_reason == END_REASON_USER_QUIT, "Case_A20_001", "应记录用户强制退出原因");
    }
}

static void test_quit_is_available_in_all_running_contexts(void) {
    const GameContext contexts[] = {
        CONTEXT_TURN_START,
        CONTEXT_BUY_CONFIRM,
        CONTEXT_GIFT_HOUSE,
        CONTEXT_TOLL_SETTLEMENT
    };
    size_t index;
    for (index = 0; index < sizeof(contexts) / sizeof(contexts[0]); index++) {
        Game game = running_game(contexts[index]);
        char message[128];
        CommandResult result = command_execute(&game, "quit", message, sizeof(message));
        CHECK(result == COMMAND_OK, "Case_A20_002", "运行中的任意交互场景均可 Quit");
        CHECK(game.phase == GAME_ENDED, "Case_A20_002", "任意场景 Quit 后整局结束");
    }
}

static void test_invalid_commands_do_not_end_game(void) {
    const char *invalid[] = {"qui", "quitt", "quit now", "quit a b c", "roll"};
    size_t index;
    for (index = 0; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        Game game = running_game(CONTEXT_TURN_START);
        char message[128];
        CommandResult result = command_execute(&game, invalid[index], message, sizeof(message));
        CHECK(result == COMMAND_INVALID, "Case_A20_003", "拼写错误、空输入、多余参数及其他命令不能触发 Quit");
        CHECK(game.phase == GAME_RUNNING, "Case_A20_003", "无效输入不得改变游戏状态");
    }
}

static void test_setup_blank_uses_pdf_default_without_quitting(void) {
    Game game = running_game(CONTEXT_TURN_START);
    char message[256];
    CHECK(command_execute(&game, "", message, sizeof(message)) == COMMAND_OK,
          "Case_A20_PDF_Default", "开局空输入应采用说明书默认资金");
    CHECK(game.phase == GAME_RUNNING && game.end_reason == END_REASON_NONE,
          "Case_A20_PDF_Default", "默认资金输入不能触发Quit");
}

static void test_misspelled_quit_has_clear_setup_error(void) {
    Game game = running_game(CONTEXT_TURN_START);
    char message[256];
    CHECK(command_execute(&game, "quitt", message, sizeof(message)) == COMMAND_INVALID,
          "Case_A20_027", "quitt不能触发Quit");
    CHECK(strstr(message, "无效命令") != NULL && game.phase == GAME_RUNNING,
          "Case_A20_027", "开局阶段也应给出明确的无效命令原因");
}

static void test_quit_before_start_is_rejected(void) {
    Game game;
    char message[128];
    game_init(&game);
    CHECK(command_execute(&game, "quit", message, sizeof(message)) == COMMAND_NOT_ALLOWED,
          "Case_A20_004", "游戏开始前不能执行 Quit");
    CHECK(game.phase == GAME_NOT_STARTED, "Case_A20_004", "拒绝后状态保持未开始");
}

static void test_commands_after_quit_are_ignored(void) {
    Game game = running_game(CONTEXT_TURN_START);
    char message[128];
    unsigned long revision;
    CHECK(command_execute(&game, "quit", message, sizeof(message)) == COMMAND_OK,
          "Case_A20_005", "第一次 Quit 应成功");
    revision = game.state_revision;
    CHECK(command_execute(&game, "roll", message, sizeof(message)) == COMMAND_GAME_ENDED,
          "Case_A20_005", "Quit 后后续命令全部无效");
    CHECK(command_execute(&game, "quit", message, sizeof(message)) == COMMAND_GAME_ENDED,
          "Case_A20_005", "重复 Quit 不重复处理");
    CHECK(game.state_revision == revision, "Case_A20_005", "游戏结束后状态不得再次变化");
}

int main(void) {
    test_quit_ends_whole_game();
    test_quit_is_available_in_all_running_contexts();
    test_invalid_commands_do_not_end_game();
    test_setup_blank_uses_pdf_default_without_quitting();
    test_misspelled_quit_has_clear_setup_error();
    test_quit_before_start_is_rejected();
    test_commands_after_quit_are_ignored();

    if (failures == 0) {
        printf("[PASS] A20: %d assertions passed.\n", tests_run);
        return 0;
    }
    fprintf(stderr, "[FAIL] A20: %d/%d assertions failed.\n", failures, tests_run);
    return 1;
}
