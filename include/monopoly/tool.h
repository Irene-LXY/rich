#ifndef MONOPOLY_TOOL_H
#define MONOPOLY_TOOL_H

#include <stdbool.h>
#include <stddef.h>

#define MONOPOLY_BOARD_SIZE 70
#define MONOPOLY_MAX_PLAYERS 4
#define MONOPOLY_MAX_TOOLS_PER_PLAYER 10
#define MONOPOLY_HOSPITAL_POSITION 14
#define MONOPOLY_PRISON_POSITION 49

typedef enum {
    TOOL_NONE = 0,
    TOOL_BLOCK = 1,
    TOOL_ROBOT = 2,
    TOOL_BOMB = 3,
    TOOL_TYPE_COUNT = 4
} ToolType;

typedef struct {
    int money;
    int points;
    int position;
    int hospital_turns;
    unsigned int tools[TOOL_TYPE_COUNT];
} MonopolyPlayer;

typedef struct {
    ToolType placed_tool;
    int mine_reward;
} MonopolyBoardCell;

typedef struct {
    MonopolyBoardCell cells[MONOPOLY_BOARD_SIZE];
} MonopolyBoard;

typedef enum {
    TOOL_RESULT_OK = 0,
    TOOL_RESULT_INVALID_PLAYER,
    TOOL_RESULT_INVALID_BOARD,
    TOOL_RESULT_INVALID_TOOL,
    TOOL_RESULT_NOT_ENOUGH_POINTS,
    TOOL_RESULT_INVENTORY_FULL,
    TOOL_RESULT_TOOL_NOT_OWNED,
    TOOL_RESULT_INVALID_DISTANCE,
    TOOL_RESULT_POSITION_OCCUPIED
} ToolResult;

typedef enum {
    MOVE_COMPLETED = 0,
    MOVE_STOPPED_BY_BLOCK,
    MOVE_SENT_TO_HOSPITAL,
    MOVE_INVALID
} MovementStopReason;

typedef struct {
    MovementStopReason reason;
    int requested_steps;
    int moved_steps;
    int triggered_position;
    int mine_points_awarded;
} MovementResult;

void monopoly_player_init(MonopolyPlayer *player, int money, int points, int position);
void monopoly_board_init(MonopolyBoard *board);
int monopoly_wrap_position(int position);
int monopoly_tool_price(ToolType tool);
unsigned int monopoly_player_total_tools(const MonopolyPlayer *player);
int monopoly_board_mine_reward(const MonopolyBoard *board, int position);

ToolResult monopoly_purchase_tool(MonopolyPlayer *player, ToolType tool);
ToolResult monopoly_can_purchase_tool(const MonopolyPlayer *player, ToolType tool);
bool monopoly_tool_shop_should_auto_exit(const MonopolyPlayer *player);
ToolResult monopoly_place_tool(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    ToolType tool,
    int relative_distance,
    int *placed_position
);
ToolResult monopoly_use_robot(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    unsigned int *removed_count
);

int monopoly_apply_mine_reward(const MonopolyBoard *board, MonopolyPlayer *player);
MovementResult monopoly_move_player(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    int steps
);

#endif
