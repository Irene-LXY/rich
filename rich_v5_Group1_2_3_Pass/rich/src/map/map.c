#include "map/map.h"

#include <stdio.h>

static MapCell make_cell(int index, CellType type, int price, int points) {
    MapCell cell;
    cell.index = index;
    cell.type = type;
    cell.land_price = price;
    cell.mine_points = points;
    cell.owner_id = RICH_NO_OWNER;
    cell.building_level = 0;
    cell.has_block = 0;
    cell.has_fortune = 0;
    return cell;
}

void game_map_init(GameMap *map) {
    int i;
    if (map == NULL) return;

    for (i = 0; i < RICH_MAP_SIZE; ++i) {
        map->cells[i] = make_cell(i, CELL_LAND, 0, 0);
    }

    /* 上边：S + 13块地 + P + 13块地 + T（地段1，共26块）。 */
    map->cells[0] = make_cell(0, CELL_START, 0, 0);
    for (i = 1; i <= 13; ++i) map->cells[i] = make_cell(i, CELL_LAND, 200, 0);
    map->cells[14] = make_cell(14, CELL_PARK, 0, 0);
    for (i = 15; i <= 27; ++i) map->cells[i] = make_cell(i, CELL_LAND, 200, 0);
    map->cells[28] = make_cell(28, CELL_TOOL_SHOP, 0, 0);

    /* 右边：地段2，6块黄金地段。 */
    for (i = 29; i <= 34; ++i) map->cells[i] = make_cell(i, CELL_LAND, 500, 0);

    /* 下边按顺时针方向从右向左：G + 13块地 + P + 13块地 + P。 */
    map->cells[35] = make_cell(35, CELL_GIFT_SHOP, 0, 0);
    for (i = 36; i <= 48; ++i) map->cells[i] = make_cell(i, CELL_LAND, 300, 0);
    map->cells[49] = make_cell(49, CELL_PARK, 0, 0);
    for (i = 50; i <= 62; ++i) map->cells[i] = make_cell(i, CELL_LAND, 300, 0);
    map->cells[63] = make_cell(63, CELL_PARK, 0, 0);

    /* 左边编号从下向上递增；奖励按画面从上到下排列。 */
    map->cells[64] = make_cell(64, CELL_MINE, 0, 60);
    map->cells[65] = make_cell(65, CELL_MINE, 0, 80);
    map->cells[66] = make_cell(66, CELL_MINE, 0, 40);
    map->cells[67] = make_cell(67, CELL_MINE, 0, 100);
    map->cells[68] = make_cell(68, CELL_MINE, 0, 80);
    map->cells[69] = make_cell(69, CELL_MINE, 0, 20);
}

const MapCell *game_map_cell_at(const GameMap *map, int index) {
    if (map == NULL || index < 0 || index >= RICH_MAP_SIZE) return NULL;
    return &map->cells[index];
}

MapCell *game_map_cell_at_mut(GameMap *map, int index) {
    if (map == NULL || index < 0 || index >= RICH_MAP_SIZE) return NULL;
    return &map->cells[index];
}

int game_map_normalize_position(int position) {
    int result = position % RICH_MAP_SIZE;
    return result < 0 ? result + RICH_MAP_SIZE : result;
}

int game_map_destination(int current_position, int steps) {
    return game_map_normalize_position(
        game_map_normalize_position(current_position) + steps);
}

int game_map_screen_position(int map_index, int *x, int *y) {
    if (map_index < 0 || map_index >= RICH_MAP_SIZE || x == NULL || y == NULL) return 0;
    if (map_index <= 28) {
        *x = map_index;
        *y = 0;
    } else if (map_index <= 34) {
        *x = RICH_MAP_WIDTH - 1;
        *y = map_index - 28;
    } else if (map_index <= 63) {
        *x = 63 - map_index;
        *y = RICH_MAP_HEIGHT - 1;
    } else {
        *x = 0;
        *y = 70 - map_index;
    }
    return 1;
}

char game_map_base_symbol(const GameMap *map, int index) {
    const MapCell *cell = game_map_cell_at(map, index);
    if (cell == NULL) return '?';
    if (cell->has_block) return '#';
    if (cell->has_fortune) return 'F';
    switch (cell->type) {
        case CELL_START:       return 'S';
        case CELL_LAND:
            return (cell->building_level >= 0 && cell->building_level <= 3)
                ? (char)('0' + cell->building_level) : '0';
        case CELL_TOOL_SHOP:   return 'T';
        case CELL_GIFT_SHOP:   return 'G';
        case CELL_PARK:        return 'P';
        case CELL_MINE:        return '$';
        default:               return '?';
    }
}

const char *game_map_cell_type_name(CellType type) {
    switch (type) {
        case CELL_START:       return "起点";
        case CELL_LAND:        return "空地";
        case CELL_TOOL_SHOP:   return "道具屋";
        case CELL_GIFT_SHOP:   return "礼品屋";
        case CELL_PARK:        return "公园";
        case CELL_MINE:        return "矿地";
        default:               return "未知格子";
    }
}

int game_map_cell_description(const GameMap *map,
                              int index,
                              char *buffer,
                              size_t buffer_size) {
    const MapCell *cell = game_map_cell_at(map, index);
    int written;
    if (cell == NULL || buffer == NULL || buffer_size == 0) return 0;
    if (cell->type == CELL_LAND) {
        written = snprintf(buffer, buffer_size, "空地（价格%d元）", cell->land_price);
    } else if (cell->type == CELL_MINE) {
        written = snprintf(buffer, buffer_size, "矿地（%d点）", cell->mine_points);
    } else {
        written = snprintf(buffer, buffer_size, "%s", game_map_cell_type_name(cell->type));
    }
    return written >= 0 && (size_t)written < buffer_size;
}
