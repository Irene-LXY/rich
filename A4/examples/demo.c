#include "a4_turn_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static A4MoveResult demo_roll_and_move(
    void *context,
    const A4TurnSnapshot *snapshot,
    int forced_steps,
    int *actual_steps
)
{
    (void)context;
    *actual_steps = forced_steps > 0 ? forced_steps : (rand() % 6) + 1;
    (void)printf(
        "[A8预留接口] 玩家%u %s 掷出/行走 %d 步；本示例视为落地事件已处理。\n",
        snapshot->current_player_id,
        snapshot->current_role_name,
        *actual_steps
    );
    return A4_MOVE_RESOLVED;
}

static void demo_state_changed(
    void *context,
    A4StateChange change,
    const A4TurnSnapshot *snapshot
)
{
    (void)context;
    (void)change;
    (void)printf(
        "[A5/UI预留接口] 第%llu轮/第%llu个回合，当前玩家%u %s，阶段=%s，命令位图=0x%02X\n",
        (unsigned long long)snapshot->round_number,
        (unsigned long long)snapshot->turn_number,
        snapshot->current_player_id,
        snapshot->current_role_name,
        a4_turn_phase_string(snapshot->phase),
        (unsigned int)snapshot->available_operations
    );
}

static void demo_player_skipped(
    void *context,
    const A4TurnSnapshot *snapshot,
    A4SkipReason reason,
    uint16_t remaining_after_skip,
    const char *note
)
{
    (void)context;
    (void)printf(
        "玩家%u %s 因%s跳过本回合，剩余%u次。%s\n",
        snapshot->current_player_id,
        snapshot->current_role_name,
        a4_skip_reason_string(reason),
        (unsigned int)remaining_after_skip,
        note == NULL ? "" : note
    );
}

static void demo_notice(
    void *context,
    A4TurnStatus status,
    const char *detail,
    const A4TurnSnapshot *snapshot
)
{
    (void)context;
    (void)snapshot;
    (void)fprintf(
        stderr,
        "拒绝操作：%s%s%s\n",
        a4_turn_status_string(status),
        detail == NULL ? "" : "；",
        detail == NULL ? "" : detail
    );
}

int main(void)
{
    A4TurnManager manager;
    A4TurnHooks hooks = {0};
    A4TurnSnapshot snapshot;
    size_t completed_turns;
    const A4PlayerConfig players[] = {
        {1U, "钱夫人"},
        {2U, "阿土伯"},
        {3U, "孙小美"}
    };

    (void)srand((unsigned int)time(NULL));
    hooks.roll_and_move = demo_roll_and_move;
    hooks.on_state_changed = demo_state_changed;
    hooks.on_player_skipped = demo_player_skipped;
    hooks.on_notice = demo_notice;

    if (a4_turn_manager_init(&manager, players, 3U, &hooks) != A4_TURN_OK) {
        return EXIT_FAILURE;
    }
    /* 模拟阿土伯入狱一次；轮到他时会显示原因并自动换人。 */
    (void)a4_turn_manager_set_skip(
        &manager,
        2U,
        A4_SKIP_PRISON,
        1U,
        "等待释放"
    );
    if (a4_turn_manager_begin(&manager) != A4_TURN_OK) {
        return EXIT_FAILURE;
    }

    for (completed_turns = 0U; completed_turns < 6U; ++completed_turns) {
        snapshot = a4_turn_manager_snapshot(&manager);
        if (a4_turn_manager_roll(
                &manager,
                snapshot.current_player_id,
                0) != A4_TURN_OK) {
            return EXIT_FAILURE;
        }
    }

    (void)a4_turn_manager_finish(&manager, 0U);
    return EXIT_SUCCESS;
}
