#include "monopoly/query.h"

#include <stdarg.h>
#include <stdio.h>

static int append_text(char *message,
                       size_t message_size,
                       size_t *used,
                       const char *format,
                       ...) {
    int written;
    va_list arguments;
    if (message == NULL || used == NULL || *used >= message_size) {
        return 1;
    }
    va_start(arguments, format);
    written = vsnprintf(message + *used, message_size - *used, format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= message_size - *used) {
        message[message_size - 1U] = '\0';
        return 1;
    }
    *used += (size_t)written;
    return 0;
}

const char *query_item_name(QueryItemType type) {
    switch (type) {
        case QUERY_ITEM_BLOCK: return "路障";
        case QUERY_ITEM_ROBOT: return "机器娃娃";
        default:               return "未知道具";
    }
}

const char *query_building_name(int level) {
    switch (level) {
        case 0: return "空地";
        case 1: return "茅屋";
        case 2: return "洋房";
        case 3: return "摩天楼";
        default: return "未知等级";
    }
}

static int query_land_segment(int land_price) {
    if (land_price == 200) return 1;
    if (land_price == 500) return 2;
    if (land_price == 300) return 3;
    return 0;
}

int query_format_player(const QueryPlayerState *state,
                        char *message,
                        size_t message_size) {
    size_t used = 0U;
    size_t i;
    int item_total = 0;
    if (state == NULL || message == NULL || message_size == 0U ||
        state->player_name == NULL ||
        state->property_count > QUERY_MAX_PROPERTIES) {
        return 1;
    }
    message[0] = '\0';
    if (append_text(message, message_size, &used, "======== 玩家资产 ========\n") ||
        append_text(message, message_size, &used, "玩家：%s（%c，%d号）\n",
                    state->player_name, state->symbol, state->player_id) ||
        append_text(message, message_size, &used, "资金：%d 元\n", state->money) ||
        append_text(message, message_size, &used, "点数：%d 点\n", state->points) ||
        append_text(message, message_size, &used, "当前位置：%d\n", state->position) ||
        append_text(message, message_size, &used, "房产：\n")) {
        return 1;
    }

    if (state->property_count == 0U &&
        append_text(message, message_size, &used, "  无\n")) {
        return 1;
    }
    for (i = 0U; i < state->property_count; ++i) {
        const QueryProperty *property = &state->properties[i];
        if (append_text(message, message_size, &used,
                        "  - 位置%d：地段%d，%s（等级%d，地价%d元）\n",
                        property->position,
                        query_land_segment(property->land_price),
                        query_building_name(property->building_level),
                        property->building_level,
                        property->land_price)) {
            return 1;
        }
    }
    for (i = 0U; i < QUERY_ITEM_TYPE_COUNT; ++i) {
        item_total += state->item_counts[i];
    }
    if (append_text(message, message_size, &used, "房产总数：%u\n",
                    (unsigned int)state->property_count) ||
        append_text(message, message_size, &used, "剩余道具：%d/10\n", item_total)) {
        return 1;
    }
    for (i = 0U; i < QUERY_ITEM_TYPE_COUNT; ++i) {
        if (append_text(message, message_size, &used, "  - %s：%d 个\n",
                        query_item_name((QueryItemType)i), state->item_counts[i])) {
            return 1;
        }
    }
    if (append_text(message, message_size, &used, "状态剩余轮数：\n") ||
        append_text(message, message_size, &used, "  - 财神：%d 回合\n",
                    state->fortune_turns) ||
        append_text(message, message_size, &used, "是否破产：%s\n",
                    state->bankrupt ? "是" : "否") ||
        append_text(message, message_size, &used, "==========================\n")) {
        return 1;
    }
    return 0;
}
