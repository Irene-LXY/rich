#ifndef MONOPOLY_FORTUNE_H
#define MONOPOLY_FORTUNE_H

#include <stddef.h>
#include <stdint.h>

#define FORTUNE_MAX_PLAYERS 4U
#define FORTUNE_EFFECT_ROUNDS 5
#define FORTUNE_INITIAL_DELAY_TURNS 10U
#define FORTUNE_MAP_LIFETIME_TURNS 5U
#define FORTUNE_RESPAWN_DELAY_TURNS 10U
#define FORTUNE_NO_POSITION (-1)

typedef enum FortuneTurnEvent {
    FORTUNE_TURN_NONE = 0,
    FORTUNE_TURN_SPAWN_DUE,
    FORTUNE_TURN_EXPIRED
} FortuneTurnEvent;

typedef struct FortuneState {
    int effect_rounds[FORTUNE_MAX_PLAYERS];
    int just_granted[FORTUNE_MAX_PLAYERS];
    size_t player_count;
    int map_position;
    uint64_t spawned_turn;
    uint64_t next_spawn_turn;
    uint64_t last_processed_turn;
} FortuneState;

void fortune_init(FortuneState *state, size_t player_count);

/* 每个玩家回合开始时调用一次；初次在第11回合提示生成，地图上保留5回合。 */
FortuneTurnEvent fortune_advance_turn(FortuneState *state,
                                      uint64_t turn_number);
int fortune_place(FortuneState *state, int position, uint64_t turn_number);
int fortune_collect(FortuneState *state,
                    int position,
                    size_t player_index,
                    uint64_t turn_number);

int fortune_position(const FortuneState *state);
int fortune_effect_rounds(const FortuneState *state, size_t player_index);
int fortune_is_toll_free(const FortuneState *state, size_t player_index);

/* 获得财神的当前回合不扣次数，之后每个该玩家完成的回合扣一次。 */
void fortune_finish_player_turn(FortuneState *state, size_t player_index);

#endif
