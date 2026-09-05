#ifndef A4_TURN_MANAGER_H
#define A4_TURN_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define A4_MIN_PLAYERS 2U
#define A4_MAX_PLAYERS 4U
#define A4_ROLE_NAME_CAPACITY 32U
#define A4_SKIP_NOTE_CAPACITY 64U
#define A4_MAX_SKIP_TURNS 1000U

typedef uint32_t A4PlayerId;

typedef enum A4TurnStatus {
    A4_TURN_OK = 0,
    A4_TURN_ERR_INVALID_ARGUMENT,
    A4_TURN_ERR_PLAYER_COUNT,
    A4_TURN_ERR_DUPLICATE_PLAYER,
    A4_TURN_ERR_NOT_STARTED,
    A4_TURN_ERR_ALREADY_STARTED,
    A4_TURN_ERR_FINISHED,
    A4_TURN_ERR_PLAYER_NOT_FOUND,
    A4_TURN_ERR_WRONG_PLAYER,
    A4_TURN_ERR_ACTION_NOT_ALLOWED,
    A4_TURN_ERR_ALREADY_ROLLED,
    A4_TURN_ERR_EVENT_PENDING,
    A4_TURN_ERR_ROLL_REQUIRED,
    A4_TURN_ERR_CALLBACK_MISSING,
    A4_TURN_ERR_CALLBACK_FAILED,
    A4_TURN_ERR_INVALID_SKIP,
    A4_TURN_ERR_NO_ACTIVE_PLAYER
} A4TurnStatus;

typedef enum A4TurnPhase {
    A4_TURN_PHASE_NOT_STARTED = 0,
    A4_TURN_PHASE_PRE_ROLL,
    A4_TURN_PHASE_MOVING,
    A4_TURN_PHASE_RESOLVING_LANDING,
    A4_TURN_PHASE_SKIPPING,
    A4_TURN_PHASE_FINISHED
} A4TurnPhase;

typedef enum A4SkipReason {
    A4_SKIP_NONE = 0,
    A4_SKIP_OTHER
} A4SkipReason;

/*
 * 可执行操作位图。UI/A5 模块只读取该位图，不应自行推导回合阶段。
 * 这样在回合切换后，提示、地图高亮和命令列表可以使用同一份快照刷新。
 */
typedef enum A4AvailableOperation {
    A4_OP_NONE = 0,
    A4_OP_PRE_ROLL_ACTION = 1U << 0,
    A4_OP_ROLL = 1U << 1,
    A4_OP_RESOLVE_LANDING = 1U << 2,
    A4_OP_QUERY = 1U << 3,
    A4_OP_HELP = 1U << 4,
    A4_OP_QUIT = 1U << 5
} A4AvailableOperation;

typedef enum A4PreRollActionKind {
    A4_PRE_ACTION_SELL_PROPERTY = 0,
    A4_PRE_ACTION_PLACE_BLOCK,
    A4_PRE_ACTION_USE_ROBOT,
    A4_PRE_ACTION_CUSTOM
} A4PreRollActionKind;

typedef struct A4PreRollAction {
    A4PreRollActionKind kind;
    int value;
    const char *text;
} A4PreRollAction;

typedef struct A4PlayerConfig {
    A4PlayerId id;
    const char *role_name;
} A4PlayerConfig;

typedef struct A4TurnSnapshot {
    A4TurnPhase phase;
    size_t player_count;
    size_t current_player_index;
    A4PlayerId current_player_id;
    const char *current_role_name;
    uint64_t turn_number;
    uint64_t round_number;
    bool has_rolled;
    int last_roll_steps;
    uint32_t available_operations;
    A4SkipReason skip_reason;
    uint16_t skip_turns_remaining;
    const char *skip_note;
} A4TurnSnapshot;

typedef enum A4MoveResult {
    /* A8 已完成移动及落地处理，A4 可立即进入下一名玩家的回合。 */
    A4_MOVE_RESOLVED = 0,
    /* 落地后仍需买地/交租/特殊事件输入；完成后必须调用 complete_landing。 */
    A4_MOVE_LANDING_PENDING,
    /* 移动导致游戏结束。 */
    A4_MOVE_GAME_OVER,
    /* A8 执行失败；A4 回滚为可再次掷骰的状态。 */
    A4_MOVE_FAILED
} A4MoveResult;

typedef enum A4StateChange {
    A4_STATE_TURN_STARTED = 0,
    A4_STATE_PHASE_CHANGED,
    A4_STATE_EXTERNAL_STATE_UPDATED,
    A4_STATE_PLAYER_SKIPPED,
    A4_STATE_TURN_ADVANCED,
    A4_STATE_GAME_FINISHED
} A4StateChange;

typedef struct A4TurnHooks {
    void *context;

    /*
     * 预留给卖房、路障、机器娃娃等模块。
     * 返回 false 表示操作失败；失败不会改变 A4 的回合阶段。
     */
    bool (*run_pre_roll_action)(
        void *context,
        const A4TurnSnapshot *snapshot,
        const A4PreRollAction *action
    );

    /*
     * A8 掷骰/移动接口。forced_steps < 0 表示普通 Roll；>= 0 表示测试命令 Step n（0 为原地）。
     * actual_steps 必须写入实际移动步数。
     */
    A4MoveResult (*roll_and_move)(
        void *context,
        const A4TurnSnapshot *snapshot,
        int forced_steps,
        int *actual_steps
    );

    /*
     * A5/UI 的统一同步点。回调收到的快照已是最新状态，应在这里一起刷新：
     * 当前玩家提示、地图当前玩家高亮、可执行命令列表。
     */
    void (*on_state_changed)(
        void *context,
        A4StateChange change,
        const A4TurnSnapshot *snapshot
    );

    /* 通用跳过状态的提示接口。当前地图规则不会产生轮空状态。 */
    void (*on_player_skipped)(
        void *context,
        const A4TurnSnapshot *snapshot,
        A4SkipReason reason,
        uint16_t remaining_after_skip,
        const char *note
    );

    /* UI 可直接显示 status 对应的稳定错误原因。detail 可为空。 */
    void (*on_notice)(
        void *context,
        A4TurnStatus status,
        const char *detail,
        const A4TurnSnapshot *snapshot
    );

    /* 预留给破产/胜利模块；winner_id == 0 表示无可用玩家或强制结束。 */
    void (*on_game_finished)(
        void *context,
        A4PlayerId winner_id,
        const A4TurnSnapshot *snapshot
    );
} A4TurnHooks;

typedef struct A4PlayerState {
    A4PlayerId id;
    char role_name[A4_ROLE_NAME_CAPACITY];
    bool participating;
    A4SkipReason skip_reason;
    uint16_t skip_turns_remaining;
    char skip_note[A4_SKIP_NOTE_CAPACITY];
} A4PlayerState;

/*
 * 为便于主程序静态分配，结构体在头文件中公开；业务模块请通过 API 修改状态，
 * 不要直接写入字段。
 */
typedef struct A4TurnManager {
    A4PlayerState players[A4_MAX_PLAYERS];
    size_t player_count;
    size_t current_player_index;
    uint64_t turn_number;
    uint64_t round_number;
    A4TurnPhase phase;
    bool started;
    bool has_rolled;
    int last_roll_steps;
    A4TurnHooks hooks;
} A4TurnManager;

A4TurnStatus a4_turn_manager_init(
    A4TurnManager *manager,
    const A4PlayerConfig *players,
    size_t player_count,
    const A4TurnHooks *hooks
);

A4TurnStatus a4_turn_manager_begin(A4TurnManager *manager);

/* 从指定玩家开始回合（不触发轮空），供自动化预设直接指定当前玩家时使用。 */
A4TurnStatus a4_turn_manager_begin_at(A4TurnManager *manager, size_t start_index);

/* 跳过当前玩家的本次回合，并推进到下一名玩家。 */
A4TurnStatus a4_turn_manager_skip_current(A4TurnManager *manager);

/* 自动化 STATE 测试专用：不移动、不处理落点，直接结束当前回合。 */
A4TurnStatus a4_turn_manager_advance_for_test(A4TurnManager *manager);

A4TurnStatus a4_turn_manager_run_pre_roll_action(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    const A4PreRollAction *action
);

/* forced_steps < 0: 普通 Roll；>= 0: 可测试的 Step n（0 为原地）。 */
A4TurnStatus a4_turn_manager_roll(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    int forced_steps
);

/* 仅在 A4_MOVE_LANDING_PENDING 后调用；成功后自动进入下一名玩家回合。 */
A4TurnStatus a4_turn_manager_complete_landing(
    A4TurnManager *manager,
    A4PlayerId actor_id,
    bool game_over
);

/*
 * 供命令层处理显式 End 请求。A4 当前流程为自动换人，因此此接口主要用于
 * 给出“尚未掷骰”或“事件未完成”的明确拒绝原因。
 */
A4TurnStatus a4_turn_manager_request_end_turn(
    A4TurnManager *manager,
    A4PlayerId actor_id
);

/* 设置下一次轮到该玩家时需要跳过的次数；turns == 0 清除状态。 */
A4TurnStatus a4_turn_manager_set_skip(
    A4TurnManager *manager,
    A4PlayerId player_id,
    A4SkipReason reason,
    uint16_t turns,
    const char *note
);

/* 预留给破产模块；最后仅剩一名参与者时自动结束游戏。 */
A4TurnStatus a4_turn_manager_mark_player_out(
    A4TurnManager *manager,
    A4PlayerId player_id
);

/* 预留给 Quit/主控制器的强制结束接口。 */
A4TurnStatus a4_turn_manager_finish(
    A4TurnManager *manager,
    A4PlayerId winner_id
);

A4TurnSnapshot a4_turn_manager_snapshot(const A4TurnManager *manager);
uint32_t a4_turn_manager_available_operations(const A4TurnManager *manager);
const char *a4_turn_status_string(A4TurnStatus status);
const char *a4_skip_reason_string(A4SkipReason reason);
const char *a4_turn_phase_string(A4TurnPhase phase);

#ifdef __cplusplus
}
#endif

#endif
