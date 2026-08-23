#include "a4_turn_manager.h"

#include <stdio.h>
#include <string.h>

typedef struct TestContext {
    A4MoveResult next_move_result;
    int move_calls;
    int pre_action_calls;
    int state_calls;
    int skip_calls;
    int notice_calls;
    int finished_calls;
    int last_forced_steps;
    A4PlayerId last_player_id;
    A4PlayerId winner_id;
    A4TurnPhase last_phase;
    uint16_t last_skip_before;
    uint16_t last_skip_after;
} TestContext;

#define CHECK(expression)                                                       \
    do {                                                                        \
        if (!(expression)) {                                                    \
            (void)fprintf(                                                      \
                stderr,                                                         \
                "CHECK failed at %s:%d: %s\n",                                 \
                __FILE__,                                                       \
                __LINE__,                                                       \
                #expression                                                     \
            );                                                                  \
            return 1;                                                           \
        }                                                                       \
    } while (0)

static bool test_pre_action(
    void *context,
    const A4TurnSnapshot *snapshot,
    const A4PreRollAction *action
)
{
    TestContext *test = (TestContext *)context;
    CHECK(snapshot != NULL);
    CHECK(action != NULL);
    ++test->pre_action_calls;
    test->last_player_id = snapshot->current_player_id;
    return true;
}

static A4MoveResult test_roll_and_move(
    void *context,
    const A4TurnSnapshot *snapshot,
    int forced_steps,
    int *actual_steps
)
{
    TestContext *test = (TestContext *)context;

    ++test->move_calls;
    test->last_player_id = snapshot->current_player_id;
    test->last_forced_steps = forced_steps;
    *actual_steps = forced_steps == 0 ? 4 : forced_steps;
    return test->next_move_result;
}

static void test_state_changed(
    void *context,
    A4StateChange change,
    const A4TurnSnapshot *snapshot
)
{
    TestContext *test = (TestContext *)context;
    (void)change;
    ++test->state_calls;
    test->last_player_id = snapshot->current_player_id;
    test->last_phase = snapshot->phase;
}

static void test_player_skipped(
    void *context,
    const A4TurnSnapshot *snapshot,
    A4SkipReason reason,
    uint16_t remaining_after_skip,
    const char *note
)
{
    TestContext *test = (TestContext *)context;
    (void)reason;
    (void)note;
    ++test->skip_calls;
    test->last_player_id = snapshot->current_player_id;
    test->last_skip_before = snapshot->skip_turns_remaining;
    test->last_skip_after = remaining_after_skip;
}

static void test_notice(
    void *context,
    A4TurnStatus status,
    const char *detail,
    const A4TurnSnapshot *snapshot
)
{
    TestContext *test = (TestContext *)context;
    (void)status;
    (void)detail;
    (void)snapshot;
    ++test->notice_calls;
}

static void test_game_finished(
    void *context,
    A4PlayerId winner_id,
    const A4TurnSnapshot *snapshot
)
{
    TestContext *test = (TestContext *)context;
    ++test->finished_calls;
    test->winner_id = winner_id;
    test->last_phase = snapshot->phase;
}

static int test_validation(void)
{
    A4TurnManager manager;
    const A4PlayerConfig too_few[] = {{1U, "钱夫人"}};
    const A4PlayerConfig duplicate[] = {
        {1U, "钱夫人"},
        {1U, "阿土伯"}
    };

    CHECK(a4_turn_manager_init(&manager, too_few, 1U, NULL) ==
          A4_TURN_ERR_PLAYER_COUNT);
    CHECK(a4_turn_manager_init(&manager, duplicate, 2U, NULL) ==
          A4_TURN_ERR_DUPLICATE_PLAYER);
    return 0;
}

static int test_full_turn_flow(void)
{
    A4TurnManager manager;
    TestContext test;
    A4TurnSnapshot snapshot;
    A4PreRollAction action;
    const A4PlayerConfig players[] = {
        {101U, "钱夫人"},
        {102U, "阿土伯"},
        {103U, "孙小美"}
    };
    A4TurnHooks hooks;

    (void)memset(&test, 0, sizeof(test));
    test.next_move_result = A4_MOVE_LANDING_PENDING;

    (void)memset(&hooks, 0, sizeof(hooks));
    hooks.context = &test;
    hooks.run_pre_roll_action = test_pre_action;
    hooks.roll_and_move = test_roll_and_move;
    hooks.on_state_changed = test_state_changed;
    hooks.on_player_skipped = test_player_skipped;
    hooks.on_notice = test_notice;
    hooks.on_game_finished = test_game_finished;

    CHECK(a4_turn_manager_init(&manager, players, 3U, &hooks) == A4_TURN_OK);
    CHECK(a4_turn_manager_begin(&manager) == A4_TURN_OK);

    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(snapshot.current_player_id == 101U);
    CHECK(snapshot.current_player_index == 0U);
    CHECK(snapshot.turn_number == 1U);
    CHECK(snapshot.round_number == 1U);
    CHECK(snapshot.phase == A4_TURN_PHASE_PRE_ROLL);
    CHECK((snapshot.available_operations & A4_OP_ROLL) != 0U);

    action.kind = A4_PRE_ACTION_PLACE_BLOCK;
    action.value = 2;
    action.text = NULL;
    CHECK(a4_turn_manager_run_pre_roll_action(&manager, 102U, &action) ==
          A4_TURN_ERR_WRONG_PLAYER);
    CHECK(test.pre_action_calls == 0);
    CHECK(a4_turn_manager_run_pre_roll_action(&manager, 101U, &action) ==
          A4_TURN_OK);
    CHECK(test.pre_action_calls == 1);

    CHECK(a4_turn_manager_roll(&manager, 102U, 0) ==
          A4_TURN_ERR_WRONG_PLAYER);
    CHECK(a4_turn_manager_roll(&manager, 101U, 0) == A4_TURN_OK);
    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(snapshot.phase == A4_TURN_PHASE_RESOLVING_LANDING);
    CHECK(snapshot.has_rolled);
    CHECK(snapshot.last_roll_steps == 4);
    CHECK((snapshot.available_operations & A4_OP_PRE_ROLL_ACTION) == 0U);
    CHECK((snapshot.available_operations & A4_OP_RESOLVE_LANDING) != 0U);
    CHECK(a4_turn_manager_roll(&manager, 101U, 0) ==
          A4_TURN_ERR_ALREADY_ROLLED);
    CHECK(a4_turn_manager_run_pre_roll_action(&manager, 101U, &action) ==
          A4_TURN_ERR_ACTION_NOT_ALLOWED);
    CHECK(a4_turn_manager_request_end_turn(&manager, 101U) ==
          A4_TURN_ERR_EVENT_PENDING);

    CHECK(a4_turn_manager_set_skip(
              &manager,
              102U,
              A4_SKIP_PRISON,
              1U,
              "等待释放") == A4_TURN_OK);
    CHECK(a4_turn_manager_complete_landing(&manager, 101U, false) ==
          A4_TURN_OK);

    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(test.skip_calls == 1);
    CHECK(test.last_skip_before == 1U);
    CHECK(test.last_skip_after == 0U);
    CHECK(snapshot.current_player_id == 103U);
    CHECK(snapshot.turn_number == 3U);
    CHECK(snapshot.round_number == 1U);
    CHECK(snapshot.phase == A4_TURN_PHASE_PRE_ROLL);

    test.next_move_result = A4_MOVE_RESOLVED;
    CHECK(a4_turn_manager_roll(&manager, 103U, 7) == A4_TURN_OK);
    CHECK(test.last_forced_steps == 7);
    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(snapshot.current_player_id == 101U);
    CHECK(snapshot.turn_number == 4U);
    CHECK(snapshot.round_number == 2U);

    test.next_move_result = A4_MOVE_FAILED;
    CHECK(a4_turn_manager_roll(&manager, 101U, 0) ==
          A4_TURN_ERR_CALLBACK_FAILED);
    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(snapshot.current_player_id == 101U);
    CHECK(snapshot.phase == A4_TURN_PHASE_PRE_ROLL);
    CHECK(!snapshot.has_rolled);

    CHECK(a4_turn_manager_mark_player_out(&manager, 102U) == A4_TURN_OK);
    CHECK(a4_turn_manager_mark_player_out(&manager, 103U) == A4_TURN_OK);
    snapshot = a4_turn_manager_snapshot(&manager);
    CHECK(snapshot.phase == A4_TURN_PHASE_FINISHED);
    CHECK(test.finished_calls == 1);
    CHECK(test.winner_id == 101U);
    CHECK(a4_turn_manager_roll(&manager, 101U, 0) == A4_TURN_ERR_FINISHED);
    return 0;
}

static int test_missing_integration_hook(void)
{
    A4TurnManager manager;
    const A4PlayerConfig players[] = {
        {1U, "钱夫人"},
        {2U, "阿土伯"}
    };

    CHECK(a4_turn_manager_init(&manager, players, 2U, NULL) == A4_TURN_OK);
    CHECK(a4_turn_manager_begin(&manager) == A4_TURN_OK);
    CHECK(a4_turn_manager_roll(&manager, 1U, 0) ==
          A4_TURN_ERR_CALLBACK_MISSING);
    CHECK(a4_turn_manager_snapshot(&manager).phase ==
          A4_TURN_PHASE_PRE_ROLL);
    return 0;
}

int main(void)
{
    if (test_validation() != 0) {
        return 1;
    }
    if (test_full_turn_flow() != 0) {
        return 1;
    }
    if (test_missing_integration_hook() != 0) {
        return 1;
    }

    (void)printf("A4 turn manager tests passed.\n");
    return 0;
}
