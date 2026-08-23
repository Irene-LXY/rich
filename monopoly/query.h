#ifndef MONOPOLY_QUERY_H
#define MONOPOLY_QUERY_H

#include <stddef.h>

#define QUERY_MAX_PROPERTIES 58
#define QUERY_ITEM_TYPE_COUNT 3

typedef enum QueryItemType {
    QUERY_ITEM_BLOCK = 0,
    QUERY_ITEM_ROBOT,
    QUERY_ITEM_BOMB
} QueryItemType;

typedef struct QueryProperty {
    int position;
    int land_price;
    int building_level;
} QueryProperty;

/*
 * Query只读取这份视图，不直接依赖地图、回合或道具模块。
 * 后续模块更新运行时状态后，由集成层生成该视图即可显示。
 */
typedef struct QueryPlayerState {
    int player_id;
    const char *player_name;
    char symbol;
    int money;
    int points;
    int position;
    QueryProperty properties[QUERY_MAX_PROPERTIES];
    size_t property_count;
    int item_counts[QUERY_ITEM_TYPE_COUNT];
    int fortune_turns;
    int hospital_turns;
    int prison_turns;
    int bankrupt;
} QueryPlayerState;

const char *query_item_name(QueryItemType type);
const char *query_building_name(int level);

/* 成功返回0；参数无效或输出缓冲不足返回非0。 */
int query_format_player(const QueryPlayerState *state,
                        char *message,
                        size_t message_size);

#endif
