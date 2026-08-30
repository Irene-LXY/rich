#include "map/game_interfaces.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int stop_after_three(PlayerToken *player,
                            const MapCell *cell,
                            const MoveContext *context,
                            void *user_data) {
    int *entered = (int *)user_data;
    (void)player;
    (void)cell;
    (void)context;
    ++(*entered);
    return *entered < 3;
}

int main(void) {
    GameMap map;
    PlayerToken player = {1, "钱夫人", 'Q', COLOR_RED, 1, 1};
    char rendered[4096];
    int lands200 = 0;
    int lands500 = 0;
    int lands300 = 0;
    int mines = 0;
    int entered = 0;
    int i;
    MoveContext result;
    RandomDice random_dice;
    Dice dice;

    game_map_init(&map);
    assert(game_map_base_symbol(&map, 0) == 'S');
    assert(game_map_base_symbol(&map, 14) == 'H');
    assert(game_map_base_symbol(&map, 28) == 'T');
    assert(game_map_base_symbol(&map, 35) == 'G');
    assert(game_map_base_symbol(&map, 49) == 'P');
    assert(game_map_base_symbol(&map, 63) == 'M');
    assert(game_map_base_symbol(&map, 69) == '$');
    assert(game_map_destination(68, 4) == 2);
    assert(game_map_destination(1, -3) == 68);

    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        const MapCell *cell = game_map_cell_at(&map, i);
        if (cell->type == CELL_LAND && cell->land_price == 200) ++lands200;
        if (cell->type == CELL_LAND && cell->land_price == 500) ++lands500;
        if (cell->type == CELL_LAND && cell->land_price == 300) ++lands300;
        if (cell->type == CELL_MINE) ++mines;
    }
    assert(lands200 == 26);
    assert(lands500 == 6);
    assert(lands300 == 26);
    assert(mines == 6);
    assert(game_map_cell_at(&map, 69)->mine_points == 20);
    assert(game_map_cell_at(&map, 68)->mine_points == 80);
    assert(game_map_cell_at(&map, 67)->mine_points == 100);
    assert(game_map_cell_at(&map, 66)->mine_points == 40);
    assert(game_map_cell_at(&map, 65)->mine_points == 80);
    assert(game_map_cell_at(&map, 64)->mine_points == 60);

    assert(render_map(&map, &player, 1, 0, 0, rendered, sizeof(rendered)));
    assert(strchr(rendered, 'Q') != NULL);
    assert(game_map_base_symbol(&map, 1) == '0');

    map.cells[1].owner_id = 1;
    map.cells[1].building_level = 1;
    player.position = 2;
    assert(render_map(&map, &player, 1, 0, 1, rendered, sizeof(rendered)));
    assert(strstr(rendered, "[00:S]") != NULL);
    assert(strstr(rendered, "[01:1]") != NULL);
    assert(strstr(rendered, "[02:0/Q]") != NULL);
    assert(strstr(rendered, "[69:$]") != NULL);
    assert(render_map(&map, &player, 1, 1, 1, rendered, sizeof(rendered)));
    assert(strstr(rendered, "[01:\033[31m1\033[0m]") != NULL);
    assert(strstr(rendered,
                  "[02:0/\033[31mQ\033[0m]") != NULL);

    player.position = 1;
    result = move_player(&map, &player, 6, stop_after_three, &entered);
    assert(result.interrupted);
    assert(result.completed_steps == 3);
    assert(player.position == 4);

    random_dice_init(&random_dice, 12345U);
    dice = random_dice_as_interface(&random_dice);
    for (i = 0; i < 100; ++i) {
        int value = dice_roll(&dice);
        assert(value >= 1 && value <= 6);
    }

    printf("All C map tests passed.\n");
    return 0;
}
