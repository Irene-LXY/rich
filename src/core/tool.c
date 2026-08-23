#include "monopoly/tool.h"

#include <string.h>

static int is_placeable_tool(ToolType tool) {
    return tool == TOOL_BLOCK || tool == TOOL_BOMB;
}

void monopoly_player_init(MonopolyPlayer *player, int money, int points, int position) {
    if (player == NULL) {
        return;
    }
    player->money = money;
    player->points = points;
    player->position = monopoly_wrap_position(position);
    player->hospital_turns = 0;
    (void)memset(player->tools, 0, sizeof(player->tools));
}

void monopoly_board_init(MonopolyBoard *board) {
    int index;
    static const int mine_positions[6] = {69, 68, 67, 66, 65, 64};
    static const int mine_rewards[6] = {20, 80, 100, 40, 80, 60};

    if (board == NULL) {
        return;
    }
    for (index = 0; index < MONOPOLY_BOARD_SIZE; index++) {
        board->cells[index].placed_tool = TOOL_NONE;
        board->cells[index].mine_reward = 0;
    }
    for (index = 0; index < 6; index++) {
        board->cells[mine_positions[index]].mine_reward = mine_rewards[index];
    }
}

int monopoly_wrap_position(int position) {
    int wrapped = position % MONOPOLY_BOARD_SIZE;
    if (wrapped < 0) {
        wrapped += MONOPOLY_BOARD_SIZE;
    }
    return wrapped;
}

int monopoly_tool_price(ToolType tool) {
    switch (tool) {
        case TOOL_BLOCK:
            return 50;
        case TOOL_ROBOT:
            return 30;
        case TOOL_BOMB:
            return 50;
        default:
            return -1;
    }
}

unsigned int monopoly_player_total_tools(const MonopolyPlayer *player) {
    unsigned int total = 0;
    int tool;
    if (player == NULL) {
        return 0;
    }
    for (tool = TOOL_BLOCK; tool < TOOL_TYPE_COUNT; tool++) {
        total += player->tools[tool];
    }
    return total;
}

int monopoly_board_mine_reward(const MonopolyBoard *board, int position) {
    if (board == NULL || position < 0 || position >= MONOPOLY_BOARD_SIZE) {
        return 0;
    }
    return board->cells[position].mine_reward;
}

ToolResult monopoly_can_purchase_tool(const MonopolyPlayer *player, ToolType tool) {
    int price;
    if (player == NULL) {
        return TOOL_RESULT_INVALID_PLAYER;
    }
    price = monopoly_tool_price(tool);
    if (price < 0) {
        return TOOL_RESULT_INVALID_TOOL;
    }
    if (monopoly_player_total_tools(player) >= MONOPOLY_MAX_TOOLS_PER_PLAYER) {
        return TOOL_RESULT_INVENTORY_FULL;
    }
    if (player->points < price) {
        return TOOL_RESULT_NOT_ENOUGH_POINTS;
    }
    return TOOL_RESULT_OK;
}

bool monopoly_tool_shop_should_auto_exit(const MonopolyPlayer *player) {
    if (player == NULL) {
        return true;
    }
    return monopoly_player_total_tools(player) >= MONOPOLY_MAX_TOOLS_PER_PLAYER ||
        player->points < monopoly_tool_price(TOOL_ROBOT);
}

ToolResult monopoly_purchase_tool(MonopolyPlayer *player, ToolType tool) {
    ToolResult availability = monopoly_can_purchase_tool(player, tool);
    int price;
    if (availability != TOOL_RESULT_OK) {
        return availability;
    }
    price = monopoly_tool_price(tool);
    player->points -= price;
    player->tools[tool]++;
    return TOOL_RESULT_OK;
}

ToolResult monopoly_place_tool(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    ToolType tool,
    int relative_distance,
    int *placed_position
) {
    int target;
    if (board == NULL) {
        return TOOL_RESULT_INVALID_BOARD;
    }
    if (player == NULL) {
        return TOOL_RESULT_INVALID_PLAYER;
    }
    if (!is_placeable_tool(tool)) {
        return TOOL_RESULT_INVALID_TOOL;
    }
    if (relative_distance < -10 || relative_distance > 10) {
        return TOOL_RESULT_INVALID_DISTANCE;
    }
    if (player->tools[tool] == 0) {
        return TOOL_RESULT_TOOL_NOT_OWNED;
    }
    target = monopoly_wrap_position(player->position + relative_distance);
    if (board->cells[target].placed_tool != TOOL_NONE) {
        return TOOL_RESULT_POSITION_OCCUPIED;
    }
    board->cells[target].placed_tool = tool;
    player->tools[tool]--;
    if (placed_position != NULL) {
        *placed_position = target;
    }
    return TOOL_RESULT_OK;
}

ToolResult monopoly_use_robot(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    unsigned int *removed_count
) {
    unsigned int removed = 0;
    int distance;
    if (board == NULL) {
        return TOOL_RESULT_INVALID_BOARD;
    }
    if (player == NULL) {
        return TOOL_RESULT_INVALID_PLAYER;
    }
    if (player->tools[TOOL_ROBOT] == 0) {
        return TOOL_RESULT_TOOL_NOT_OWNED;
    }
    for (distance = 1; distance <= 10; distance++) {
        int position = monopoly_wrap_position(player->position + distance);
        if (board->cells[position].placed_tool == TOOL_BLOCK ||
            board->cells[position].placed_tool == TOOL_BOMB) {
            board->cells[position].placed_tool = TOOL_NONE;
            removed++;
        }
    }
    player->tools[TOOL_ROBOT]--;
    if (removed_count != NULL) {
        *removed_count = removed;
    }
    return TOOL_RESULT_OK;
}

int monopoly_apply_mine_reward(const MonopolyBoard *board, MonopolyPlayer *player) {
    int reward;
    if (board == NULL || player == NULL || player->position < 0 ||
        player->position >= MONOPOLY_BOARD_SIZE) {
        return 0;
    }
    reward = monopoly_board_mine_reward(board, player->position);
    if (reward > 0) {
        player->points += reward;
    }
    return reward;
}

MovementResult monopoly_move_player(
    MonopolyBoard *board,
    MonopolyPlayer *player,
    int steps
) {
    MovementResult result = {MOVE_INVALID, steps, 0, -1, 0};
    int step;

    if (board == NULL || player == NULL || steps < 0) {
        return result;
    }
    result.reason = MOVE_COMPLETED;
    if (steps == 0) {
        return result;
    }
    for (step = 0; step < steps; step++) {
        ToolType encountered;
        player->position = monopoly_wrap_position(player->position + 1);
        result.moved_steps++;
        encountered = board->cells[player->position].placed_tool;
        if (encountered == TOOL_BLOCK) {
            result.triggered_position = player->position;
            board->cells[player->position].placed_tool = TOOL_NONE;
            result.reason = MOVE_STOPPED_BY_BLOCK;
            result.mine_points_awarded = monopoly_apply_mine_reward(board, player);
            return result;
        }
        if (encountered == TOOL_BOMB) {
            result.triggered_position = player->position;
            board->cells[player->position].placed_tool = TOOL_NONE;
            player->position = MONOPOLY_HOSPITAL_POSITION;
            player->hospital_turns = 3;
            result.reason = MOVE_SENT_TO_HOSPITAL;
            return result;
        }
    }
    result.mine_points_awarded = monopoly_apply_mine_reward(board, player);
    return result;
}
