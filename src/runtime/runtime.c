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

#include "monopoly/gift.h"
#include "monopoly/character.h"

#include "a4/a4_turn_manager.h"
#include "map/map.h"
#include "map/game_interfaces.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 角色颜色映射：角色编号 1~4 对应 Q/A/S/J（红/绿/蓝/黄）。 */
static const ConsoleColor ROLE_COLORS[CHARACTER_COUNT] = {
    COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW
};

#define NOTICE_CAPACITY 2048
#define MAGIC_EFFECT_CAPACITY 16U

struct GameRuntime {
    GameMap      map;
    PlayerToken  players[A4_MAX_PLAYERS];
    int          money[A4_MAX_PLAYERS];
    int          player_count;
    int          initial_money;
    RandomDice   dice;
    Dice         dice_iface;
    A4TurnManager turn_manager;
    A4TurnHooks   hooks;
    GiftShopState gift_shop;
    MagicHouseState magic_house;
    RuntimeContext context;
    int          pending_position;   /* 待购买/升级的房产位置 */
    int          pending_price;      /* 待支付价格 */
    MagicEffect magic_effects[MAGIC_EFFECT_CAPACITY];
    size_t magic_effect_count;
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

    notice_append(rt, "玩家 %s 掷出 %d 点。\n", token->name, steps);

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
                rt->context = RUNTIME_CONTEXT_BUY_CONFIRM;
                rt->pending_position = token->position;
                rt->pending_price = cell->land_price;
                notice_append(rt,
                    "到达无主空地 %d 号（价格 %d 元）。是否购买？(Y/N)\n",
                    cell->index, cell->land_price);
                return A4_MOVE_LANDING_PENDING;
            } else if (cell->owner_id == player_id) {
                rt->context = RUNTIME_CONTEXT_UPGRADE_CONFIRM;
                rt->pending_position = token->position;
                rt->pending_price = cell->land_price;
                notice_append(rt,
                    "到达自己的房产 %d 号（等级 %d）。是否升级？(Y/N)\n",
                    cell->index, cell->building_level);
                return A4_MOVE_LANDING_PENDING;
            } else {
                int toll = cell->land_price / 2;
                int owner_idx = cell->owner_id - 1;
                rt->money[idx] -= toll;
                if (owner_idx >= 0 && owner_idx < rt->player_count) {
                    rt->money[owner_idx] += toll;
                }
                notice_append(rt,
                    "到达玩家 %d 的房产 %d 号，支付过路费 %d 元。\n",
                    cell->owner_id, cell->index, toll);
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
            rt->money[idx] += cell->mine_points;
            notice_append(rt, "到达矿地，获得 %d 点。\n", cell->mine_points);
            break;
        case CELL_TOOL_SHOP:
            notice_append(rt, "到达道具屋。\n");
            break;
        case CELL_GIFT_SHOP:
            if (gift_shop_begin(&rt->gift_shop, (size_t)idx) != GIFT_OK) {
                return A4_MOVE_FAILED;
            }
            rt->context = RUNTIME_CONTEXT_GIFT_HOUSE;
            notice_append(rt,
                "欢迎光临礼品屋，请选择一件礼品：\n"
                "  1 奖金（2000 元）\n"
                "  2 点数卡（200 点）\n"
                "  3 财神（5 轮内免过路费）\n");
            return A4_MOVE_LANDING_PENDING;
        case CELL_MAGIC_HOUSE:
            if (magic_house_begin(&rt->magic_house, (size_t)idx) != MAGIC_OK) {
                return A4_MOVE_FAILED;
            }
            rt->context = RUNTIME_CONTEXT_MAGIC_HOUSE;
            notice_append(rt, "进入魔法屋，可选择已注册的魔法：\n");
            if (rt->magic_effect_count == 0U) {
                notice_append(rt, "  当前没有配置具体魔法。\n");
            } else {
                size_t effect_index;
                for (effect_index = 0; effect_index < rt->magic_effect_count;
                     ++effect_index) {
                    notice_append(rt, "  %d %s\n",
                                  rt->magic_effects[effect_index].id,
                                  rt->magic_effects[effect_index].name);
                }
            }
            notice_append(rt, "  F 离开魔法屋\n");
            return A4_MOVE_LANDING_PENDING;
        default:
            break;
    }

    gift_shop_finish_turn(&rt->gift_shop, (size_t)idx);
    rt->context = RUNTIME_CONTEXT_TURN_START;
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

GameRuntime *runtime_create(int player_count, int initial_money,
                            const int *chosen_roles)
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
    rt->context = RUNTIME_CONTEXT_TURN_START;
    gift_shop_init(&rt->gift_shop, (size_t)player_count);
    magic_house_init(&rt->magic_house, (size_t)player_count);

    if (chosen_roles == NULL) {
        free(rt);
        return NULL;
    }
    for (i = 0; i < player_count; ++i) {
        const Character *ch = character_by_id(chosen_roles[i]);
        if (ch == NULL) {
            free(rt);
            return NULL;
        }
        rt->players[i].id = i + 1;
        rt->players[i].name = ch->name;
        rt->players[i].symbol = ch->symbol;
        rt->players[i].color = ROLE_COLORS[chosen_roles[i] - 1];
        rt->players[i].position = 0;
        rt->players[i].active = 1;
        rt->money[i] = initial_money;

        configs[i].id = (A4PlayerId)(i + 1);
        configs[i].role_name = ch->name;
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

static int runtime_move(GameRuntime *rt,
                        int forced_steps,
                        char *message,
                        size_t message_size)
{
    A4TurnSnapshot snapshot;
    A4TurnStatus status;
    if (rt == NULL || message == NULL || message_size == 0 || forced_steps < 0) {
        return 1;
    }
    notice_clear(rt);
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    status = a4_turn_manager_roll(&rt->turn_manager,
                                  snapshot.current_player_id, forced_steps);
    (void)snprintf(message, message_size, "%s", rt->notice);
    return status == A4_TURN_OK ? 0 : 1;
}

int runtime_roll(GameRuntime *rt, char *message, size_t message_size)
{
    return runtime_move(rt, 0, message, message_size);
}

int runtime_step(GameRuntime *rt, int steps, char *message, size_t message_size)
{
    if (steps <= 0) {
        return 1;
    }
    return runtime_move(rt, steps, message, message_size);
}

int runtime_answer(GameRuntime *rt,
                   const char *answer,
                   char *message,
                   size_t message_size)
{
    A4TurnSnapshot snapshot;
    A4TurnStatus status;
    size_t player_index;
    int result = 0;

    if (rt == NULL || answer == NULL || message == NULL || message_size == 0) {
        return -1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    if (snapshot.current_player_id == 0U) {
        return -1;
    }
    player_index = (size_t)(snapshot.current_player_id - 1U);
    notice_clear(rt);

    if (rt->context == RUNTIME_CONTEXT_GIFT_HOUSE) {
        GiftCode code = gift_shop_answer(&rt->gift_shop, rt->money,
                                         (size_t)rt->player_count, answer);
        if (code == GIFT_OK) {
            notice_append(rt, "礼品已领取并立即生效。\n");
        } else if (code == GIFT_INVALID_CHOICE) {
            notice_append(rt, "礼品编号无效，已放弃本次机会。\n");
            result = 1;
        } else {
            notice_append(rt, "礼品屋处理失败。\n");
            (void)snprintf(message, message_size, "%s", rt->notice);
            return -1;
        }
    } else if (rt->context == RUNTIME_CONTEXT_MAGIC_HOUSE) {
        MagicTarget target = {MAGIC_TARGET_NONE, 0};
        MagicCode code = magic_house_answer(&rt->magic_house, answer,
                                             rt->magic_effects,
                                             rt->magic_effect_count, &target);
        if (code == MAGIC_OK) {
            notice_append(rt, "魔法施展成功。\n");
        } else if (code == MAGIC_EXIT) {
            notice_append(rt, "已离开魔法屋，本次未施展魔法。\n");
        } else if (code == MAGIC_INVALID_CHOICE) {
            notice_append(rt, "魔法编号无效，请重新选择，或输入 F 离开。\n");
            (void)snprintf(message, message_size, "%s", rt->notice);
            return 1;
        } else if (code == MAGIC_EFFECT_REJECTED) {
            notice_append(rt, "当前条件不能施展该魔法，请重新选择或输入 F。\n");
            (void)snprintf(message, message_size, "%s", rt->notice);
            return 1;
        } else {
            notice_append(rt, "魔法屋处理失败。\n");
            (void)snprintf(message, message_size, "%s", rt->notice);
            return -1;
        }
    } else if (rt->context == RUNTIME_CONTEXT_BUY_CONFIRM) {
        int yes = answer[0] == 'Y' || answer[0] == 'y';
        MapCell *cell = game_map_cell_at_mut(&rt->map, rt->pending_position);
        if (yes && cell != NULL) {
            if (rt->money[player_index] >= rt->pending_price) {
                rt->money[player_index] -= rt->pending_price;
                cell->owner_id = (int)snapshot.current_player_id;
                cell->building_level = 0;
                notice_append(rt, "购买成功，成为房产 %d 号的主人。\n",
                              rt->pending_position);
            } else {
                notice_append(rt, "资金不足，无法购买。\n");
            }
        } else {
            notice_append(rt, "放弃购买。\n");
        }
    } else if (rt->context == RUNTIME_CONTEXT_UPGRADE_CONFIRM) {
        int yes = answer[0] == 'Y' || answer[0] == 'y';
        MapCell *cell = game_map_cell_at_mut(&rt->map, rt->pending_position);
        if (yes && cell != NULL) {
            if (rt->money[player_index] >= rt->pending_price) {
                rt->money[player_index] -= rt->pending_price;
                ++cell->building_level;
                notice_append(rt, "升级成功，房产 %d 号升至等级 %d。\n",
                              rt->pending_position, cell->building_level);
            } else {
                notice_append(rt, "资金不足，无法升级。\n");
            }
        } else {
            notice_append(rt, "放弃升级。\n");
        }
    } else {
        (void)snprintf(message, message_size, "当前没有等待处理的落地事件。\n");
        return -1;
    }

    gift_shop_finish_turn(&rt->gift_shop, player_index);
    rt->context = RUNTIME_CONTEXT_TURN_START;
    status = a4_turn_manager_complete_landing(
        &rt->turn_manager, snapshot.current_player_id, false);
    if (status != A4_TURN_OK) {
        notice_append(rt, "结束落地事件失败：%s。\n",
                      a4_turn_status_string(status));
        result = -1;
    }
    (void)snprintf(message, message_size, "%s", rt->notice);
    return result;
}

RuntimeContext runtime_context(const GameRuntime *rt)
{
    return rt == NULL ? RUNTIME_CONTEXT_TURN_START : rt->context;
}

int runtime_register_magic_effect(GameRuntime *rt, const MagicEffect *effect)
{
    size_t index;
    if (rt == NULL || effect == NULL || effect->id <= 0 ||
        effect->name == NULL || effect->handler == NULL ||
        rt->magic_effect_count >= MAGIC_EFFECT_CAPACITY) {
        return 1;
    }
    for (index = 0; index < rt->magic_effect_count; ++index) {
        if (rt->magic_effects[index].id == effect->id) {
            return 1;
        }
    }
    rt->magic_effects[rt->magic_effect_count++] = *effect;
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
        "玩家 %s（%d 号）：位置 %d（%s），资金 %d 元，点数 %d，财神剩余 %d 轮。\n",
        rt->players[idx].name, player_id, rt->players[idx].position,
        description, rt->money[idx],
        gift_shop_points(&rt->gift_shop, (size_t)idx),
        gift_shop_god_rounds(&rt->gift_shop, (size_t)idx));
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
        "  Step n 遥控骰子移动 n 步（测试用）\n"
        "  Query 查询当前玩家状态\n"
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

int runtime_player_position(const GameRuntime *rt, size_t player_index)
{
    return rt != NULL && player_index < (size_t)rt->player_count
        ? rt->players[player_index].position : -1;
}

int runtime_player_money(const GameRuntime *rt, size_t player_index)
{
    return rt != NULL && player_index < (size_t)rt->player_count
        ? rt->money[player_index] : -1;
}

int runtime_player_points(const GameRuntime *rt, size_t player_index)
{
    return rt != NULL && player_index < (size_t)rt->player_count
        ? gift_shop_points(&rt->gift_shop, player_index) : -1;
}

int runtime_player_god_rounds(const GameRuntime *rt, size_t player_index)
{
    return rt != NULL && player_index < (size_t)rt->player_count
        ? gift_shop_god_rounds(&rt->gift_shop, player_index) : -1;
}
