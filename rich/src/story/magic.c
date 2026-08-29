#include "monopoly/magic.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int valid_player(const MagicHouseState *state, size_t player_index)
{
    return state != NULL && player_index < state->player_count;
}

static int is_exit_answer(const char *answer)
{
    const unsigned char *cursor = (const unsigned char *)answer;

    while (*cursor != '\0' && isspace(*cursor)) {
        ++cursor;
    }
    if (tolower(*cursor) != 'f') {
        return 0;
    }
    ++cursor;
    while (*cursor != '\0' && isspace(*cursor)) {
        ++cursor;
    }
    return *cursor == '\0';
}

static int parse_effect_id(const char *answer, int *effect_id)
{
    const char *cursor = answer;
    char *end;
    long parsed;

    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    errno = 0;
    parsed = strtol(cursor, &end, 10);
    if (cursor == end || errno == ERANGE || parsed <= 0 || parsed > INT_MAX) {
        return 0;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return 0;
    }
    *effect_id = (int)parsed;
    return 1;
}

void magic_house_init(MagicHouseState *state, size_t player_count)
{
    if (state == NULL) {
        return;
    }
    (void)memset(state, 0, sizeof(*state));
    state->player_count = player_count;
    state->active_player = player_count;
    state->last_target.type = MAGIC_TARGET_NONE;
}

MagicCode magic_house_begin(MagicHouseState *state, size_t acting_player)
{
    if (!valid_player(state, acting_player)) {
        return MAGIC_ERR_INVALID_ARGUMENT;
    }
    if (state->is_open) {
        return MAGIC_ERR_ALREADY_OPEN;
    }
    state->active_player = acting_player;
    state->is_open = 1;
    state->exited_without_effect = 0;
    state->last_effect_id = 0;
    state->last_target.type = MAGIC_TARGET_NONE;
    state->last_target.value = 0;
    return MAGIC_OK;
}

MagicCode magic_house_answer(MagicHouseState *state,
                             const char *answer,
                             const MagicEffect *effects,
                             size_t effect_count,
                             const MagicTarget *target)
{
    MagicTarget empty_target = {MAGIC_TARGET_NONE, 0};
    const MagicEffect *effect = NULL;
    size_t index;
    int effect_id;

    if (state == NULL || answer == NULL ||
        (effect_count > 0U && effects == NULL)) {
        return MAGIC_ERR_INVALID_ARGUMENT;
    }
    if (!state->is_open || !valid_player(state, state->active_player)) {
        return MAGIC_ERR_NOT_OPEN;
    }
    if (is_exit_answer(answer)) {
        state->is_open = 0;
        state->exited_without_effect = 1;
        return MAGIC_EXIT;
    }
    if (!parse_effect_id(answer, &effect_id)) {
        return MAGIC_INVALID_CHOICE;
    }
    for (index = 0; index < effect_count; ++index) {
        if (effects[index].id == effect_id) {
            effect = &effects[index];
            break;
        }
    }
    if (effect == NULL || effect->handler == NULL) {
        return MAGIC_INVALID_CHOICE;
    }
    if (target == NULL) {
        target = &empty_target;
    }
    if (effect->handler(state->active_player, effect_id,
                        target, effect->context) != 0) {
        return MAGIC_EFFECT_REJECTED;
    }
    state->is_open = 0;
    state->last_effect_id = effect_id;
    state->last_target = *target;
    return MAGIC_OK;
}
