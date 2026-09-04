#ifndef PROPERTY_SYSTEM_H
#define PROPERTY_SYSTEM_H

#include <stddef.h>

#include "map/map.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PROPERTY_MAX_LEVEL 3

typedef enum PropertyAction {
    PROPERTY_ACTION_NONE = 0,
    PROPERTY_ACTION_BUY,
    PROPERTY_ACTION_UPGRADE,
    PROPERTY_ACTION_TOLL,
    PROPERTY_ACTION_SELL
} PropertyAction;

typedef enum PropertyCode {
    PROPERTY_OK = 0,
    PROPERTY_PENDING,
    PROPERTY_NOT_APPLICABLE,
    PROPERTY_ERR_INVALID_ARGUMENT,
    PROPERTY_ERR_INVALID_DECISION,
    PROPERTY_ERR_INVALID_POSITION,
    PROPERTY_ERR_INVALID_POSITION_FORMAT,
    PROPERTY_ERR_PENDING_DECISION,
    PROPERTY_ERR_INSUFFICIENT_FUNDS,
    PROPERTY_ERR_MAX_LEVEL,
    PROPERTY_ERR_NOT_OWNER,
    PROPERTY_ERR_NO_PROPERTY,
    PROPERTY_ERR_NOT_SELLABLE,
    PROPERTY_ERR_NOT_PRE_ROLL,
    PROPERTY_ERR_NOT_CURRENT_TURN,
    PROPERTY_ERR_PLAYER_RESTRAINED
} PropertyCode;

typedef enum PropertyTollExemption {
    PROPERTY_TOLL_EXEMPT_NONE = 0,
    PROPERTY_TOLL_EXEMPT_FORTUNE = 1U << 0,
    PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED = 1U << 1
} PropertyTollExemption;

typedef struct PropertyResult {
    PropertyCode code;
    PropertyAction action;
    unsigned int toll_exemptions;
    int player_index;
    int owner_index;
    int position;
    int cost;
    int building_level;
    int total_investment;
    int toll;
    int amount_paid;
    int accepted;
    int player_bankrupt;
    int released_property_count;
    int sale_price;
} PropertyResult;

typedef struct PropertySellPermission {
    int is_current_turn;
    int is_pre_roll;
    int is_restrained;
} PropertySellPermission;

typedef struct PropertySystem {
    GameMap *map;
    int *money;
    size_t player_count;
    int has_pending;
    PropertyAction pending_action;
    int pending_player_index;
    int pending_position;
    int pending_cost;
} PropertySystem;

PropertyCode property_system_init(PropertySystem *system,
                                  GameMap *map,
                                  int *money,
                                  size_t player_count);

PropertyCode property_after_move(PropertySystem *system,
                                 int player_index,
                                 int position,
                                 unsigned int toll_exemptions,
                                 PropertyResult *result);

PropertyCode property_resolve_pending(PropertySystem *system,
                                      int player_index,
                                      int accept,
                                      PropertyResult *result);

PropertyCode property_resolve_answer(PropertySystem *system,
                                     int player_index,
                                     const char *answer,
                                     PropertyResult *result);

PropertyCode property_sell(PropertySystem *system,
                           int player_index,
                           int position,
                           PropertyResult *result);

PropertyCode property_sell_checked(PropertySystem *system,
                                   int player_index,
                                   int position,
                                   PropertySellPermission permission,
                                   PropertyResult *result);

PropertyCode property_parse_position(const char *text, int *position);
PropertyCode property_parse_money(const char *text, int *money);
PropertyCode property_parse_sell_command(const char *text, int *position);

int property_release_player_properties(PropertySystem *system,
                                       int player_index);
int property_count_player_properties(const PropertySystem *system,
                                     int player_index);
int property_has_pending(const PropertySystem *system);
PropertyAction property_pending_action(const PropertySystem *system);
const char *property_code_string(PropertyCode code);

#ifdef __cplusplus
}
#endif

#endif
