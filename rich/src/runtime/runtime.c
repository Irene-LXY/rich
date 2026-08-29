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
#include "monopoly/query.h"
#include "monopoly/automation.h"

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
#define MAX_BUILDING_LEVEL 3  /* 空地0 / 茅屋1 / 洋房2 / 摩天楼3 */

struct GameRuntime {
    GameMap      map;
    PlayerToken  players[A4_MAX_PLAYERS];
    int          money[A4_MAX_PLAYERS];
    unsigned int tools[A4_MAX_PLAYERS][4];  /* 道具库存：1路障 2机器娃娃 3炸弹 */
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

/* 移动途中每格回调：处理路障/炸弹拦截。返回 0 中断移动。 */
typedef struct ToolMoveContext {
    GameRuntime *runtime;
    int player_index;
    int event;  /* 0=无，1=路障，2=炸弹 */
} ToolMoveContext;

static int tool_move_handler(PlayerToken *player, const MapCell *cell,
                             const MoveContext *context, void *data)
{
    ToolMoveContext *move = (ToolMoveContext *)data;
    GameRuntime *rt;
    (void)player;
    (void)context;
    if (move == NULL || cell == NULL) {
        return 1;
    }
    rt = move->runtime;
    if (cell->has_block) {
        MapCell *mutable_cell = game_map_cell_at_mut(&rt->map, cell->index);
        if (mutable_cell != NULL) {
            mutable_cell->has_block = 0;
        }
        move->event = 1;
        notice_append(rt, "踩到路障，停在 %d 号位置，路障已消失。\n", cell->index);
        return 0;
    }
    if (cell->has_bomb) {
        MapCell *mutable_cell = game_map_cell_at_mut(&rt->map, cell->index);
        if (mutable_cell != NULL) {
            mutable_cell->has_bomb = 0;
        }
        player->position = 14; /* 医院 */
        move->event = 2;
        notice_append(rt, "踩到炸弹，送往医院并住院 3 回合。\n");
        (void)a4_turn_manager_set_skip(&rt->turn_manager,
            (A4PlayerId)(move->player_index + 1), A4_SKIP_HOSPITAL, 3U, "炸弹住院");
        return 0;
    }
    return 1;
}

/* 宣布破产（A17）：土地归还系统，只剩一名玩家时由 A4 结束游戏。 */
static void runtime_declare_bankruptcy(GameRuntime *rt, int idx)
{
    int i;
    notice_append(rt, "玩家 %s 资金不足 0，宣告破产！\n", rt->players[idx].name);
    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        MapCell *cell = game_map_cell_at_mut(&rt->map, i);
        if (cell != NULL && cell->type == CELL_LAND &&
            cell->owner_id == idx + 1) {
            cell->owner_id = RICH_NO_OWNER;
            cell->building_level = 0;
            notice_append(rt, "位置 %d 的土地归还系统，恢复为空地。\n", i);
        }
    }
    (void)a4_turn_manager_mark_player_out(&rt->turn_manager,
                                          (A4PlayerId)(idx + 1));
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

    if (tool_ctx.event == 2) {
        /* 炸弹已送医院并住院，跳过落地处理避免重复 */
        gift_shop_finish_turn(&rt->gift_shop, (size_t)idx);
        rt->context = RUNTIME_CONTEXT_TURN_START;
        return A4_MOVE_RESOLVED;
    }

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
                if (cell->building_level >= MAX_BUILDING_LEVEL) {
                    notice_append(rt,
                        "到达自己的房产 %d 号，已是最高等级（摩天楼），无法继续升级。\n",
                        cell->index);
                    break;
                }
                rt->context = RUNTIME_CONTEXT_UPGRADE_CONFIRM;
                rt->pending_position = token->position;
                rt->pending_price = cell->land_price;
                notice_append(rt,
                    "到达自己的房产 %d 号（等级 %d）。是否升级？(Y/N)\n",
                    cell->index, cell->building_level);
                return A4_MOVE_LANDING_PENDING;
            } else {
                int property_value = cell->land_price * (cell->building_level + 1);
                int toll = property_value / 2;
                int owner_idx = cell->owner_id - 1;
                int exempt = 0;
                if (gift_shop_god_rounds(&rt->gift_shop, (size_t)idx) > 0) {
                    exempt = 1;
                    notice_append(rt, "财神附身，可免过路费。\n");
                }
                if (owner_idx >= 0 && owner_idx < rt->player_count) {
                    const A4PlayerState *op = &rt->turn_manager.players[owner_idx];
                    if (op->skip_turns_remaining > 0U &&
                        (op->skip_reason == A4_SKIP_HOSPITAL ||
                         op->skip_reason == A4_SKIP_PRISON)) {
                        exempt = 1;
                        notice_append(rt, "地产主人在医院或监狱中，可免过路费。\n");
                    }
                }
                if (exempt) {
                    notice_append(rt, "到达玩家 %d 的房产 %d 号，免付过路费。\n",
                                  cell->owner_id, cell->index);
                } else {
                    rt->money[idx] -= toll;
                    if (owner_idx >= 0 && owner_idx < rt->player_count) {
                        rt->money[owner_idx] += toll;
                    }
                    notice_append(rt,
                        "到达玩家 %d 的房产 %d 号，支付过路费 %d 元。\n",
                        cell->owner_id, cell->index, toll);
                    if (rt->money[idx] < 0) {
                        runtime_declare_bankruptcy(rt, idx);
                    }
                }
            }
            break;
        case CELL_HOSPITAL:
            notice_append(rt, "进入医院，住院 3 天（轮空 3 次）。\n");
            (void)a4_turn_manager_set_skip(
                &rt->turn_manager, (A4PlayerId)player_id,
                A4_SKIP_HOSPITAL, 3U, "住院");
            break;
        case CELL_PRISON:
            notice_append(rt, "进入监狱，扣留 2 天（轮空 2 次）。\n");
            (void)a4_turn_manager_set_skip(
                &rt->turn_manager, (A4PlayerId)player_id,
                A4_SKIP_PRISON, 2U, "入狱");
            break;
        case CELL_MINE:
            rt->gift_shop.points[idx] += cell->mine_points;
            notice_append(rt, "到达矿地，获得 %d 点，当前点数 %d。\n",
                          cell->mine_points, rt->gift_shop.points[idx]);
            break;
        case CELL_TOOL_SHOP:
            if (rt->gift_shop.points[idx] < 30 ||
                rt->tools[idx][1] + rt->tools[idx][2] +
                rt->tools[idx][3] >= 10U) {
                notice_append(rt, "到达道具屋，但点数不足或背包已满，自动退出。\n");
                break;
            }
            rt->context = RUNTIME_CONTEXT_TOOL_SHOP;
            notice_append(rt,
                "到达道具屋。欢迎光临，请输入 1/2/3 选择道具：\n"
                "  1 路障（50 点）  2 机器娃娃（30 点）  3 炸弹（50 点）\n"
                "  F 退出道具屋\n"
                "当前点数 %d。\n", rt->gift_shop.points[idx]);
            return A4_MOVE_LANDING_PENDING;
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
            if (cell->building_level >= MAX_BUILDING_LEVEL) {
                notice_append(rt, "房产 %d 号已是最高等级，无法升级。\n",
                              rt->pending_position);
            } else if (rt->money[player_index] >= rt->pending_price) {
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
    } else if (rt->context == RUNTIME_CONTEXT_TOOL_SHOP) {
        int choice = answer[0] - '0';
        int price;
        if (answer[0] == 'F' || answer[0] == 'f') {
            notice_append(rt, "退出道具屋。\n");
        } else if (answer[0] >= '1' && answer[0] <= '3' && answer[1] == '\0') {
            price = (answer[0] == '2') ? 30 : 50;
            if (rt->gift_shop.points[player_index] < price) {
                notice_append(rt, "点数不足：需要 %d 点，当前 %d 点。\n",
                              price, rt->gift_shop.points[player_index]);
                (void)snprintf(message, message_size, "%s", rt->notice);
                return 1;
            }
            if (rt->tools[player_index][1] + rt->tools[player_index][2] +
                rt->tools[player_index][3] >= 10U) {
                notice_append(rt, "道具数量已达上限 10。\n");
                (void)snprintf(message, message_size, "%s", rt->notice);
                return 1;
            }
            rt->gift_shop.points[player_index] -= price;
            rt->tools[player_index][choice]++;
            notice_append(rt, "购买成功，剩余点数 %d。\n",
                          rt->gift_shop.points[player_index]);
        } else {
            notice_append(rt, "输入无效，请输入 1/2/3 或 F。\n");
            (void)snprintf(message, message_size, "%s", rt->notice);
            return 1;
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
    QueryPlayerState state;
    int player_id;
    int idx;
    int i;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;

    (void)memset(&state, 0, sizeof(state));
    state.player_id = player_id;
    state.player_name = rt->players[idx].name;
    state.symbol = rt->players[idx].symbol;
    state.money = rt->money[idx];
    state.points = gift_shop_points(&rt->gift_shop, (size_t)idx);
    state.position = rt->players[idx].position;
    state.fortune_turns = gift_shop_god_rounds(&rt->gift_shop, (size_t)idx);

    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const MapCell *cell = &rt->map.cells[i];
        if (cell->type == CELL_LAND && cell->owner_id == player_id &&
            state.property_count < QUERY_MAX_PROPERTIES) {
            state.properties[state.property_count].position = i;
            state.properties[state.property_count].land_price = cell->land_price;
            state.properties[state.property_count].building_level =
                cell->building_level;
            ++state.property_count;
        }
    }

    {
        const A4PlayerState *ap = &rt->turn_manager.players[idx];
        state.hospital_turns = (ap->skip_reason == A4_SKIP_HOSPITAL)
            ? (int)ap->skip_turns_remaining : 0;
        state.prison_turns = (ap->skip_reason == A4_SKIP_PRISON)
            ? (int)ap->skip_turns_remaining : 0;
    }

    state.item_counts[QUERY_ITEM_BLOCK] = (int)rt->tools[idx][1];
    state.item_counts[QUERY_ITEM_ROBOT] = (int)rt->tools[idx][2];
    state.item_counts[QUERY_ITEM_BOMB] = (int)rt->tools[idx][3];
    state.bankrupt = rt->turn_manager.players[idx].participating ? 0 : 1;

    return query_format_player(&state, message, message_size) == 0 ? 0 : 1;
}

int runtime_sell(GameRuntime *rt, int position, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    MapCell *cell;
    int player_id;
    int idx;
    int sale_price;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    notice_clear(rt);
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;
    if (position < 0 || position >= RICH_MAP_SIZE) {
        notice_append(rt, "位置无效，请输入 0~69 的房产位置。\n");
        (void)snprintf(message, message_size, "%s", rt->notice);
        return 1;
    }
    cell = game_map_cell_at_mut(&rt->map, position);
    if (cell == NULL || cell->type != CELL_LAND || cell->owner_id != player_id) {
        notice_append(rt, "位置 %d 不是你的房产，无法出售。\n", position);
        (void)snprintf(message, message_size, "%s", rt->notice);
        return 1;
    }
    sale_price = cell->land_price * (cell->building_level + 1) * 2;
    rt->money[idx] += sale_price;
    cell->owner_id = RICH_NO_OWNER;
    cell->building_level = 0;
    notice_append(rt, "出售房产 %d 号，获得 %d 元。\n", position, sale_price);
    (void)snprintf(message, message_size, "%s", rt->notice);
    return 0;
}

int runtime_place_tool(GameRuntime *rt, int tool, int distance,
                       char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    int player_id;
    int idx;
    int target;
    MapCell *cell;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    if (tool != 1 && tool != 3) {
        return 1;
    }
    if (distance < -10 || distance > 10) {
        (void)snprintf(message, message_size, "距离必须在 -10~10 之间。\n");
        return 1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;
    if (rt->tools[idx][tool] == 0U) {
        (void)snprintf(message, message_size, "你没有该道具。\n");
        return 1;
    }
    target = game_map_normalize_position(rt->players[idx].position + distance);
    cell = game_map_cell_at_mut(&rt->map, target);
    if (cell == NULL || cell->has_block || cell->has_bomb) {
        (void)snprintf(message, message_size, "目标位置已有道具，无法放置。\n");
        return 1;
    }
    if (tool == 1) {
        cell->has_block = 1;
    } else {
        cell->has_bomb = 1;
    }
    rt->tools[idx][tool]--;
    (void)snprintf(message, message_size, "已在位置 %d 放置%s。\n",
                   target, tool == 1 ? "路障" : "炸弹");
    return 0;
}

int runtime_use_robot(GameRuntime *rt, char *message, size_t message_size)
{
    A4TurnSnapshot snapshot;
    int player_id;
    int idx;
    int distance;
    int removed = 0;
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    snapshot = a4_turn_manager_snapshot(&rt->turn_manager);
    player_id = (int)snapshot.current_player_id;
    idx = player_id - 1;
    if (rt->tools[idx][2] == 0U) {
        (void)snprintf(message, message_size, "你没有机器娃娃。\n");
        return 1;
    }
    for (distance = 1; distance <= 10; ++distance) {
        int pos = game_map_normalize_position(rt->players[idx].position + distance);
        MapCell *cell = game_map_cell_at_mut(&rt->map, pos);
        if (cell != NULL && (cell->has_block || cell->has_bomb)) {
            cell->has_block = 0;
            cell->has_bomb = 0;
            ++removed;
        }
    }
    rt->tools[idx][2]--;
    (void)snprintf(message, message_size, "机器娃娃清扫了 %d 个道具。\n", removed);
    return 0;
}

int runtime_buy_tool(GameRuntime *rt, int tool, char *message, size_t message_size)
{
    char answer[2];
    if (rt == NULL || message == NULL || message_size == 0 || tool < 1 || tool > 3) {
        return 1;
    }
    answer[0] = (char)('0' + tool);
    answer[1] = '\0';
    return runtime_answer(rt, answer, message, message_size) == 0 ? 0 : 1;
}

int runtime_render(GameRuntime *rt, char *message, size_t message_size)
{
    if (rt == NULL || message == NULL || message_size == 0) {
        return 1;
    }
    return render_map(&rt->map, rt->players, (size_t)rt->player_count,
                      1, 1, message, message_size) ? 0 : 1;
}

int runtime_help(GameRuntime *rt, char *message, size_t message_size)
{
    (void)rt;
    if (message == NULL || message_size == 0) {
        return 1;
    }
    (void)snprintf(message, message_size,
        "可用命令：\n"
        "  Roll      掷骰子移动 1~6 步\n"
        "  Step n    遥控骰子移动 n 步（测试用）\n"
        "  Sell n    出售位置 n 的房产\n"
        "  Block n   在前后 10 步内放置路障（n 为相对距离，负数表示后方）\n"
        "  Bomb n    在前后 10 步内放置炸弹（n 为相对距离，负数表示后方）\n"
        "  Robot     使用机器娃娃清扫前方 10 步内的障碍\n"
        "  Query     查询当前玩家资产\n"
        "  Map       显示地图\n"
        "  Help      显示本帮助\n"
        "  Quit      结束整局游戏\n");
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

/* ---- 自动化测试适配：装载预设 / 快照 / 强制结束（新增，不改既有逻辑） ---- */

static int automation_role_id_for_symbol(char symbol)
{
    const Character *table = character_table();
    int i;

    for (i = 0; i < CHARACTER_COUNT; ++i) {
        if (table[i].symbol == symbol) {
            return (int)table[i].id;
        }
    }
    return 0;
}

GameRuntime *runtime_load_preset(const AutomationPreset *preset)
{
    int roles[AUTOMATION_MAX_PLAYERS];
    GameRuntime *rt;
    int i;

    if (preset == NULL || preset->player_count < (int)A4_MIN_PLAYERS ||
        preset->player_count > (int)A4_MAX_PLAYERS) {
        return NULL;
    }

    for (i = 0; i < preset->player_count; ++i) {
        roles[i] = automation_role_id_for_symbol(preset->players[i].symbol);
        if (roles[i] <= 0) {
            return NULL;
        }
    }

    rt = runtime_create(preset->player_count, 0, roles);
    if (rt == NULL) {
        return NULL;
    }

    for (i = 0; i < preset->player_count; ++i) {
        const AutomationPlayer *p = &preset->players[i];

        rt->players[i].position = p->position;
        rt->money[i] = p->fund;
        rt->gift_shop.points[i] = p->credit;
        rt->gift_shop.god_of_wealth_rounds[i] = p->god_of_wealth_rounds;
        rt->tools[i][1] = (unsigned int)p->block;
        rt->tools[i][2] = (unsigned int)p->robot;
        rt->tools[i][3] = (unsigned int)p->bomb;

        if (p->status == AUTOMATION_STATUS_HOSPITAL) {
            uint16_t rounds =
                (uint16_t)(p->remaining_rounds > 0 ? p->remaining_rounds : 3);
            (void)a4_turn_manager_set_skip(&rt->turn_manager,
                (A4PlayerId)(i + 1), A4_SKIP_HOSPITAL, rounds, "住院");
        } else if (p->status == AUTOMATION_STATUS_JAIL) {
            uint16_t rounds =
                (uint16_t)(p->remaining_rounds > 0 ? p->remaining_rounds : 2);
            (void)a4_turn_manager_set_skip(&rt->turn_manager,
                (A4PlayerId)(i + 1), A4_SKIP_PRISON, rounds, "入狱");
        } else if (p->status == AUTOMATION_STATUS_BANKRUPT) {
            (void)a4_turn_manager_mark_player_out(&rt->turn_manager,
                                                  (A4PlayerId)(i + 1));
        }
    }

    for (i = 0; i < preset->property_count; ++i) {
        const AutomationProperty *pp = &preset->properties[i];
        MapCell *cell = game_map_cell_at_mut(&rt->map, pp->position);
        if (cell == NULL || cell->type != CELL_LAND) {
            continue;
        }
        cell->owner_id = pp->owner_index + 1;
        cell->building_level = pp->level;
    }

    for (i = 0; i < preset->map_item_count; ++i) {
        const AutomationMapItem *mi = &preset->map_items[i];
        MapCell *cell = game_map_cell_at_mut(&rt->map, mi->position);
        if (cell == NULL) {
            continue;
        }
        if (mi->type == AUTOMATION_ITEM_BLOCK) {
            cell->has_block = 1;
        } else if (mi->type == AUTOMATION_ITEM_BOMB) {
            cell->has_bomb = 1;
        }
    }

    (void)a4_turn_manager_begin(&rt->turn_manager);
    if (preset->current_user_index >= 0 &&
        preset->current_user_index < preset->player_count) {
        rt->turn_manager.current_player_index =
            (size_t)preset->current_user_index;
    }
    rt->context = RUNTIME_CONTEXT_TURN_START;

    return rt;
}

static const char *automation_context_prompt(RuntimeContext context)
{
    switch (context) {
        case RUNTIME_CONTEXT_BUY_CONFIRM: return "BUY";
        case RUNTIME_CONTEXT_UPGRADE_CONFIRM: return "UPGRADE";
        case RUNTIME_CONTEXT_TOOL_SHOP: return "TOOL_SHOP";
        case RUNTIME_CONTEXT_GIFT_HOUSE: return "GIFT_SHOP";
        case RUNTIME_CONTEXT_MAGIC_HOUSE: return "MAGIC_HOUSE";
        default: return NULL;
    }
}

void runtime_snapshot(const GameRuntime *rt, AutomationSnapshot *snapshot)
{
    A4TurnSnapshot turn;
    int i;
    int active_count = 0;
    int last_active = -1;

    if (rt == NULL || snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->current_user_index = -1;
    snapshot->winner_index = -1;
    snapshot->player_count = rt->player_count;

    for (i = 0; i < rt->player_count; ++i) {
        const A4PlayerState *ts = &rt->turn_manager.players[i];
        AutomationPlayer *ap = &snapshot->players[i];

        ap->symbol = rt->players[i].symbol;
        ap->fund = rt->money[i];
        ap->credit = rt->gift_shop.points[i];
        ap->position = rt->players[i].position;
        ap->block = (int)rt->tools[i][1];
        ap->robot = (int)rt->tools[i][2];
        ap->bomb = (int)rt->tools[i][3];
        ap->god_of_wealth_rounds = rt->gift_shop.god_of_wealth_rounds[i];

        if (!ts->participating) {
            ap->status = AUTOMATION_STATUS_BANKRUPT;
            ap->remaining_rounds = 0;
        } else if (ts->skip_reason == A4_SKIP_HOSPITAL) {
            ap->status = AUTOMATION_STATUS_HOSPITAL;
            ap->remaining_rounds = (int)ts->skip_turns_remaining;
        } else if (ts->skip_reason == A4_SKIP_PRISON) {
            ap->status = AUTOMATION_STATUS_JAIL;
            ap->remaining_rounds = (int)ts->skip_turns_remaining;
        } else {
            ap->status = AUTOMATION_STATUS_NORMAL;
            ap->remaining_rounds = 0;
        }

        if (ts->participating) {
            ++active_count;
            last_active = i;
        }
    }

    if (rt->turn_manager.phase == A4_TURN_PHASE_FINISHED) {
        snapshot->phase = AUTOMATION_PHASE_ENDED;
        snapshot->game_status = 1;
    } else {
        snapshot->game_status = 0;
        snapshot->phase = (rt->context == RUNTIME_CONTEXT_TURN_START)
            ? AUTOMATION_PHASE_COMMAND : AUTOMATION_PHASE_PROMPT;
        if (snapshot->phase == AUTOMATION_PHASE_PROMPT) {
            snapshot->pending_prompt = automation_context_prompt(rt->context);
        }
    }

    turn = a4_turn_manager_snapshot(&rt->turn_manager);
    if (turn.current_player_id != 0U) {
        snapshot->current_user_index = (int)(turn.current_player_id - 1U);
    }

    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const MapCell *cell = &rt->map.cells[i];
        if (cell->type == CELL_LAND && cell->owner_id != RICH_NO_OWNER) {
            AutomationProperty *p =
                &snapshot->properties[snapshot->property_count];
            p->position = i;
            p->owner_index = cell->owner_id - 1;
            p->level = cell->building_level;
            ++snapshot->property_count;
        }
    }

    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const MapCell *cell = &rt->map.cells[i];
        if (cell->has_block || cell->has_bomb) {
            AutomationMapItem *m =
                &snapshot->map_items[snapshot->map_item_count];
            m->position = i;
            m->type = cell->has_block ? AUTOMATION_ITEM_BLOCK
                                      : AUTOMATION_ITEM_BOMB;
            ++snapshot->map_item_count;
        }
    }

    if (snapshot->game_status == 1 && active_count == 1) {
        snapshot->winner_index = last_active;
    }
}

int runtime_finish(GameRuntime *rt)
{
    if (rt == NULL) {
        return 1;
    }
    return a4_turn_manager_finish(&rt->turn_manager, 0U) == A4_TURN_OK ? 0 : 1;
}
