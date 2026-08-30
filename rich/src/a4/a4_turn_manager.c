#include "a4/a4_turn_manager.h"

#include <stdio.h>
#include <string.h>

static void a4_copy_text(char *destination, size_t capacity, const char *source)
{
    if (destination == NULL || capacity == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    (void)snprintf(destination, capacity, "%s", source);
}

static bool a4_find_player_index(
    const A4TurnManager *manager,
    A4PlayerId player_id,
    size_t *out_index
)
{
    size_t index;

    if (manager == NULL) {
        return false;
    }

    for (index = 0U; index < manager->player_count; ++index) {
        if (manager->players[index].id == player_id) {
            if (out_index != NULL) {
                *out_index = index;
            }
            return true;
        }
    }

    return false;
}

static size_t a4_participating_player_count(const A4TurnManager *manager)
{
    size_t count = 0U;
    size_t index;

    if (manager == NULL) {
        return 0U;
    }

    for (index = 0U; index < manager->player_count; ++index) {
        if (manager->players[index].participating) {
            ++count;
        }
    }
    return count;
}

static A4PlayerId a4_single_participating_player(const A4TurnManager *manager)
{
    size_t index;

    if (manager == NULL) {
        return 0U;
    }

    for (index = 0U; index < manager->player_count; ++index) {
        if (manager->players[index].participating) {
            return manager->players[index].id;
        }
    }
    return 0U;
}

uint32_t a4_turn_manager_available_operations(const A4TurnManager *manager)
{
    if (manager == NULL || !manager->started) {
        return A4_OP_NONE;
    }

    switch (manager->phase) {
        case A4_TURN_PHASE_PRE_ROLL:
            return A4_OP_PRE_ROLL_ACTION |
                   A4_OP_ROLL |
                   A4_OP_QUERY |
                   A4_OP_HELP |
                   A4_OP_QUIT;
        case A4_TURN_PHASE_MOVING:
            return A4_OP_QUERY | A4_OP_HELP | A4_OP_QUIT;
        case A4_TURN_PHASE_RESOLVING_LANDING:
            return A4_OP_RESOLVE_LANDING |
                   A4_OP_QUERY |
                   A4_OP_HELP |
                   A4_OP_QUIT;
        case A4_TURN_PHASE_NOT_STARTED:
        case A4_TURN_PHASE_SKIPPING:
        case A4_TURN_PHASE_FINISHED:
        default:
            return A4_OP_NONE;
    }
}

A4TurnSnapshot a4_turn_manager_snapshot(const A4TurnManager *manager)
{
    A4TurnSnapshot snapshot;

    (void)memset(&snapshot, 0, sizeof(snapshot));
    snapshot.phase = A4_TURN_PHASE_NOT_STARTED;

    if (manager == NULL) {
        return snapshot;
    }

    snapshot.phase = manager->phase;
    snapshot.player_count = manager->player_count;
    snapshot.current_player_index = manager->current_player_index;
    snapshot.turn_number = manager->turn_number;
    snapshot.round_number = manager->round_number;
    snapshot.has_rolled = manager->has_rolled;
    snapshot.last_roll_steps = manager->last_roll_steps;
    snapshot.available_operations = a4_turn_manager_available_operations(manager);

    if (manager->player_count > 0U &&
        manager->current_player_index < manager->player_count) {
        const A4PlayerState *player =
            &manager->players[manager->current_player_index];
        snapshot.current_player_id = player->id;
        snapshot.current_role_name = player->role_name;
        snapshot.skip_reason = player->skip_reason;
        snapshot.skip_turns_remaining = player->skip_turns_remaining;
        snapshot.skip_note = player->skip_note;
    }

    return snapshot;
}

static void a4_emit_state_changed(
    A4TurnManager *manager,
    A4StateChange change
)
{
    A4TurnSnapshot snapshot;

    if (manager == NULL || manager->hooks.on_state_changed == NULL) {
        return;
    }

    snapshot = a4_turn_manager_snapshot(manager);
    manager->hooks.on_state_changed(
        manager->hooks.context,
        change,
        &snapshot
    );
}

static void a4_emit_notice(
    A4TurnManager *manager,
    A4TurnStatus status,
    const char *detail
)
{
    A4TurnSnapshot snapshot;

    if (manager == NULL || manager->hooks.on_notice == NULL) {
        return;
    }

    snapshot = a4_turn_manager_snapshot(manager);
    manager->hooks.on_notice(
        manager->hooks.context,
        status,
        detail,
        &snapshot
    );
}

static A4TurnStatus a4_fail(
    A4TurnManager *manager,
    A4TurnStatus status,
    const char *detail
)
{
    a4_emit_notice(manager, status, detail);
    return status;
}

static A4TurnStatus a4_validate_running(A4TurnManager *manager)
{
    if (manager == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (!manager->started) {
        return a4_fail(manager, A4_TURN_ERR_NOT_STARTED, NULL);
    }
    if (manager->phase == A4_TURN_PHASE_FINISHED) {
        return a4_fail(manager, A4_TURN_ERR_FINISHED, NULL);
    }
    return A4_TURN_OK;
}

static A4TurnStatus a4_validate_current_actor(
    A4TurnManager *manager,
    A4PlayerId actor_id
)
{
    A4TurnStatus status;
    size_t actor_index;

    status = a4_validate_running(manager);
    if (status != A4_TURN_OK) {
        return status;
    }

    if (!a4_find_player_index(manager, actor_id, &actor_index) ||
        !manager->players[actor_index].participating) {
        return a4_fail(manager, A4_TURN_ERR_PLAYER_NOT_FOUND, NULL);
    }

    if (actor_index != manager->current_player_index) {
        return a4_fail(
            manager,
            A4_TURN_ERR_WRONG_PLAYER,
            "只有当前玩家可以执行会改变游戏状态的操作"
        );
    }

    return A4_TURN_OK;
}

static A4TurnStatus a4_finish_internal(
    A4TurnManager *manager,
    A4PlayerId winner_id
)
{
    A4TurnSnapshot snapshot;

    manager->phase = A4_TURN_PHASE_FINISHED;
    manager->has_rolled = false;
    manager->last_roll_steps = 0;
    a4_emit_state_changed(manager, A4_STATE_GAME_FINISHED);

    if (manager->hooks.on_game_finished != NULL) {
        snapshot = a4_turn_manager_snapshot(manager);
        manager->hooks.on_game_finished(
            manager->hooks.context,
            winner_id,
            &snapshot
        );
    }
    return A4_TURN_OK;
}

static bool a4_move_to_next_participating_player(A4TurnManager *manager)
{
    size_t old_index;
    size_t offset;

    old_index = manager->current_player_index;
    for (offset = 1U; offset <= manager->player_count; ++offset) {
        size_t candidate = (old_index + offset) % manager->player_count;
        if (manager->players[candidate].participating) {
            manager->current_player_index = candidate;
            ++manager->turn_number;
            if (candidate <= old_index) {
                ++manager->round_number;
            }
            return true;
        }
    }
    return false;
}

static A4TurnStatus a4_start_or_skip_current_turn(
    A4TurnManager *manager,
    A4StateChange active_change
)
{
    for (;;) {
        A4PlayerState *player;

        if (a4_participating_player_count(manager) == 0U) {
            return a4_finish_internal(manager, 0U);
        }

        player = &manager->players[manager->current_player_index];
        manager->has_rolled = false;
        manager->last_roll_steps = 0;

        if (player->skip_turns_remaining == 0U) {
            manager->phase = A4_TURN_PHASE_PRE_ROLL;
            a4_emit_state_changed(manager, active_change);
            return A4_TURN_OK;
        }

        manager->phase = A4_TURN_PHASE_SKIPPING;
        a4_emit_state_changed(manager, A4_STATE_PLAYER_SKIPPED);

        {
            A4TurnSnapshot snapshot = a4_turn_manager_snapshot(manager);
            const uint16_t remaining_after_skip =
                (uint16_t)(player->skip_turns_remaining - 1U);

            if (manager->hooks.on_player_skipped != NULL) {
                manager->hooks.on_player_skipped(
                    manager->hooks.context,
                    &snapshot,
                    player->skip_reason,
                    remaining_after_skip,
                    player->skip_note
                );
            }

            player->skip_turns_remaining = remaining_after_skip;
            if (remaining_after_skip == 0U) {
                player->skip_reason = A4_SKIP_NONE;
                player->skip_note[0] = '\0';
            }
        }

        if (!a4_move_to_next_participating_player(manager)) {
            return a4_finish_internal(manager, 0U);
        }
        active_change = A4_STATE_TURN_ADVANCED;
    }
}

static A4TurnStatus a4_advance_turn(A4TurnManager *manager)
{
    const size_t participating = a4_participating_player_count(manager);

    if (participating == 0U) {
        return a4_finish_internal(manager, 0U);
    }
    if (participating == 1U) {
        return a4_finish_internal(
            manager,
            a4_single_participating_player(manager)
        );
    }
    if (!a4_move_to_next_participating_player(manager)) {
        return a4_finish_internal(manager, 0U);
    }
    return a4_start_or_skip_current_turn(manager, A4_STATE_TURN_ADVANCED);
}

A4TurnStatus a4_turn_manager_init(
    A4TurnManager *manager,
    const A4PlayerConfig *players,
    size_t player_count,
    const A4TurnHooks *hooks
)
{
    size_t index;
    size_t previous;

    if (manager == NULL || players == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (player_count < A4_MIN_PLAYERS || player_count > A4_MAX_PLAYERS) {
        return A4_TURN_ERR_PLAYER_COUNT;
    }

    (void)memset(manager, 0, sizeof(*manager));
    manager->player_count = player_count;
    manager->phase = A4_TURN_PHASE_NOT_STARTED;
    if (hooks != NULL) {
        manager->hooks = *hooks;
    }

    for (index = 0U; index < player_count; ++index) {
        if (players[index].id == 0U ||
            players[index].role_name == NULL ||
            players[index].role_name[0] == '\0') {
            (void)memset(manager, 0, sizeof(*manager));
            return A4_TURN_ERR_INVALID_ARGUMENT;
        }

        for (previous = 0U; previous < index; ++previous) {
            if (players[previous].id == players[index].id) {
                (void)memset(manager, 0, sizeof(*manager));
                return A4_TURN_ERR_DUPLICATE_PLAYER;
            }
        }

        manager->players[index].id = players[index].id;
        manager->players[index].participating = true;
        manager->players[index].skip_reason = A4_SKIP_NONE;
        a4_copy_text(
            manager->players[index].role_name,
            sizeof(manager->players[index].role_name),
            players[index].role_name
        );
    }

    return A4_TURN_OK;
}

A4TurnStatus a4_turn_manager_begin(A4TurnManager *manager)
{
    if (manager == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (manager->started) {
        return a4_fail(manager, A4_TURN_ERR_ALREADY_STARTED, NULL);
    }
    if (a4_participating_player_count(manager) == 0U) {
        return a4_fail(manager, A4_TURN_ERR_NO_ACTIVE_PLAYER, NULL);
    }

    manager->started = true;
    manager->current_player_index = 0U;
    manager->turn_number = 1U;
    manager->round_number = 1U;
    return a4_start_or_skip_current_turn(manager, A4_STATE_TURN_STARTED);
}

A4TurnStatus a4_turn_manager_begin_at(A4TurnManager *manager, size_t start_index)
{
    if (manager == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (manager->started) {
        return a4_fail(manager, A4_TURN_ERR_ALREADY_STARTED, NULL);
    }
    if (a4_participating_player_count(manager) == 0U) {
        return a4_fail(manager, A4_TURN_ERR_NO_ACTIVE_PLAYER, NULL);
    }
    if (start_index >= manager->player_count) {
        return a4_fail(manager, A4_TURN_ERR_INVALID_ARGUMENT, "start_index");
    }

    manager->started = true;
    manager->current_player_index = start_index;
    manager->turn_number = 1U;
    manager->round_number = 1U;
    manager->has_rolled = false;
    manager->last_roll_steps = 0;
    manager->phase = A4_TURN_PHASE_PRE_ROLL;
    a4_emit_state_changed(manager, A4_STATE_TURN_STARTED);
    return A4_TURN_OK;
}

A4TurnStatus a4_turn_manager_skip_current(A4TurnManager *manager)
{
    if (manager == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (!manager->started) {
        return a4_fail(manager, A4_TURN_ERR_NOT_STARTED, NULL);
    }
    return a4_start_or_skip_current_turn(manager, A4_STATE_TURN_ADVANCED);
}

A4TurnStatus a4_turn_manager_run_pre_roll_action(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    const A4PreRollAction *action
)
{
    A4TurnStatus status = a4_validate_current_actor(manager, actor_id);
    A4TurnSnapshot snapshot;

    if (status != A4_TURN_OK) {
        return status;
    }
    if (action == NULL) {
        return a4_fail(manager, A4_TURN_ERR_INVALID_ARGUMENT, NULL);
    }
    if (manager->phase != A4_TURN_PHASE_PRE_ROLL || manager->has_rolled) {
        return a4_fail(
            manager,
            A4_TURN_ERR_ACTION_NOT_ALLOWED,
            "道具放置、卖房等操作只能在本回合掷骰之前执行"
        );
    }
    if (manager->hooks.run_pre_roll_action == NULL) {
        return a4_fail(manager, A4_TURN_ERR_CALLBACK_MISSING, "pre-roll action");
    }

    snapshot = a4_turn_manager_snapshot(manager);
    if (!manager->hooks.run_pre_roll_action(
            manager->hooks.context,
            &snapshot,
            action)) {
        return a4_fail(manager, A4_TURN_ERR_CALLBACK_FAILED, "pre-roll action");
    }

    a4_emit_state_changed(manager, A4_STATE_EXTERNAL_STATE_UPDATED);
    return A4_TURN_OK;
}

A4TurnStatus a4_turn_manager_roll(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    int forced_steps
)
{
    A4TurnStatus status = a4_validate_current_actor(manager, actor_id);
    A4TurnSnapshot snapshot;
    A4MoveResult move_result;
    int actual_steps = 0;

    if (status != A4_TURN_OK) {
        return status;
    }
    if (forced_steps < -1) {
        return a4_fail(manager, A4_TURN_ERR_INVALID_ARGUMENT, "forced_steps");
    }
    if (manager->has_rolled ||
        manager->phase == A4_TURN_PHASE_RESOLVING_LANDING) {
        return a4_fail(manager, A4_TURN_ERR_ALREADY_ROLLED, NULL);
    }
    if (manager->phase != A4_TURN_PHASE_PRE_ROLL) {
        return a4_fail(manager, A4_TURN_ERR_ACTION_NOT_ALLOWED, NULL);
    }
    if (manager->hooks.roll_and_move == NULL) {
        return a4_fail(manager, A4_TURN_ERR_CALLBACK_MISSING, "roll_and_move/A8");
    }

    manager->phase = A4_TURN_PHASE_MOVING;
    manager->has_rolled = true;
    manager->last_roll_steps = 0;
    a4_emit_state_changed(manager, A4_STATE_PHASE_CHANGED);

    snapshot = a4_turn_manager_snapshot(manager);
    move_result = manager->hooks.roll_and_move(
        manager->hooks.context,
        &snapshot,
        forced_steps,
        &actual_steps
    );

    if (move_result == A4_MOVE_FAILED ||
        move_result < A4_MOVE_RESOLVED ||
        move_result > A4_MOVE_FAILED ||
        actual_steps < 0) {
        manager->phase = A4_TURN_PHASE_PRE_ROLL;
        manager->has_rolled = false;
        manager->last_roll_steps = 0;
        a4_emit_state_changed(manager, A4_STATE_PHASE_CHANGED);
        return a4_fail(manager, A4_TURN_ERR_CALLBACK_FAILED, "roll_and_move/A8");
    }

    manager->last_roll_steps = actual_steps;
    if (move_result == A4_MOVE_LANDING_PENDING) {
        manager->phase = A4_TURN_PHASE_RESOLVING_LANDING;
        a4_emit_state_changed(manager, A4_STATE_PHASE_CHANGED);
        return A4_TURN_OK;
    }
    if (move_result == A4_MOVE_GAME_OVER) {
        return a4_finish_internal(manager, 0U);
    }

    return a4_advance_turn(manager);
}

A4TurnStatus a4_turn_manager_complete_landing(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    bool game_over
)
{
    A4TurnStatus status = a4_validate_current_actor(manager, actor_id);

    if (status != A4_TURN_OK) {
        return status;
    }
    if (manager->phase != A4_TURN_PHASE_RESOLVING_LANDING) {
        return a4_fail(
            manager,
            A4_TURN_ERR_ACTION_NOT_ALLOWED,
            "当前没有待完成的落地事件"
        );
    }

    if (game_over) {
        return a4_finish_internal(manager, 0U);
    }
    return a4_advance_turn(manager);
}

A4TurnStatus a4_turn_manager_request_end_turn(
    A4TurnManager *manager,
    A4PlayerId actor_id
)
{
    A4TurnStatus status = a4_validate_current_actor(manager, actor_id);

    if (status != A4_TURN_OK) {
        return status;
    }

    if (manager->phase == A4_TURN_PHASE_RESOLVING_LANDING) {
        return a4_fail(
            manager,
            A4_TURN_ERR_EVENT_PENDING,
            "落地事件尚未处理完成，不能结束回合"
        );
    }
    if (manager->phase == A4_TURN_PHASE_PRE_ROLL) {
        return a4_fail(
            manager,
            A4_TURN_ERR_ROLL_REQUIRED,
            "本回合尚未完成掷骰和移动"
        );
    }
    return a4_fail(
        manager,
        A4_TURN_ERR_ACTION_NOT_ALLOWED,
        "回合由 A4 在移动/事件处理完成后自动切换"
    );
}

A4TurnStatus a4_turn_manager_set_skip(
    A4TurnManager *manager,
    A4PlayerId player_id,
    A4SkipReason reason,
    uint16_t turns,
    const char *note
)
{
    size_t index;

    if (manager == NULL) {
        return A4_TURN_ERR_INVALID_ARGUMENT;
    }
    if (!a4_find_player_index(manager, player_id, &index)) {
        return a4_fail(manager, A4_TURN_ERR_PLAYER_NOT_FOUND, NULL);
    }
    if ((turns == 0U && reason != A4_SKIP_NONE) ||
        (turns > 0U && reason == A4_SKIP_NONE) ||
        turns > A4_MAX_SKIP_TURNS) {
        return a4_fail(manager, A4_TURN_ERR_INVALID_SKIP, NULL);
    }

    manager->players[index].skip_reason = reason;
    manager->players[index].skip_turns_remaining = turns;
    a4_copy_text(
        manager->players[index].skip_note,
        sizeof(manager->players[index].skip_note),
        note
    );

    if (manager->started && manager->phase != A4_TURN_PHASE_FINISHED) {
        a4_emit_state_changed(manager, A4_STATE_EXTERNAL_STATE_UPDATED);
    }
    return A4_TURN_OK;
}

A4TurnStatus a4_turn_manager_mark_player_out(
    A4TurnManager *manager,
    A4PlayerId player_id
)
{
    A4TurnStatus status = a4_validate_running(manager);
    size_t index;
    size_t participating;

    if (status != A4_TURN_OK) {
        return status;
    }
    if (!a4_find_player_index(manager, player_id, &index) ||
        !manager->players[index].participating) {
        return a4_fail(manager, A4_TURN_ERR_PLAYER_NOT_FOUND, NULL);
    }

    manager->players[index].participating = false;
    manager->players[index].skip_reason = A4_SKIP_NONE;
    manager->players[index].skip_turns_remaining = 0U;
    manager->players[index].skip_note[0] = '\0';
    participating = a4_participating_player_count(manager);

    if (participating <= 1U) {
        return a4_finish_internal(
            manager,
            a4_single_participating_player(manager)
        );
    }
    if (index == manager->current_player_index) {
        return a4_advance_turn(manager);
    }

    a4_emit_state_changed(manager, A4_STATE_EXTERNAL_STATE_UPDATED);
    return A4_TURN_OK;
}

A4TurnStatus a4_turn_manager_finish(
    A4TurnManager *manager,
    A4PlayerId winner_id
)
{
    A4TurnStatus status = a4_validate_running(manager);
    size_t winner_index;

    if (status != A4_TURN_OK) {
        return status;
    }
    if (winner_id != 0U &&
        (!a4_find_player_index(manager, winner_id, &winner_index) ||
         !manager->players[winner_index].participating)) {
        return a4_fail(manager, A4_TURN_ERR_PLAYER_NOT_FOUND, "winner_id");
    }
    return a4_finish_internal(manager, winner_id);
}

const char *a4_turn_status_string(A4TurnStatus status)
{
    switch (status) {
        case A4_TURN_OK:
            return "成功";
        case A4_TURN_ERR_INVALID_ARGUMENT:
            return "参数无效";
        case A4_TURN_ERR_PLAYER_COUNT:
            return "玩家数量必须为2至4";
        case A4_TURN_ERR_DUPLICATE_PLAYER:
            return "玩家编号不能重复";
        case A4_TURN_ERR_NOT_STARTED:
            return "回合系统尚未开始";
        case A4_TURN_ERR_ALREADY_STARTED:
            return "回合系统已经开始";
        case A4_TURN_ERR_FINISHED:
            return "游戏已经结束";
        case A4_TURN_ERR_PLAYER_NOT_FOUND:
            return "玩家不存在或已退出游戏";
        case A4_TURN_ERR_WRONG_PLAYER:
            return "只有当前玩家可以执行该操作";
        case A4_TURN_ERR_ACTION_NOT_ALLOWED:
            return "当前回合阶段不允许执行该操作";
        case A4_TURN_ERR_ALREADY_ROLLED:
            return "当前玩家本回合已经掷过骰子";
        case A4_TURN_ERR_EVENT_PENDING:
            return "落地事件尚未处理完成，不能结束回合";
        case A4_TURN_ERR_ROLL_REQUIRED:
            return "必须先完成掷骰和移动";
        case A4_TURN_ERR_CALLBACK_MISSING:
            return "对应功能接口尚未接入";
        case A4_TURN_ERR_CALLBACK_FAILED:
            return "外部功能执行失败，回合状态未改变";
        case A4_TURN_ERR_INVALID_SKIP:
            return "跳过回合参数无效";
        case A4_TURN_ERR_NO_ACTIVE_PLAYER:
            return "没有可参与回合的玩家";
        default:
            return "未知回合错误";
    }
}

const char *a4_skip_reason_string(A4SkipReason reason)
{
    switch (reason) {
        case A4_SKIP_NONE:
            return "无";
        case A4_SKIP_HOSPITAL:
            return "住院";
        case A4_SKIP_PRISON:
            return "入狱";
        case A4_SKIP_OTHER:
            return "其他";
        default:
            return "未知";
    }
}

const char *a4_turn_phase_string(A4TurnPhase phase)
{
    switch (phase) {
        case A4_TURN_PHASE_NOT_STARTED:
            return "未开始";
        case A4_TURN_PHASE_PRE_ROLL:
            return "掷骰前";
        case A4_TURN_PHASE_MOVING:
            return "移动中";
        case A4_TURN_PHASE_RESOLVING_LANDING:
            return "处理落地事件";
        case A4_TURN_PHASE_SKIPPING:
            return "跳过回合";
        case A4_TURN_PHASE_FINISHED:
            return "已结束";
        default:
            return "未知";
    }
}
