#include "monopoly/fortune.h"

#include <string.h>

static int valid_player(const FortuneState *state, size_t player_index)
{
    return state != NULL && player_index < state->player_count;
}

void fortune_init(FortuneState *state, size_t player_count)
{
    if (state == NULL) {
        return;
    }
    (void)memset(state, 0, sizeof(*state));
    state->player_count = player_count <= FORTUNE_MAX_PLAYERS
        ? player_count : FORTUNE_MAX_PLAYERS;
    state->map_position = FORTUNE_NO_POSITION;
    /* 第1回合开始计时，完成10个玩家回合后在第11回合生成。 */
    state->next_spawn_turn = 1U + FORTUNE_INITIAL_DELAY_TURNS;
}

FortuneTurnEvent fortune_advance_turn(FortuneState *state,
                                      uint64_t turn_number)
{
    if (state == NULL || turn_number == 0U ||
        turn_number <= state->last_processed_turn) {
        return FORTUNE_TURN_NONE;
    }
    state->last_processed_turn = turn_number;

    if (state->map_position != FORTUNE_NO_POSITION &&
        turn_number >= state->spawned_turn + FORTUNE_MAP_LIFETIME_TURNS) {
        state->map_position = FORTUNE_NO_POSITION;
        state->spawned_turn = 0U;
        state->next_spawn_turn = turn_number + FORTUNE_RESPAWN_DELAY_TURNS;
        return FORTUNE_TURN_EXPIRED;
    }
    if (state->map_position == FORTUNE_NO_POSITION &&
        state->next_spawn_turn != 0U &&
        turn_number >= state->next_spawn_turn) {
        return FORTUNE_TURN_SPAWN_DUE;
    }
    return FORTUNE_TURN_NONE;
}

int fortune_place(FortuneState *state, int position, uint64_t turn_number)
{
    if (state == NULL || position < 0 || turn_number == 0U ||
        state->map_position != FORTUNE_NO_POSITION) {
        return 0;
    }
    state->map_position = position;
    state->spawned_turn = turn_number;
    state->next_spawn_turn = 0U;
    return 1;
}

int fortune_collect(FortuneState *state,
                    int position,
                    size_t player_index,
                    uint64_t turn_number)
{
    if (!valid_player(state, player_index) || turn_number == 0U ||
        state->map_position != position) {
        return 0;
    }
    state->map_position = FORTUNE_NO_POSITION;
    state->spawned_turn = 0U;
    /* 领取发生在当前回合内；“再过 10 个完整回合”后才到新的生成点。 */
    state->next_spawn_turn = turn_number + FORTUNE_RESPAWN_DELAY_TURNS + 1U;
    return fortune_grant(state, player_index);
}

int fortune_grant(FortuneState *state, size_t player_index)
{
    if (!valid_player(state, player_index)) {
        return 0;
    }
    state->effect_rounds[player_index] = FORTUNE_EFFECT_ROUNDS;
    state->just_granted[player_index] = 1;
    return 1;
}

int fortune_position(const FortuneState *state)
{
    return state != NULL ? state->map_position : FORTUNE_NO_POSITION;
}

int fortune_effect_rounds(const FortuneState *state, size_t player_index)
{
    return valid_player(state, player_index)
        ? state->effect_rounds[player_index] : 0;
}

int fortune_is_toll_free(const FortuneState *state, size_t player_index)
{
    return fortune_effect_rounds(state, player_index) > 0;
}

void fortune_finish_player_turn(FortuneState *state, size_t player_index)
{
    if (!valid_player(state, player_index)) {
        return;
    }
    if (state->just_granted[player_index]) {
        state->just_granted[player_index] = 0;
    } else if (state->effect_rounds[player_index] > 0) {
        --state->effect_rounds[player_index];
    }
}
