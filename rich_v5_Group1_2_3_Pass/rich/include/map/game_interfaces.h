#ifndef RICH_GAME_INTERFACES_H
#define RICH_GAME_INTERFACES_H

#include "map/map.h"

#include <stddef.h>
#include <stdint.h>

typedef enum ConsoleColor {
    COLOR_DEFAULT,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW
} ConsoleColor;

/* 地图模块只依赖这份轻量角色数据。 */
typedef struct PlayerToken {
    int id;
    const char *name;
    char symbol;
    ConsoleColor color;
    int position;
    int active;
} PlayerToken;

/* C语言骰子接口：context保存实现数据，roll是掷骰子函数指针。 */
typedef struct Dice {
    void *context;
    int (*roll)(void *context);
} Dice;

typedef struct RandomDice {
    uint32_t state;
} RandomDice;

void random_dice_init(RandomDice *dice, uint32_t seed);
Dice random_dice_as_interface(RandomDice *dice);
int dice_roll(Dice *dice);
size_t random_dice_uniform(RandomDice *dice, size_t upper_bound);

typedef struct MoveContext {
    int requested_steps;
    int completed_steps;
    int interrupted;
} MoveContext;

/* 每进入一个格子调用一次。返回0可中断移动，供路障使用。 */
typedef int (*EnterCellHandler)(PlayerToken *player,
                                const MapCell *cell,
                                const MoveContext *context,
                                void *user_data);

MoveContext move_player(const GameMap *map,
                        PlayerToken *player,
                        int steps,
                        EnterCellHandler on_enter,
                        void *user_data);

int render_map(const GameMap *map,
               const PlayerToken *players,
               size_t player_count,
               int use_ansi_color,
               int show_indices,
               char *buffer,
               size_t buffer_size);

#endif
