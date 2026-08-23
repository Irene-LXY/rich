#ifndef RICH_MAP_H
#define RICH_MAP_H

#include <stddef.h>

#define RICH_MAP_SIZE 70
#define RICH_MAP_WIDTH 29
#define RICH_MAP_HEIGHT 8
#define RICH_NO_OWNER (-1)

/* 地图格子的真实类型。显示角色时只覆盖画面，不修改这里的数据。 */
typedef enum CellType {
    CELL_START,
    CELL_LAND,
    CELL_TOOL_SHOP,
    CELL_GIFT_SHOP,
    CELL_MAGIC_HOUSE,
    CELL_HOSPITAL,
    CELL_PRISON,
    CELL_MINE
} CellType;

typedef struct MapCell {
    int index;
    CellType type;
    int land_price;
    int mine_points;
    int owner_id;
    int building_level;
    int has_block;
    int has_bomb;
} MapCell;

typedef struct GameMap {
    MapCell cells[RICH_MAP_SIZE];
} GameMap;

void game_map_init(GameMap *map);
const MapCell *game_map_cell_at(const GameMap *map, int index);
MapCell *game_map_cell_at_mut(GameMap *map, int index);
int game_map_normalize_position(int position);
int game_map_destination(int current_position, int steps);
int game_map_screen_position(int map_index, int *x, int *y);
char game_map_base_symbol(const GameMap *map, int index);
const char *game_map_cell_type_name(CellType type);
int game_map_cell_description(const GameMap *map,
                              int index,
                              char *buffer,
                              size_t buffer_size);

#endif
