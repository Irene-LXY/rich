#include "property/property_system.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void result_reset(PropertyResult *result)
{
    if (result != NULL) {
        (void)memset(result, 0, sizeof(*result));
        result->code = PROPERTY_OK;
        result->action = PROPERTY_ACTION_NONE;
        result->player_index = -1;
        result->owner_index = -1;
        result->position = -1;
    }
}

static PropertyCode set_code(PropertyResult *result, PropertyCode code)
{
    if (result != NULL) {
        result->code = code;
    }
    return code;
}

static int system_is_valid(const PropertySystem *system)
{
    return system != NULL && system->map != NULL && system->money != NULL &&
           system->player_count > 0U;
}

static int player_is_valid(const PropertySystem *system, int player_index)
{
    return system_is_valid(system) && player_index >= 0 &&
           (size_t)player_index < system->player_count;
}

static void clear_pending(PropertySystem *system)
{
    system->has_pending = 0;
    system->pending_action = PROPERTY_ACTION_NONE;
    system->pending_player_index = -1;
    system->pending_position = -1;
    system->pending_cost = 0;
}

PropertyCode property_system_init(PropertySystem *system,
                                  GameMap *map,
                                  int *money,
                                  size_t player_count)
{
    if (system == NULL || map == NULL || money == NULL || player_count == 0U) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    system->map = map;
    system->money = money;
    system->player_count = player_count;
    clear_pending(system);
    return PROPERTY_OK;
}

int property_release_player_properties(PropertySystem *system,
                                       int player_index)
{
    int position;
    int released = 0;
    int owner_id;
    if (!player_is_valid(system, player_index)) {
        return 0;
    }
    owner_id = player_index + 1;
    for (position = 0; position < RICH_MAP_SIZE; ++position) {
        MapCell *cell = game_map_cell_at_mut(system->map, position);
        if (cell != NULL && cell->type == CELL_LAND &&
            cell->owner_id == owner_id) {
            cell->owner_id = RICH_NO_OWNER;
            cell->building_level = 0;
            ++released;
        }
    }
    return released;
}

int property_count_player_properties(const PropertySystem *system,
                                     int player_index)
{
    int position;
    int count = 0;
    int owner_id;
    if (!player_is_valid(system, player_index)) {
        return 0;
    }
    owner_id = player_index + 1;
    for (position = 0; position < RICH_MAP_SIZE; ++position) {
        const MapCell *cell = game_map_cell_at(system->map, position);
        if (cell != NULL && cell->type == CELL_LAND &&
            cell->owner_id == owner_id) {
            ++count;
        }
    }
    return count;
}

PropertyCode property_after_move(PropertySystem *system,
                                 int player_index,
                                 int position,
                                 unsigned int toll_exemptions,
                                 PropertyResult *result)
{
    MapCell *cell;
    int owner_index;
    int available;

    result_reset(result);
    if (!player_is_valid(system, player_index) || result == NULL) {
        return set_code(result, PROPERTY_ERR_INVALID_ARGUMENT);
    }
    result->player_index = player_index;
    result->position = position;
    if (position < 0 || position >= RICH_MAP_SIZE) {
        return set_code(result, PROPERTY_ERR_INVALID_POSITION);
    }
    if (system->has_pending) {
        return set_code(result, PROPERTY_ERR_PENDING_DECISION);
    }
    if (system->money[player_index] < 0) {
        return set_code(result, PROPERTY_ERR_INVALID_ARGUMENT);
    }

    cell = game_map_cell_at_mut(system->map, position);
    if (cell == NULL || cell->type != CELL_LAND) {
        return set_code(result, PROPERTY_NOT_APPLICABLE);
    }
    result->cost = cell->land_price;
    result->building_level = cell->building_level;
    result->total_investment = cell->land_price * (cell->building_level + 1);

    if (cell->owner_id == RICH_NO_OWNER) {
        system->has_pending = 1;
        system->pending_action = PROPERTY_ACTION_BUY;
        system->pending_player_index = player_index;
        system->pending_position = position;
        system->pending_cost = cell->land_price;
        result->action = PROPERTY_ACTION_BUY;
        return set_code(result, PROPERTY_PENDING);
    }

    if (cell->owner_id == player_index + 1) {
        result->owner_index = player_index;
        result->action = PROPERTY_ACTION_UPGRADE;
        if (cell->building_level >= PROPERTY_MAX_LEVEL) {
            return set_code(result, PROPERTY_ERR_MAX_LEVEL);
        }
        system->has_pending = 1;
        system->pending_action = PROPERTY_ACTION_UPGRADE;
        system->pending_player_index = player_index;
        system->pending_position = position;
        system->pending_cost = cell->land_price;
        return set_code(result, PROPERTY_PENDING);
    }

    owner_index = cell->owner_id - 1;
    result->action = PROPERTY_ACTION_TOLL;
    result->owner_index = owner_index;
    result->toll_exemptions = toll_exemptions &
        (PROPERTY_TOLL_EXEMPT_FORTUNE |
         PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED);
    if (result->toll_exemptions != PROPERTY_TOLL_EXEMPT_NONE) {
        result->toll = 0;
        return set_code(result, PROPERTY_OK);
    }

    result->toll = result->total_investment / 2;
    available = system->money[player_index];
    result->amount_paid = available < result->toll ? available : result->toll;
    if (result->amount_paid < 0) {
        result->amount_paid = 0;
    }
    system->money[player_index] -= result->toll;
    if (owner_index >= 0 && (size_t)owner_index < system->player_count) {
        system->money[owner_index] += result->amount_paid;
    }
    if (system->money[player_index] < 0) {
        result->player_bankrupt = 1;
        result->released_property_count =
            property_release_player_properties(system, player_index);
    }
    return set_code(result, PROPERTY_OK);
}

PropertyCode property_resolve_pending(PropertySystem *system,
                                      int player_index,
                                      int accept,
                                      PropertyResult *result)
{
    MapCell *cell;
    PropertyAction action;
    int position;
    int cost;

    result_reset(result);
    if (!player_is_valid(system, player_index) || result == NULL) {
        return set_code(result, PROPERTY_ERR_INVALID_ARGUMENT);
    }
    if (!system->has_pending ||
        system->pending_player_index != player_index) {
        return set_code(result, PROPERTY_ERR_PENDING_DECISION);
    }
    action = system->pending_action;
    position = system->pending_position;
    cost = system->pending_cost;
    result->action = action;
    result->player_index = player_index;
    result->position = position;
    result->cost = cost;

    if (accept != 0 && accept != 1) {
        return set_code(result, PROPERTY_ERR_INVALID_DECISION);
    }
    if (accept == 0) {
        cell = game_map_cell_at_mut(system->map, position);
        if (cell != NULL) {
            result->building_level = cell->building_level;
        }
        clear_pending(system);
        return set_code(result, PROPERTY_OK);
    }
    result->accepted = 1;
    if (system->money[player_index] < cost) {
        clear_pending(system);
        return set_code(result, PROPERTY_ERR_INSUFFICIENT_FUNDS);
    }

    cell = game_map_cell_at_mut(system->map, position);
    if (cell == NULL || cell->type != CELL_LAND) {
        clear_pending(system);
        return set_code(result, PROPERTY_NOT_APPLICABLE);
    }
    if (action == PROPERTY_ACTION_BUY) {
        if (cell->owner_id != RICH_NO_OWNER) {
            clear_pending(system);
            return set_code(result, PROPERTY_ERR_NO_PROPERTY);
        }
        system->money[player_index] -= cost;
        cell->owner_id = player_index + 1;
        cell->building_level = 0;
    } else if (action == PROPERTY_ACTION_UPGRADE) {
        if (cell->owner_id != player_index + 1) {
            clear_pending(system);
            return set_code(result, PROPERTY_ERR_NOT_OWNER);
        }
        if (cell->building_level >= PROPERTY_MAX_LEVEL) {
            clear_pending(system);
            return set_code(result, PROPERTY_ERR_MAX_LEVEL);
        }
        system->money[player_index] -= cost;
        ++cell->building_level;
    } else {
        clear_pending(system);
        return set_code(result, PROPERTY_ERR_PENDING_DECISION);
    }

    result->building_level = cell->building_level;
    result->owner_index = player_index;
    result->total_investment = cell->land_price * (cell->building_level + 1);
    clear_pending(system);
    return set_code(result, PROPERTY_OK);
}

PropertyCode property_resolve_answer(PropertySystem *system,
                                     int player_index,
                                     const char *answer,
                                     PropertyResult *result)
{
    const char *begin;
    const char *end;
    int accept;
    result_reset(result);
    if (answer == NULL || result == NULL) {
        return set_code(result, PROPERTY_ERR_INVALID_ARGUMENT);
    }
    begin = answer;
    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }
    end = begin + strlen(begin);
    while (end > begin && isspace((unsigned char)end[-1])) {
        --end;
    }
    if (end - begin != 1 ||
        (begin[0] != 'Y' && begin[0] != 'y' &&
         begin[0] != 'N' && begin[0] != 'n')) {
        return set_code(result, PROPERTY_ERR_INVALID_DECISION);
    }
    accept = begin[0] == 'Y' || begin[0] == 'y';
    return property_resolve_pending(system, player_index, accept, result);
}

PropertyCode property_sell_checked(PropertySystem *system,
                                   int player_index,
                                   int position,
                                   PropertySellPermission permission,
                                   PropertyResult *result)
{
    MapCell *cell;
    int sale_price;
    result_reset(result);
    if (!player_is_valid(system, player_index) || result == NULL) {
        return set_code(result, PROPERTY_ERR_INVALID_ARGUMENT);
    }
    result->action = PROPERTY_ACTION_SELL;
    result->player_index = player_index;
    result->position = position;
    if (position < 0 || position >= RICH_MAP_SIZE) {
        return set_code(result, PROPERTY_ERR_INVALID_POSITION);
    }
    if (!permission.is_current_turn) {
        return set_code(result, PROPERTY_ERR_NOT_CURRENT_TURN);
    }
    if (!permission.is_pre_roll) {
        return set_code(result, PROPERTY_ERR_NOT_PRE_ROLL);
    }
    if (permission.is_restrained) {
        return set_code(result, PROPERTY_ERR_PLAYER_RESTRAINED);
    }
    cell = game_map_cell_at_mut(system->map, position);
    if (cell == NULL || cell->type != CELL_LAND) {
        return set_code(result, PROPERTY_ERR_NOT_SELLABLE);
    }
    if (cell->owner_id == RICH_NO_OWNER) {
        return set_code(result, PROPERTY_ERR_NO_PROPERTY);
    }
    if (cell->owner_id != player_index + 1) {
        return set_code(result, PROPERTY_ERR_NOT_OWNER);
    }
    sale_price = 2 * cell->land_price * (cell->building_level + 1);
    system->money[player_index] += sale_price;
    result->sale_price = sale_price;
    result->building_level = cell->building_level;
    cell->owner_id = RICH_NO_OWNER;
    cell->building_level = 0;
    return set_code(result, PROPERTY_OK);
}

PropertyCode property_sell(PropertySystem *system,
                           int player_index,
                           int position,
                           PropertyResult *result)
{
    PropertySellPermission permission = {1, 1, 0};
    return property_sell_checked(system, player_index, position,
                                 permission, result);
}

static PropertyCode parse_integer(const char *text,
                                  long minimum,
                                  long maximum,
                                  int *value,
                                  PropertyCode range_error)
{
    char *end;
    long parsed;
    const char *begin = text;
    if (text == NULL || value == NULL) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }
    if (*begin == '\0') {
        return PROPERTY_ERR_INVALID_POSITION_FORMAT;
    }
    errno = 0;
    parsed = strtol(begin, &end, 10);
    if (begin == end || errno == ERANGE) {
        return PROPERTY_ERR_INVALID_POSITION_FORMAT;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return PROPERTY_ERR_INVALID_POSITION_FORMAT;
    }
    if (parsed < minimum || parsed > maximum) {
        return range_error;
    }
    *value = (int)parsed;
    return PROPERTY_OK;
}

PropertyCode property_parse_position(const char *text, int *position)
{
    return parse_integer(text, 0L, (long)(RICH_MAP_SIZE - 1), position,
                         PROPERTY_ERR_INVALID_POSITION);
}

PropertyCode property_parse_money(const char *text, int *money)
{
    PropertyCode code = parse_integer(text, 0L, INT_MAX, money,
                                      PROPERTY_ERR_INVALID_ARGUMENT);
    return code == PROPERTY_ERR_INVALID_POSITION_FORMAT
        ? PROPERTY_ERR_INVALID_ARGUMENT : code;
}

static int word_equals_ignore_case(const char *begin,
                                   const char *end,
                                   const char *word)
{
    while (begin < end && *word != '\0') {
        if (tolower((unsigned char)*begin) !=
            tolower((unsigned char)*word)) {
            return 0;
        }
        ++begin;
        ++word;
    }
    return begin == end && *word == '\0';
}

PropertyCode property_parse_sell_command(const char *text, int *position)
{
    const char *begin;
    const char *verb_end;
    if (text == NULL || position == NULL) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    begin = text;
    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }
    verb_end = begin;
    while (*verb_end != '\0' && !isspace((unsigned char)*verb_end)) {
        ++verb_end;
    }
    if (!word_equals_ignore_case(begin, verb_end, "sell")) {
        return PROPERTY_ERR_INVALID_ARGUMENT;
    }
    begin = verb_end;
    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }
    return property_parse_position(begin, position);
}

int property_has_pending(const PropertySystem *system)
{
    return system_is_valid(system) ? system->has_pending : 0;
}

PropertyAction property_pending_action(const PropertySystem *system)
{
    return system_is_valid(system) ? system->pending_action
                                   : PROPERTY_ACTION_NONE;
}

const char *property_code_string(PropertyCode code)
{
    switch (code) {
        case PROPERTY_OK: return "成功";
        case PROPERTY_PENDING: return "等待玩家确认";
        case PROPERTY_NOT_APPLICABLE: return "该位置不触发房产操作";
        case PROPERTY_ERR_INVALID_ARGUMENT: return "输入无效";
        case PROPERTY_ERR_INVALID_DECISION: return "输入无效，请输入 Y 或 N";
        case PROPERTY_ERR_INVALID_POSITION: return "无效的位置编号";
        case PROPERTY_ERR_INVALID_POSITION_FORMAT: return "请输入有效的整数位置";
        case PROPERTY_ERR_PENDING_DECISION: return "当前房产操作状态无效";
        case PROPERTY_ERR_INSUFFICIENT_FUNDS: return "资金不足";
        case PROPERTY_ERR_MAX_LEVEL: return "房产已达到最高等级，无法升级";
        case PROPERTY_ERR_NOT_OWNER: return "该房产不属于你";
        case PROPERTY_ERR_NO_PROPERTY: return "该位置没有房产";
        case PROPERTY_ERR_NOT_SELLABLE: return "该位置不可出售";
        case PROPERTY_ERR_NOT_PRE_ROLL:
            return "掷骰后无法出售房产，请下回合操作";
        case PROPERTY_ERR_NOT_CURRENT_TURN: return "请等待你的回合";
        case PROPERTY_ERR_PLAYER_RESTRAINED: return "你正在住院，无法操作";
        default: return "未知房产错误";
    }
}
