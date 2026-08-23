#include "property_system.h"

#include <string.h>

static int valid_player_index(int player_index) {
    return player_index >= 0 && player_index < MAX_PLAYERS;
}

static int valid_position(int position) {
    return position >= 0 && position < BOARD_SIZE;
}

static void reset_result(PropertyResult *result) {
    (void)memset(result, 0, sizeof(*result));
    result->code = PROPERTY_OK;
    result->player_index = -1;
    result->owner_index = -1;
    result->position = -1;
}

static void clear_pending(PropertySystem *system) {
    system->pending_action = PROPERTY_ACTION_NONE;
    system->pending_player_index = -1;
    system->pending_position = -1;
    system->pending_cost = 0;
}

static int land_base_price(const PropertySystem *system,
                           const Cell *cell,
                           int position) {
    int price = system->base_price[position];
    if (price > 0) {
        return price;
    }
    if (cell->value <= 0) {
        return 0;
    }
    if (cell->type == CELL_HOUSE && cell->level >= 0) {
        return cell->value / (cell->level + 1);
    }
    return cell->value;
}

static int total_investment(const PropertySystem *system,
                            const Cell *cell,
                            int position) {
    int price = land_base_price(system, cell, position);
    int level = cell->level;
    if (price <= 0 || cell->type != CELL_HOUSE || cell->owner < 0) {
        return 0;
    }
    if (level < 0) {
        level = 0;
    } else if (level > PROPERTY_MAX_LEVEL) {
        level = PROPERTY_MAX_LEVEL;
    }
    return price * (level + 1);
}

static void finish_player_turn(PropertySystem *system, int player_index) {
    if (valid_player_index(player_index) &&
        system->fortune_turns[player_index] > 0U) {
        --system->fortune_turns[player_index];
    }
}

void property_system_init(PropertySystem *system, const GameState *gs) {
    int position;
    if (system == NULL) {
        return;
    }
    (void)memset(system, 0, sizeof(*system));
    clear_pending(system);
    if (gs == NULL) {
        return;
    }
    for (position = 0; position < BOARD_SIZE; ++position) {
        const Cell *cell = &gs->board[position];
        if (cell->type == CELL_EMPTY && cell->value > 0) {
            system->base_price[position] = cell->value;
        } else if (cell->type == CELL_HOUSE && cell->value > 0) {
            int divisor = cell->level >= 0 ? cell->level + 1 : 1;
            system->base_price[position] = cell->value / divisor;
        }
    }
}

PropertyCode property_configure_land(PropertySystem *system,
                                     GameState *gs,
                                     int position,
                                     int price) {
    Cell *cell;
    if (system == NULL || gs == NULL || !valid_position(position) ||
        price <= 0) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    cell = &gs->board[position];
    system->base_price[position] = price;
    cell->type = CELL_EMPTY;
    cell->owner = -1;
    cell->value = price;
    cell->level = 0;
    return PROPERTY_OK;
}

static PropertyCode begin_pending(PropertySystem *system,
                                  PropertyAction action,
                                  int player_index,
                                  int position,
                                  int cost,
                                  int level,
                                  PropertyResult *result) {
    system->pending_action = action;
    system->pending_player_index = player_index;
    system->pending_position = position;
    system->pending_cost = cost;
    result->code = PROPERTY_PENDING;
    result->action = action;
    result->player_index = player_index;
    result->position = position;
    result->cost = cost;
    result->building_level = level;
    return PROPERTY_PENDING;
}

static PropertyCode handle_toll(PropertySystem *system,
                                GameState *gs,
                                int player_index,
                                const MoveResult *move,
                                PropertyResult *result) {
    Cell *cell = &gs->board[move->to_pos];
    int owner_index = cell->owner;
    int investment;
    int expected_toll;
    int adjustment;
    int exempt_owner;
    int exempt_fortune;

    if (!valid_player_index(owner_index) || owner_index == player_index) {
        result->code = PROPERTY_ERR_INVALID_ARGUMENT;
        return result->code;
    }
    investment = total_investment(system, cell, move->to_pos);
    expected_toll = investment / 2;

    /* A8 已按 Cell.value/2 收费；这里校准为 A10 的累计投资/2。 */
    adjustment = expected_toll - move->toll;
    gs->players[player_index].money -= adjustment;
    gs->players[owner_index].money += adjustment;

    exempt_owner = gs->players[owner_index].hospital_days > 0 ||
                   gs->players[owner_index].prison_days > 0;
    exempt_fortune = system->fortune_turns[player_index] > 0U;
    if (exempt_owner || exempt_fortune) {
        gs->players[player_index].money += expected_toll;
        gs->players[owner_index].money -= expected_toll;
    }

    result->action = PROPERTY_ACTION_TOLL;
    result->player_index = player_index;
    result->owner_index = owner_index;
    result->position = move->to_pos;
    result->building_level = cell->level;
    result->total_investment = investment;
    result->toll = exempt_owner || exempt_fortune ? 0 : expected_toll;
    if (exempt_owner) {
        result->toll_outcome = TOLL_EXEMPT_OWNER_RESTRAINED;
    } else if (exempt_fortune) {
        result->toll_outcome = TOLL_EXEMPT_FORTUNE;
    } else {
        result->toll_outcome = TOLL_PAID;
    }
    result->code = PROPERTY_OK;
    return PROPERTY_OK;
}

PropertyCode property_after_move(PropertySystem *system,
                                 GameState *gs,
                                 int moving_player_index,
                                 const MoveResult *move,
                                 PropertyResult *result) {
    Cell *cell;
    Player *player;
    int price;
    PropertyCode code = PROPERTY_NOT_APPLICABLE;
    if (system == NULL || gs == NULL || move == NULL || result == NULL ||
        !valid_player_index(moving_player_index) ||
        !valid_position(move->to_pos)) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    reset_result(result);
    if (property_has_pending(system)) {
        result->code = PROPERTY_ERR_PENDING_DECISION;
        return result->code;
    }
    if (move->event != EV_NONE) {
        result->code = PROPERTY_NOT_APPLICABLE;
        finish_player_turn(system, moving_player_index);
        return result->code;
    }

    cell = &gs->board[move->to_pos];
    player = &gs->players[moving_player_index];
    price = land_base_price(system, cell, move->to_pos);

    if (move->land == LAND_EMPTY) {
        result->action = PROPERTY_ACTION_BUY;
        result->player_index = moving_player_index;
        result->position = move->to_pos;
        result->cost = price;
        if (price <= 0) {
            code = PROPERTY_ERR_PRICE_NOT_SET;
        } else if (player->money < price) {
            code = PROPERTY_ERR_INSUFFICIENT_FUNDS;
        } else {
            return begin_pending(system, PROPERTY_ACTION_BUY,
                                 moving_player_index, move->to_pos,
                                 price, 0, result);
        }
    } else if (move->land == LAND_OWN_HOUSE) {
        result->action = PROPERTY_ACTION_UPGRADE;
        result->player_index = moving_player_index;
        result->position = move->to_pos;
        result->cost = price;
        result->building_level = cell->level;
        if (price <= 0) {
            code = PROPERTY_ERR_PRICE_NOT_SET;
        } else if (cell->level >= PROPERTY_MAX_LEVEL) {
            code = PROPERTY_ERR_MAX_LEVEL;
        } else if (player->money < price) {
            code = PROPERTY_ERR_INSUFFICIENT_FUNDS;
        } else {
            return begin_pending(system, PROPERTY_ACTION_UPGRADE,
                                 moving_player_index, move->to_pos,
                                 price, cell->level + 1, result);
        }
    } else if (move->land == LAND_OTHER_HOUSE) {
        code = handle_toll(system, gs, moving_player_index, move, result);
    } else {
        result->code = PROPERTY_NOT_APPLICABLE;
    }

    result->code = code;
    finish_player_turn(system, moving_player_index);
    return code;
}

PropertyCode property_resolve_pending(PropertySystem *system,
                                      GameState *gs,
                                      int accept,
                                      PropertyResult *result) {
    PropertyAction action;
    int player_index;
    int position;
    int cost;
    Cell *cell;
    Player *player;
    if (system == NULL || gs == NULL || result == NULL ||
        !property_has_pending(system)) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    reset_result(result);
    action = system->pending_action;
    player_index = system->pending_player_index;
    position = system->pending_position;
    cost = system->pending_cost;
    cell = &gs->board[position];
    player = &gs->players[player_index];

    result->action = action;
    result->player_index = player_index;
    result->position = position;
    result->cost = cost;
    if (!accept) {
        clear_pending(system);
        finish_player_turn(system, player_index);
        return PROPERTY_OK;
    }
    if (player->money < cost) {
        result->code = PROPERTY_ERR_INSUFFICIENT_FUNDS;
        return result->code;
    }

    if (action == PROPERTY_ACTION_BUY) {
        if (cell->type != CELL_EMPTY || cell->owner != -1) {
            result->code = PROPERTY_ERR_INVALID_ARGUMENT;
            return result->code;
        }
        player->money -= cost;
        cell->type = CELL_HOUSE;
        cell->owner = player_index;
        cell->level = 0;
        cell->value = cost;
    } else if (action == PROPERTY_ACTION_UPGRADE) {
        if (cell->type != CELL_HOUSE || cell->owner != player_index ||
            cell->level < 0 || cell->level >= PROPERTY_MAX_LEVEL) {
            result->code = PROPERTY_ERR_INVALID_ARGUMENT;
            return result->code;
        }
        player->money -= cost;
        ++cell->level;
        cell->value = cost * (cell->level + 1);
    } else {
        result->code = PROPERTY_ERR_INVALID_ARGUMENT;
        return result->code;
    }

    result->building_level = cell->level;
    result->total_investment = cell->value;
    clear_pending(system);
    finish_player_turn(system, player_index);
    return PROPERTY_OK;
}

PropertyCode property_sell(PropertySystem *system,
                           GameState *gs,
                           int player_index,
                           int position,
                           PropertyResult *result) {
    Cell *cell;
    int price;
    int investment;
    if (system == NULL || gs == NULL || result == NULL ||
        !valid_player_index(player_index) || !valid_position(position)) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    reset_result(result);
    result->action = PROPERTY_ACTION_SELL;
    result->player_index = player_index;
    result->position = position;
    if (!property_can_roll(system, gs, player_index)) {
        result->code = PROPERTY_ERR_NOT_PRE_ROLL;
        return result->code;
    }
    cell = &gs->board[position];
    if (cell->type != CELL_HOUSE || cell->owner != player_index) {
        result->code = PROPERTY_ERR_NOT_OWNER;
        return result->code;
    }
    price = land_base_price(system, cell, position);
    investment = total_investment(system, cell, position);
    if (price <= 0 || investment <= 0) {
        result->code = PROPERTY_ERR_PRICE_NOT_SET;
        return result->code;
    }

    result->building_level = cell->level;
    result->total_investment = investment;
    result->sale_price = investment * 2;
    gs->players[player_index].money += result->sale_price;
    cell->type = CELL_EMPTY;
    cell->owner = -1;
    cell->value = price;
    cell->level = 0;
    return PROPERTY_OK;
}

int property_has_pending(const PropertySystem *system) {
    return system != NULL && system->pending_action != PROPERTY_ACTION_NONE;
}

int property_pending_player(const PropertySystem *system) {
    return property_has_pending(system) ? system->pending_player_index : -1;
}

int property_can_roll(const PropertySystem *system,
                      const GameState *gs,
                      int player_index) {
    return system != NULL && gs != NULL &&
           valid_player_index(player_index) &&
           !property_has_pending(system) &&
           gs->current_player == player_index && !gs->has_moved;
}

PropertyCode property_set_fortune_turns(PropertySystem *system,
                                        int player_index,
                                        unsigned int turns) {
    if (system == NULL || !valid_player_index(player_index)) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    system->fortune_turns[player_index] = turns;
    return PROPERTY_OK;
}

unsigned int property_get_fortune_turns(const PropertySystem *system,
                                        int player_index) {
    if (system == NULL || !valid_player_index(player_index)) {
        return 0U;
    }
    return system->fortune_turns[player_index];
}

const char *property_code_string(PropertyCode code) {
    switch (code) {
        case PROPERTY_OK: return "成功";
        case PROPERTY_PENDING: return "等待Y/N确认";
        case PROPERTY_NOT_APPLICABLE: return "本次落地无需房产处理";
        case PROPERTY_ERR_INVALID_ARGUMENT: return "参数或房产状态无效";
        case PROPERTY_ERR_PENDING_DECISION: return "已有购买或升级等待确认";
        case PROPERTY_ERR_PRICE_NOT_SET: return "土地基础价格未配置";
        case PROPERTY_ERR_INSUFFICIENT_FUNDS: return "资金不足";
        case PROPERTY_ERR_MAX_LEVEL: return "房产已达到最高等级";
        case PROPERTY_ERR_NOT_OWNER: return "只能出售自己的房产";
        case PROPERTY_ERR_NOT_PRE_ROLL: return "只能在当前玩家掷骰前出售";
        default: return "未知房产错误";
    }
}
