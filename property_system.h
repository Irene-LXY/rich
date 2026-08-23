#ifndef RICH_A9_A10_A11_PROPERTY_SYSTEM_H
#define RICH_A9_A10_A11_PROPERTY_SYSTEM_H

#include "roll.h"

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
    PROPERTY_ERR_PENDING_DECISION,
    PROPERTY_ERR_PRICE_NOT_SET,
    PROPERTY_ERR_INSUFFICIENT_FUNDS,
    PROPERTY_ERR_MAX_LEVEL,
    PROPERTY_ERR_NOT_OWNER,
    PROPERTY_ERR_NOT_PRE_ROLL
} PropertyCode;

typedef enum TollOutcome {
    TOLL_NONE = 0,
    TOLL_PAID,
    TOLL_EXEMPT_OWNER_RESTRAINED,
    TOLL_EXEMPT_FORTUNE
} TollOutcome;

typedef struct PropertyResult {
    PropertyCode code;
    PropertyAction action;
    TollOutcome toll_outcome;
    int player_index;
    int owner_index;
    int position;
    int cost;
    int building_level;
    int total_investment;
    int toll;
    int sale_price;
} PropertyResult;

/*
 * A9/A10/A11 的独立状态。
 * 不向 A8 的 GameState/Player/Cell 增加字段，保证 roll.h 接口不变。
 */
typedef struct PropertySystem {
    int base_price[BOARD_SIZE];
    unsigned int fortune_turns[MAX_PLAYERS];
    PropertyAction pending_action;
    int pending_player_index;
    int pending_position;
    int pending_cost;
} PropertySystem;

/* 在地图和玩家初始化完成后调用一次。 */
void property_system_init(PropertySystem *system, const GameState *gs);

/*
 * 配置一块无主土地及其基础地价。
 * A9/A10/A11 需要基础地价；调用后 Cell.value 保存该地价。
 */
PropertyCode property_configure_land(PropertySystem *system,
                                     GameState *gs,
                                     int position,
                                     int price);

/*
 * A8 成功移动后的接入点。
 * moving_player_index 必须是在调用 roll_handle_command 前保存的当前玩家下标。
 */
PropertyCode property_after_move(PropertySystem *system,
                                 GameState *gs,
                                 int moving_player_index,
                                 const MoveResult *move,
                                 PropertyResult *result);

/* 处理 A9 的 Y/N：accept 非 0 为 Y，0 为 N。 */
PropertyCode property_resolve_pending(PropertySystem *system,
                                      GameState *gs,
                                      int accept,
                                      PropertyResult *result);

/* A11：当前玩家掷骰前出售指定绝对位置的自有房产。 */
PropertyCode property_sell(PropertySystem *system,
                           GameState *gs,
                           int player_index,
                           int position,
                           PropertyResult *result);

int property_has_pending(const PropertySystem *system);
int property_pending_player(const PropertySystem *system);

/* 上层应在 Roll 前调用；有 Y/N 未处理时返回 0。 */
int property_can_roll(const PropertySystem *system,
                      const GameState *gs,
                      int player_index);

/* 礼品屋 Story 的财神对接接口。 */
PropertyCode property_set_fortune_turns(PropertySystem *system,
                                        int player_index,
                                        unsigned int turns);
unsigned int property_get_fortune_turns(const PropertySystem *system,
                                        int player_index);

const char *property_code_string(PropertyCode code);

#endif
