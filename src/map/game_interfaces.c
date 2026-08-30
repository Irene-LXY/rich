#include "map/game_interfaces.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct DisplayCell {
    char symbol;
    ConsoleColor color;
    int colored;
} DisplayCell;

static const char *ansi_code(ConsoleColor color) {
    switch (color) {
        case COLOR_RED:    return "\033[31m";
        case COLOR_GREEN:  return "\033[32m";
        case COLOR_BLUE:   return "\033[34m";
        case COLOR_YELLOW: return "\033[33m";
        default:           return "\033[0m";
    }
}

static uint32_t next_random(RandomDice *dice) {
    uint32_t x = dice->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    dice->state = x;
    return x;
}

static int random_dice_roll_impl(void *context) {
    RandomDice *dice = (RandomDice *)context;
    if (dice == NULL) return 0;
    return (int)(next_random(dice) % 6U) + 1;
}

void random_dice_init(RandomDice *dice, uint32_t seed) {
    if (dice == NULL) return;
    if (seed == 0) seed = (uint32_t)time(NULL);
    /* xorshift32不能使用0作为内部状态。 */
    dice->state = seed == 0 ? 0x6D2B79F5U : seed;
}

Dice random_dice_as_interface(RandomDice *dice) {
    Dice interface_value;
    interface_value.context = dice;
    interface_value.roll = random_dice_roll_impl;
    return interface_value;
}

int dice_roll(Dice *dice) {
    if (dice == NULL || dice->roll == NULL) return 0;
    return dice->roll(dice->context);
}

MoveContext move_player(const GameMap *map,
                        PlayerToken *player,
                        int steps,
                        EnterCellHandler on_enter,
                        void *user_data) {
    MoveContext context = {steps, 0, 0};
    int direction;
    int count;
    int i;
    if (map == NULL || player == NULL) {
        context.interrupted = 1;
        return context;
    }
    direction = steps >= 0 ? 1 : -1;
    count = steps >= 0 ? steps : -steps;
    player->position = game_map_normalize_position(player->position);
    for (i = 0; i < count; ++i) {
        const MapCell *cell;
        player->position = game_map_normalize_position(player->position + direction);
        ++context.completed_steps;
        cell = game_map_cell_at(map, player->position);
        if (on_enter != NULL && !on_enter(player, cell, &context, user_data)) {
            context.interrupted = 1;
            break;
        }
    }
    return context;
}

static int append_text(char *buffer, size_t size, size_t *used, const char *text) {
    size_t length = strlen(text);
    if (*used + length >= size) return 0;
    memcpy(buffer + *used, text, length);
    *used += length;
    buffer[*used] = '\0';
    return 1;
}

static int append_char(char *buffer, size_t size, size_t *used, char value) {
    if (*used + 1 >= size) return 0;
    buffer[(*used)++] = value;
    buffer[*used] = '\0';
    return 1;
}

int render_map(const GameMap *map,
               const PlayerToken *players,
               size_t player_count,
               int use_ansi_color,
               int show_indices,
               char *buffer,
               size_t buffer_size) {
    DisplayCell canvas[RICH_MAP_HEIGHT][RICH_MAP_WIDTH];
    int occupant_count[RICH_MAP_SIZE] = {0};
    const PlayerToken *first_occupant[RICH_MAP_SIZE] = {NULL};
    size_t used = 0;
    size_t i;
    int x;
    int y;
    if (map == NULL || buffer == NULL || buffer_size == 0 ||
        (player_count > 0 && players == NULL)) return 0;
    buffer[0] = '\0';
    for (y = 0; y < RICH_MAP_HEIGHT; ++y) {
        for (x = 0; x < RICH_MAP_WIDTH; ++x) {
            canvas[y][x].symbol = ' ';
            canvas[y][x].color = COLOR_DEFAULT;
            canvas[y][x].colored = 0;
        }
    }
    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const MapCell *cell = game_map_cell_at(map, (int)i);
        size_t player_index;
        game_map_screen_position((int)i, &x, &y);
        canvas[y][x].symbol = game_map_base_symbol(map, (int)i);
        if (cell == NULL || cell->type != CELL_LAND ||
            cell->owner_id == RICH_NO_OWNER) {
            continue;
        }
        for (player_index = 0; player_index < player_count; ++player_index) {
            if (players[player_index].id == cell->owner_id) {
                canvas[y][x].color = players[player_index].color;
                canvas[y][x].colored = 1;
                break;
            }
        }
    }
    for (i = 0; i < player_count; ++i) {
        int position;
        if (!players[i].active) continue;
        position = game_map_normalize_position(players[i].position);
        if (occupant_count[position] == 0) first_occupant[position] = &players[i];
        ++occupant_count[position];
    }
    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const PlayerToken *player;
        if (occupant_count[i] == 0) continue;
        game_map_screen_position((int)i, &x, &y);
        player = first_occupant[i];
        /* 多人同格时显示调用方排在最前的玩家，运行时会把当前玩家排首位。 */
        canvas[y][x].symbol = player->symbol;
        canvas[y][x].color = player->color;
        canvas[y][x].colored = 1;
    }
    for (y = 0; y < RICH_MAP_HEIGHT; ++y) {
        for (x = 0; x < RICH_MAP_WIDTH; ++x) {
            DisplayCell *display = &canvas[y][x];
            if (use_ansi_color && display->colored) {
                if (!append_text(buffer, buffer_size, &used, ansi_code(display->color)) ||
                    !append_char(buffer, buffer_size, &used, display->symbol) ||
                    !append_text(buffer, buffer_size, &used, "\033[0m")) return 0;
            } else if (!append_char(buffer, buffer_size, &used, display->symbol)) return 0;
        }
        if (!append_char(buffer, buffer_size, &used, '\n')) return 0;
    }
    if (show_indices && !append_text(
            buffer, buffer_size, &used,
            "\n格子编号：沿顺时针方向依次为 0～69。\n"
            "关键位置：S=0, H=14, T=28, G=35, P=49, M=63, 矿地=64～69\n")) return 0;
    return 1;
}
