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

#include "a4/a4_turn_manager.h"
#include "map/map.h"
#include "map/game_interfaces.h"

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
    unsigned int tools[A4_MAX_PLAYERS][4];
    unsigned int lucky_turns[A4_MAX_PLAYERS];
    int          last_shop_player;
    int          landing_pending;
    int          landing_kind; /* 1=道具屋，2=无主土地购买 */
    int          player_count;
    int          initial_money;
    RandomDice   dice;
    Dice         dice_iface;
    A4TurnManager turn_manager;
    A4TurnHooks   hooks;
    char         notice[NOTICE_CAPACITY];
};

static void notice_append(GameRuntime *rt, const char *format, ...);

typedef struct ToolMoveContext {
    GameRuntime *runtime;
    int player_index;
    int event;
} ToolMoveContext;

static int tool_move_handler(PlayerToken *player, const MapCell *cell,
                             const MoveContext *context, void *data)
{
    ToolMoveContext *move = (ToolMoveContext *)data;
    GameRuntime *rt;
    (void)player;
    (void)context;
    if (move == NULL || cell == NULL) return 1;
    rt = move->runtime;
    if (cell->has_block) {
        MapCell *mutable_cell = game_map_cell_at_mut(&rt->map, cell->index);
        mutable_cell->has_block = 0;
        move->event = 1;
        notice_append(rt, "踩到路障，停在 %d 号位置，路障已消失。\n", cell->index);
        return 0;
    }
    if (cell->has_bomb) {
        MapCell *mutable_cell = game_map_cell_at_mut(&rt->map, cell->index);
        mutable_cell->has_bomb = 0;
        player->position = 14;
        move->event = 2;
        notice_append(rt, "踩到炸弹，送往医院并住院 3 回合。\n");
        (void)a4_turn_manager_set_skip(&rt->turn_manager,
            (A4PlayerId)(move->player_index + 1), A4_SKIP_HOSPITAL, 3U, "炸弹住院");
        return 0;
    }
    return 1;
}

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
    ToolMoveContext tool_ctx;
    const MapCell *cell;

    if (actual_steps != NULL) {
        *actual_steps = steps;
    }

    notice_append(rt, "玩家 %s 掷出 %d 点。\n", token->name, steps);

    tool_ctx.runtime = rt;
    tool_ctx.player_index = idx;
    tool_ctx.event = 0;
    move_ctx = move_player(&rt->map, token, steps, tool_move_handler, &tool_ctx);
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
                rt->landing_pending = 1;
                rt->landing_kind = 2;
                return A4_MOVE_LANDING_PENDING;
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
            notice_append(rt, "到达矿地，获得 %d 点，当前点数 %d。\n",
                          cell->mine_points, rt->points[idx]);
            break;
        case CELL_TOOL_SHOP:
            rt->last_shop_player = idx;
            rt->landing_pending = 1;
            rt->landing_kind = 1;
            notice_append(rt, "到达道具屋。欢迎光临，请输入 1/2/3 选择道具：\n"
                          "1 路障（50点），2 机器娃娃（30点），3 炸弹（50点）。\n"
                          "总道具数上限10；当前点数 %d。\n", rt->points[idx]);
            return A4_MOVE_LANDING_PENDING;
        case CELL_GIFT_SHOP:
            rt->landing_pending = 1;
            rt->landing_kind = 3;
            notice_append(rt, "欢迎光临礼品屋，请选择一件礼物：\n"
                          "1 奖金（+2000元），2 点数卡（+200点），3 财神（5轮免过路费）。\n"
                          "请输入 1/2/3：\n");
            return A4_MOVE_LANDING_PENDING;
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
    rt->last_shop_player = -1;

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
    if (status == A4_TURN_OK && rt->landing_pending && rt->landing_kind == 1) {
        int idx = (int)snapshot.current_player_id - 1;
        if (rt->points[idx] < 30 ||
            rt->tools[idx][1] + rt->tools[idx][2] + rt->tools[idx][3] >= 10U) {
            rt->landing_pending = 0;
            rt->landing_kind = 0;
            rt->last_shop_player = -1;
            (void)a4_turn_manager_complete_landing(&rt->turn_manager,
                snapshot.current_player_id, false);
            notice_append(rt, "当前点数不足以购买任何道具或背包已满，道具屋自动退出。\n");
        }
    }
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

int runtime_step(GameRuntime *rt, int steps, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    A4TurnStatus status;
    if (rt == NULL || message == NULL || message_size == 0 || steps < 0) {
        return 1;
    }
    notice_clear(rt);
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    status = a4_turn_manager_roll(&rt->turn_manager, snapshot.current_player_id, steps);
    if (status == A4_TURN_OK && rt->landing_pending && rt->landing_kind == 1) {
        int idx = (int)snapshot.current_player_id - 1;
        if (rt->points[idx] < 30 ||
            rt->tools[idx][1] + rt->tools[idx][2] + rt->tools[idx][3] >= 10U) {
            rt->landing_pending = 0;
            rt->landing_kind = 0;
            rt->last_shop_player = -1;
            (void)a4_turn_manager_complete_landing(&rt->turn_manager,
                snapshot.current_player_id, false);
            notice_append(rt, "当前点数不足以购买任何道具或背包已满，道具屋自动退出。\n");
        }
    }
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

static int runtime_current_index(GameRuntime *rt)
{
    A4TurnSnapshot snapshot;
    if (rt == NULL) return -1;
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    if (snapshot.current_player_id < 1U ||
        snapshot.current_player_id > (A4PlayerId)rt->player_count) return -1;
    return (int)snapshot.current_player_id - 1;
}

static int runtime_at_tool_shop(GameRuntime *rt, int index)
{
    const MapCell *cell;
    if (rt == NULL || index < 0) return 0;
    cell = game_map_cell_at(&rt->map, rt->players[index].position);
    return cell != NULL && cell->type == CELL_TOOL_SHOP;
}

static int runtime_shop_index(GameRuntime *rt)
{
    int current = runtime_current_index(rt);
    if (rt != NULL && rt->last_shop_player >= 0 &&
        rt->last_shop_player < rt->player_count &&
        runtime_at_tool_shop(rt, rt->last_shop_player)) {
        return rt->last_shop_player;
    }
    return current;
}

int runtime_tool_shop(GameRuntime *rt, char *message, size_t message_size)
{
    int idx = runtime_shop_index(rt);
    if (message == NULL || message_size == 0 || idx < 0) return 1;
    if (!runtime_at_tool_shop(rt, idx)) {
        (void)snprintf(message, message_size, "当前不在道具屋，请先移动到 28 号位置。\n");
        return 1;
    }
    (void)snprintf(message, message_size,
        "道具屋：1 路障（50点），2 机器娃娃（30点），3 炸弹（50点）。\n"
        "当前点数：%d；库存：路障%u/机器娃娃%u/炸弹%u。\n",
        rt->points[idx], rt->tools[idx][1], rt->tools[idx][2], rt->tools[idx][3]);
    return 0;
}

int runtime_buy_tool(GameRuntime *rt, int tool, char *message, size_t message_size)
{
    return runtime_select_shop_item(rt, tool, message, message_size);
}

int runtime_select_shop_item(GameRuntime *rt, int tool, char *message, size_t message_size)
{
    int idx = runtime_shop_index(rt);
    int price;
    if (message == NULL || message_size == 0 || idx < 0 || tool < 1 || tool > 3) return 1;
    if (rt->landing_kind != 1 || !runtime_at_tool_shop(rt, idx)) {
        (void)snprintf(message, message_size, "购买道具必须在 28 号道具屋进行。\n");
        return 1;
    }
    price = tool == 2 ? 30 : 50;
    if (rt->points[idx] < price) {
        (void)snprintf(message, message_size, "点数不足：需要 %d 点，当前只有 %d 点。\n", price, rt->points[idx]);
        return 1;
    }
    if (rt->tools[idx][1] + rt->tools[idx][2] + rt->tools[idx][3] >= 10U) {
        (void)snprintf(message, message_size, "道具数量已达到上限 10。\n");
        return 1;
    }
    rt->points[idx] -= price;
    rt->tools[idx][tool]++;
    rt->landing_pending = 0;
    rt->landing_kind = 0;
    rt->last_shop_player = -1;
    (void)a4_turn_manager_complete_landing(&rt->turn_manager,
        (A4PlayerId)(idx + 1), false);
    (void)snprintf(message, message_size, "购买成功，剩余点数 %d。道具屋事件处理完成，进入下一玩家回合。\n",
                   rt->points[idx]);
    return 0;
}

int runtime_select_gift(GameRuntime *rt, int gift, char *message, size_t message_size)
{
    int idx;
    if (rt == NULL || message == NULL || message_size == 0 || gift < 1 || gift > 3) return 1;
    idx = runtime_current_index(rt);
    if (idx < 0 || !rt->landing_pending || rt->landing_kind != 3) {
        (void)snprintf(message, message_size, "当前不在礼品屋选择阶段。\n");
        return 1;
    }
    if (gift == 1) {
        rt->money[idx] += 2000;
        (void)snprintf(message, message_size, "获得奖金 2000 元，当前资金 %d 元。\n", rt->money[idx]);
    } else if (gift == 2) {
        rt->points[idx] += 200;
        (void)snprintf(message, message_size, "获得点数卡 200 点，当前点数 %d。\n", rt->points[idx]);
    } else {
        rt->lucky_turns[idx] = 5U;
        (void)snprintf(message, message_size, "获得财神，未来 5 轮经过他人地产免收过路费。\n");
    }
    rt->landing_pending = 0;
    rt->landing_kind = 0;
    (void)a4_turn_manager_complete_landing(&rt->turn_manager,
        (A4PlayerId)(idx + 1), false);
    return 0;
}

int runtime_resolve_landing(GameRuntime *rt, int accept, char *message, size_t message_size)
{
    int idx;
    MapCell *cell;
    if (rt == NULL || message == NULL || message_size == 0) return 1;
    idx = runtime_current_index(rt);
    if (!rt->landing_pending || idx < 0) {
        (void)snprintf(message, message_size, "当前没有等待处理的落点事件。\n");
        return 1;
    }
    if (accept) {
        if (rt->landing_kind == 1) {
            (void)snprintf(message, message_size, "请先输入 Shop 查看道具，再输入 Buy 1/2/3 购买。\n");
            return 1;
        }
        cell = game_map_cell_at_mut(&rt->map, rt->players[idx].position);
        if (cell == NULL || cell->type != CELL_LAND || cell->owner_id != RICH_NO_OWNER) {
            (void)snprintf(message, message_size, "当前落点不能购买。\n");
            return 1;
        }
        if (rt->money[idx] < cell->land_price) {
            (void)snprintf(message, message_size, "资金不足，无法购买 %d 号土地。当前资金 %d 元，需要 %d 元。\n",
                           cell->index, rt->money[idx], cell->land_price);
            return 1;
        }
        rt->money[idx] -= cell->land_price;
        cell->owner_id = idx + 1;
        cell->building_level = 0;
        rt->landing_pending = 0;
        rt->landing_kind = 0;
        (void)a4_turn_manager_complete_landing(&rt->turn_manager,
            (A4PlayerId)(idx + 1), false);
        (void)snprintf(message, message_size, "购买成功：%d 号土地归玩家 %s，剩余资金 %d 元。进入下一玩家回合。\n",
                       cell->index, rt->players[idx].name, rt->money[idx]);
        return 0;
    }
    rt->landing_pending = 0;
    rt->landing_kind = 0;
    rt->last_shop_player = -1;
    if (a4_turn_manager_complete_landing(&rt->turn_manager,
            (A4PlayerId)(idx + 1), false) != A4_TURN_OK) {
        (void)snprintf(message, message_size, "落点事件尚未完成，不能结束回合。\n");
        return 1;
    }
    (void)snprintf(message, message_size, "放弃当前落点处理，进入下一玩家回合。\n");
    return 0;
}

int runtime_place_tool(GameRuntime *rt, int tool, int distance,
                       char *message, size_t message_size)
{
    int idx = runtime_shop_index(rt);
    int target;
    MapCell *cell;
    if (message == NULL || message_size == 0 || idx < 0 || (tool != 1 && tool != 3)) return 1;
    if (rt->turn_manager.phase != A4_TURN_PHASE_PRE_ROLL) {
        (void)snprintf(message, message_size, "道具只能在本回合掷骰前使用。\n");
        return 1;
    }
    if (rt->tools[idx][tool] == 0U) {
        (void)snprintf(message, message_size, "当前玩家没有该道具。\n");
        return 1;
    }
    target = game_map_destination(rt->players[idx].position, distance);
    cell = game_map_cell_at_mut(&rt->map, target);
    {
        int i;
        for (i = 0; i < rt->player_count; ++i) {
            if (rt->players[i].active && rt->players[i].position == target) {
                (void)snprintf(message, message_size,
                    "目标位置已有玩家，不能在有人所在的位置放置道具。\n");
                return 1;
            }
        }
    }
    if (cell == NULL || cell->has_block || cell->has_bomb) {
        (void)snprintf(message, message_size, "目标位置已有道具，不能重复放置。\n");
        return 1;
    }
    if (tool == 1) cell->has_block = 1; else cell->has_bomb = 1;
    rt->tools[idx][tool]--;
    (void)snprintf(message, message_size, "%s已放置在 %d 号位置。\n",
        tool == 1 ? "路障" : "炸弹", target);
    return 0;
}

int runtime_sell_property(GameRuntime *rt, int position,
                          char *message, size_t message_size)
{
    int idx;
    MapCell *cell;
    int sale_value;
    if (rt == NULL || message == NULL || message_size == 0) return 1;
    idx = runtime_current_index(rt);
    if (idx < 0 || rt->turn_manager.phase != A4_TURN_PHASE_PRE_ROLL) {
        (void)snprintf(message, message_size, "房产只能在当前玩家掷骰前出售。\n");
        return 1;
    }
    if (position < 0 || position >= RICH_MAP_SIZE) {
        (void)snprintf(message, message_size, "房产编号必须为 0-69。\n");
        return 1;
    }
    cell = game_map_cell_at_mut(&rt->map, position);
    if (cell == NULL || cell->type != CELL_LAND) {
        (void)snprintf(message, message_size, "该位置不可出售。\n");
        return 1;
    }
    if (cell->owner_id != idx + 1) {
        (void)snprintf(message, message_size, "%d 号房产不属于当前玩家，不能出售。\n", position);
        return 1;
    }
    sale_value = cell->land_price * (cell->building_level + 1) * 2;
    cell->owner_id = RICH_NO_OWNER;
    cell->building_level = 0;
    rt->money[idx] += sale_value;
    (void)snprintf(message, message_size,
        "%d 号房产已售给银行，获得 %d 元（投资总成本的2倍），当前资金 %d 元。\n",
        position, sale_value, rt->money[idx]);
    return 0;
}

int runtime_use_robot(GameRuntime *rt, char *message, size_t message_size)
{
    int idx = runtime_shop_index(rt);
    int distance;
    unsigned int removed = 0U;
    if (message == NULL || message_size == 0 || idx < 0) return 1;
    if (rt->turn_manager.phase != A4_TURN_PHASE_PRE_ROLL || rt->tools[idx][2] == 0U) {
        (void)snprintf(message, message_size, "当前不能使用机器娃娃，或库存为空。\n");
        return 1;
    }
    for (distance = 1; distance <= 10; ++distance) {
        MapCell *cell = game_map_cell_at_mut(&rt->map,
            game_map_destination(rt->players[idx].position, distance));
        if (cell != NULL && (cell->has_block || cell->has_bomb)) {
            cell->has_block = 0;
            cell->has_bomb = 0;
            ++removed;
        }
    }
    rt->tools[idx][2]--;
    (void)snprintf(message, message_size, "机器娃娃已清除前方 10 步内的 %u 个道具。\n", removed);
    return 0;
}

int runtime_query(GameRuntime *rt, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    int player_id;
    int idx;
    char description[128];
    const MapCell *cell;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;
    cell = game_map_cell_at(&rt->map, rt->players[idx].position);
    if (cell != NULL) {
        (void)game_map_cell_description(&rt->map, cell->index,
                                        description, sizeof(description));
    } else {
        (void)snprintf(description, sizeof(description), "未知");
    }
    (void)snprintf(message, message_size,
        "玩家 %s（%d 号）：位置 %d（%s），资金 %d 元，点数 %d，道具[路障%u/机器娃娃%u/炸弹%u]，财神剩余 %u 轮。\n",
        rt->players[idx].name, player_id, rt->players[idx].position,
        description, rt->money[idx], rt->points[idx], rt->tools[idx][1],
        rt->tools[idx][2], rt->tools[idx][3], rt->lucky_turns[idx]);
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
        "  Step n 测试移动 n 步\n"
        "  Query 查询当前玩家状态\n"
        "  Sell n 将自己拥有的 n 号房产按投资总成本2倍出售给银行\n"
        "  Map   显示地图\n"
        "  Help  显示本帮助\n"
        "  Shop  在道具屋查看商品\n"
        "  Buy 1/2/3 购买路障/机器娃娃/炸弹\n"
        "  Block n / Bomb n 放置道具（-10 至 10）\n"
        "  Robot 清除前方 10 步道具\n"
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
