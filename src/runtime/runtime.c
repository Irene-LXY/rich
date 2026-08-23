/*
 * runtime.c —— 集成层运行时
 * 把 A4 回合管理、A5 地图、A8 掷骰移动串成一个可玩的大富翁最小循环。
 *
 * 职责：
 *   - 持有 GameMap、PlayerToken、资金、骰子、A4TurnManager；
 *   - 实现 A4 的 hooks（roll_and_move 即 A8 逻辑）；
 *   - 对外提供 roll/query/render/help 等命令入口。
 */
#include "monopoly/runtime.h"

#include "a4_turn_manager.h"
#include "map.h"
#include "game_interfaces.h"
#include "monopoly/query.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 角色表：Q/A/S/J，按玩家数量顺序分配。 */
static const char      *ROLE_NAMES[A4_MAX_PLAYERS]      = { "Q", "A", "S", "J" };
static const char       ROLE_SYMBOLS[A4_MAX_PLAYERS]    = { 'Q', 'A', 'S', 'J' };
static const ConsoleColor ROLE_COLORS[A4_MAX_PLAYERS]   = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW
};

#define NOTICE_CAPACITY 2048

struct GameRuntime {
    GameMap      map;
    PlayerToken  players[A4_MAX_PLAYERS];
    int          money[A4_MAX_PLAYERS];
    int          points[A4_MAX_PLAYERS];
    int          item_counts[A4_MAX_PLAYERS][QUERY_ITEM_TYPE_COUNT];
    int          fortune_turns[A4_MAX_PLAYERS];
    int          bankrupt[A4_MAX_PLAYERS];
    int          player_count;
    int          initial_money;
    RandomDice   dice;
    Dice         dice_iface;
    A4TurnManager turn_manager;
    A4TurnHooks   hooks;
    char         notice[NOTICE_CAPACITY];
};

/* ---- 输出缓冲 ---- */
static void notice_clear(GameRuntime *rt) {
    rt->notice[0] = '\0';
}

static void notice_append(GameRuntime *rt, const char *format, ...) {
    size_t used = strlen(rt->notice);
    va_list args;
    if (used >= sizeof(rt->notice) - 1) {
        return;
    }
    va_start(args, format);
    (void)vsnprintf(rt->notice + used, sizeof(rt->notice) - used, format, args);
    va_end(args);
}

/* ---- A4 hooks 实现 ---- */

/* 掷骰移动（A8 逻辑）。forced_steps>0 用于测试注入。 */
static A4MoveResult roll_and_move_impl(
    void *context,
    const A4TurnSnapshot *snapshot,
    int forced_steps,
    int *actual_steps
)
{
    GameRuntime *rt = (GameRuntime *)context;
    int player_id = (int)snapshot->current_player_id;
    int idx = player_id - 1;
    PlayerToken *token = &rt->players[idx];
    int steps = forced_steps > 0 ? forced_steps : dice_roll(&rt->dice_iface);
    MoveContext move_ctx;
    const MapCell *cell;

    if (actual_steps != NULL) {
        *actual_steps = steps;
    }

    if (forced_steps > 0) {
        notice_append(rt, "玩家 %s 使用遥控骰子，指定 %d 步（未使用随机点数）。\n",
                      token->name, steps);
    } else {
        notice_append(rt, "玩家 %s 掷出 %d 点。\n", token->name, steps);
    }

    move_ctx = move_player(&rt->map, token, steps, NULL, NULL);
    (void)move_ctx;

    notice_append(rt, "移动到位置 %d。\n", token->position);

    cell = game_map_cell_at(&rt->map, token->position);
    if (cell == NULL) {
        return A4_MOVE_RESOLVED;
    }

    switch (cell->type) {
        case CELL_START:
            notice_append(rt, "到达起点。\n");
            break;
        case CELL_LAND:
            if (cell->owner_id == RICH_NO_OWNER) {
                notice_append(rt, "到达无主空地 %d 号（价格 %d 元）。\n",
                              cell->index, cell->land_price);
            } else {
                notice_append(rt, "到达玩家 %d 的房产 %d 号。\n",
                              cell->owner_id, cell->index);
            }
            break;
        case CELL_HOSPITAL:
            notice_append(rt, "进入医院，住院休息 1 回合。\n");
            (void)a4_turn_manager_set_skip(
                &rt->turn_manager, (A4PlayerId)player_id,
                A4_SKIP_HOSPITAL, 1U, "住院");
            break;
        case CELL_PRISON:
            notice_append(rt, "进入监狱，被扣留 1 回合。\n");
            (void)a4_turn_manager_set_skip(
                &rt->turn_manager, (A4PlayerId)player_id,
                A4_SKIP_PRISON, 1U, "入狱");
            break;
        case CELL_MINE:
            rt->points[idx] += cell->mine_points;
            notice_append(rt, "到达矿地，获得 %d 点。\n", cell->mine_points);
            break;
        case CELL_TOOL_SHOP:
            notice_append(rt, "到达道具屋。\n");
            break;
        case CELL_GIFT_SHOP:
            notice_append(rt, "到达礼品屋。\n");
            break;
        case CELL_MAGIC_HOUSE:
            notice_append(rt, "到达魔法屋。\n");
            break;
        default:
            break;
    }

    return A4_MOVE_RESOLVED;
}

static void on_state_changed(
    void *context,
    A4StateChange change,
    const A4TurnSnapshot *snapshot
)
{
    GameRuntime *rt = (GameRuntime *)context;
    if (change == A4_STATE_TURN_STARTED || change == A4_STATE_TURN_ADVANCED) {
        notice_append(rt, "轮到玩家 %s（%d 号）。输入 Roll 掷骰子。\n",
                      snapshot->current_role_name,
                      (int)snapshot->current_player_id);
    }
}

static void on_notice(
    void *context,
    A4TurnStatus status,
    const char *detail,
    const A4TurnSnapshot *snapshot
)
{
    GameRuntime *rt = (GameRuntime *)context;
    (void)snapshot;
    notice_append(rt, "%s\n", a4_turn_status_string(status));
    if (detail != NULL && detail[0] != '\0') {
        notice_append(rt, "%s\n", detail);
    }
}

static void on_player_skipped(
    void *context,
    const A4TurnSnapshot *snapshot,
    A4SkipReason reason,
    uint16_t remaining_after_skip,
    const char *note
)
{
    GameRuntime *rt = (GameRuntime *)context;
    (void)remaining_after_skip;
    (void)note;
    notice_append(rt, "玩家 %s 因%s跳过本回合。\n",
                  snapshot->current_role_name, a4_skip_reason_string(reason));
}

static void on_game_finished(
    void *context,
    A4PlayerId winner_id,
    const A4TurnSnapshot *snapshot
)
{
    GameRuntime *rt = (GameRuntime *)context;
    (void)snapshot;
    notice_append(rt, "游戏结束。\n");
    if (winner_id != 0U) {
        notice_append(rt, "获胜玩家：%d 号。\n", (int)winner_id);
    }
}

/* ---- 生命周期 ---- */

GameRuntime *runtime_create(int player_count, int initial_money)
{
    GameRuntime *rt;
    A4PlayerConfig configs[A4_MAX_PLAYERS];
    int i;

    if (player_count < (int)A4_MIN_PLAYERS ||
        player_count > (int)A4_MAX_PLAYERS) {
        return NULL;
    }

    rt = (GameRuntime *)calloc(1, sizeof(*rt));
    if (rt == NULL) {
        return NULL;
    }

    game_map_init(&rt->map);
    random_dice_init(&rt->dice, 0U);
    rt->dice_iface = random_dice_as_interface(&rt->dice);
    rt->player_count = player_count;
    rt->initial_money = initial_money;

    for (i = 0; i < player_count; ++i) {
        rt->players[i].id = i + 1;
        rt->players[i].name = ROLE_NAMES[i];
        rt->players[i].symbol = ROLE_SYMBOLS[i];
        rt->players[i].color = ROLE_COLORS[i];
        rt->players[i].position = 0;
        rt->players[i].active = 1;
        rt->money[i] = initial_money;

        configs[i].id = (A4PlayerId)(i + 1);
        configs[i].role_name = ROLE_NAMES[i];
    }

    rt->hooks.context = rt;
    rt->hooks.roll_and_move = roll_and_move_impl;
    rt->hooks.on_state_changed = on_state_changed;
    rt->hooks.on_notice = on_notice;
    rt->hooks.on_player_skipped = on_player_skipped;
    rt->hooks.on_game_finished = on_game_finished;

    if (a4_turn_manager_init(&rt->turn_manager, configs, (size_t)player_count,
                             &rt->hooks) != A4_TURN_OK) {
        free(rt);
        return NULL;
    }

    return rt;
}

void runtime_destroy(GameRuntime *runtime)
{
    free(runtime);
}

/* ---- 命令入口 ---- */

int runtime_begin(GameRuntime *rt, char *message, size_t message_size)
{
    A4TurnStatus status;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    notice_clear(rt);
    status = a4_turn_manager_begin(&rt->turn_manager);
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

int runtime_roll(GameRuntime *rt, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    A4TurnStatus status;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    notice_clear(rt);
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    status = a4_turn_manager_roll(&rt->turn_manager, snapshot.current_player_id, 0);
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

int runtime_step(GameRuntime *rt, int steps, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    A4TurnStatus status;
    if (rt == NULL || message == NULL || message_size == 0U || steps <= 0) {
        return 1;
    }
    notice_clear(rt);
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    /* forced_steps>0时roll_and_move_impl直接采用该值，不调用dice_roll。 */
    status = a4_turn_manager_roll(
        &rt->turn_manager, snapshot.current_player_id, steps);
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

int runtime_query(GameRuntime *rt, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    QueryPlayerState state;
    int player_id;
    int idx;
    int position;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;
    if (idx < 0 || idx >= rt->player_count) {
        return 1;
    }
    (void)memset(&state, 0, sizeof(state));
    state.player_id = player_id;
    state.player_name = rt->players[idx].name;
    state.symbol = rt->players[idx].symbol;
    state.money = rt->money[idx];
    state.points = rt->points[idx];
    state.position = rt->players[idx].position;
    state.item_counts[QUERY_ITEM_BLOCK] = rt->item_counts[idx][QUERY_ITEM_BLOCK];
    state.item_counts[QUERY_ITEM_ROBOT] = rt->item_counts[idx][QUERY_ITEM_ROBOT];
    state.item_counts[QUERY_ITEM_BOMB] = rt->item_counts[idx][QUERY_ITEM_BOMB];
    state.fortune_turns = rt->fortune_turns[idx];
    state.bankrupt = rt->bankrupt[idx];
    if (snapshot.skip_reason == A4_SKIP_HOSPITAL) {
        state.hospital_turns = (int)snapshot.skip_turns_remaining;
    } else if (snapshot.skip_reason == A4_SKIP_PRISON) {
        state.prison_turns = (int)snapshot.skip_turns_remaining;
    }

    for (position = 0; position < RICH_MAP_SIZE; ++position) {
        const MapCell *cell = game_map_cell_at(&rt->map, position);
        if (cell != NULL && cell->type == CELL_LAND &&
            cell->owner_id == player_id &&
            state.property_count < QUERY_MAX_PROPERTIES) {
            QueryProperty *property = &state.properties[state.property_count++];
            property->position = position;
            property->land_price = cell->land_price;
            property->building_level = cell->building_level;
        }
    }
    return query_format_player(&state, message, message_size);
}

static int runtime_player_index(const GameRuntime *rt, int player_id)
{
    int idx = player_id - 1;
    if (rt == NULL || idx < 0 || idx >= rt->player_count) {
        return -1;
    }
    return idx;
}

int runtime_set_player_money(GameRuntime *rt, int player_id, int money)
{
    int idx = runtime_player_index(rt, player_id);
    if (idx < 0) return 1;
    rt->money[idx] = money;
    return 0;
}

int runtime_add_player_points(GameRuntime *rt, int player_id, int points)
{
    int idx = runtime_player_index(rt, player_id);
    if (idx < 0 || points < 0) return 1;
    rt->points[idx] += points;
    return 0;
}

int runtime_set_player_item_count(GameRuntime *rt,
                                  int player_id,
                                  QueryItemType item_type,
                                  int count)
{
    int idx = runtime_player_index(rt, player_id);
    int total = 0;
    int i;
    if (idx < 0 || item_type < QUERY_ITEM_BLOCK ||
        item_type >= QUERY_ITEM_TYPE_COUNT || count < 0) return 1;
    for (i = 0; i < QUERY_ITEM_TYPE_COUNT; ++i) {
        total += (i == (int)item_type) ? count : rt->item_counts[idx][i];
    }
    if (total > 10) return 1;
    rt->item_counts[idx][item_type] = count;
    return 0;
}

int runtime_set_player_fortune_turns(GameRuntime *rt,
                                     int player_id,
                                     int turns)
{
    int idx = runtime_player_index(rt, player_id);
    if (idx < 0 || turns < 0) return 1;
    rt->fortune_turns[idx] = turns;
    return 0;
}

int runtime_set_player_hospital_turns(GameRuntime *rt,
                                      int player_id,
                                      int turns)
{
    if (runtime_player_index(rt, player_id) < 0 || turns < 0 || turns > 1000) {
        return 1;
    }
    return a4_turn_manager_set_skip(
        &rt->turn_manager, (A4PlayerId)player_id,
        turns == 0 ? A4_SKIP_NONE : A4_SKIP_HOSPITAL,
        (uint16_t)turns, turns == 0 ? "" : "住院") == A4_TURN_OK ? 0 : 1;
}

int runtime_set_player_prison_turns(GameRuntime *rt,
                                    int player_id,
                                    int turns)
{
    if (runtime_player_index(rt, player_id) < 0 || turns < 0 || turns > 1000) {
        return 1;
    }
    return a4_turn_manager_set_skip(
        &rt->turn_manager, (A4PlayerId)player_id,
        turns == 0 ? A4_SKIP_NONE : A4_SKIP_PRISON,
        (uint16_t)turns, turns == 0 ? "" : "入狱") == A4_TURN_OK ? 0 : 1;
}

int runtime_set_player_bankrupt(GameRuntime *rt,
                                int player_id,
                                int bankrupt)
{
    int idx = runtime_player_index(rt, player_id);
    if (idx < 0) return 1;
    rt->bankrupt[idx] = bankrupt ? 1 : 0;
    return 0;
}

int runtime_assign_property(GameRuntime *rt,
                            int player_id,
                            int position,
                            int building_level)
{
    MapCell *cell;
    if (runtime_player_index(rt, player_id) < 0 ||
        building_level < 0 || building_level > 3) return 1;
    cell = game_map_cell_at_mut(&rt->map, position);
    if (cell == NULL || cell->type != CELL_LAND) return 1;
    cell->owner_id = player_id;
    cell->building_level = building_level;
    return 0;
}

int runtime_release_property(GameRuntime *rt, int position)
{
    MapCell *cell;
    if (rt == NULL) return 1;
    cell = game_map_cell_at_mut(&rt->map, position);
    if (cell == NULL || cell->type != CELL_LAND) return 1;
    cell->owner_id = RICH_NO_OWNER;
    cell->building_level = 0;
    return 0;
}

int runtime_render(GameRuntime *rt, char *message, size_t message_size)
{
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    return render_map(&rt->map, rt->players, (size_t)rt->player_count,
                      0, 1, message, message_size) ? 0 : 1;
}

int runtime_help(GameRuntime *rt, char *message, size_t message_size)
{
    (void)rt;
    if (message == NULL || message_size == 0) {
        return 1;
    }
    (void)snprintf(message, message_size,
        "可用命令：\n"
        "  Roll  掷骰子移动\n"
        "  Step n 遥控骰子指定任意正整数步数（不使用随机点数）\n"
        "  Query 查询资金、点数、房产、道具和状态剩余轮数\n"
        "  Map   显示地图\n"
        "  Help  显示本帮助\n"
        "  Quit  结束整局游戏\n");
    return 0;
}

const char *runtime_current_player_name(const GameRuntime *rt)
{
    A4TurnSnapshot snapshot;
    int idx;
    if (rt == NULL) {
        return "";
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    idx = (int)snapshot.current_player_id - 1;
    if (idx < 0 || idx >= rt->player_count) {
        return "";
    }
    return rt->players[idx].name;
}

int runtime_is_finished(const GameRuntime *rt)
{
    if (rt == NULL) {
        return 1;
    }
    return rt->turn_manager.phase == A4_TURN_PHASE_FINISHED;
}
