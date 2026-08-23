#include "monopoly/game.h"

void game_init(Game *game) {
    if (game == 0) {
        return;
    }
    game->phase = GAME_NOT_STARTED;
    game->context = CONTEXT_TURN_START;
    game->end_reason = END_REASON_NONE;
    game->setup_step = SETUP_PLAYER_COUNT;
    game->state_revision = 0;
    game->runtime = 0;
    game->setup_player_count = 0;
    game->setup_initial_money = 0;
    game->setup_chosen[0] = 0;
    game->setup_chosen[1] = 0;
    game->setup_chosen[2] = 0;
    game->setup_chosen[3] = 0;
    game->setup_choosing = 0;
}

bool game_start(Game *game) {
    if (game == 0 || game->phase != GAME_NOT_STARTED) {
        return false;
    }
    game->phase = GAME_RUNNING;
    game->end_reason = END_REASON_NONE;
    game->setup_step = SETUP_PLAYER_COUNT;
    game->state_revision++;
    return true;
}

bool game_end(Game *game, GameEndReason reason) {
    if (game == 0 || game->phase != GAME_RUNNING || reason == END_REASON_NONE) {
        return false;
    }
    game->phase = GAME_ENDED;
    game->end_reason = reason;
    game->state_revision++;
    return true;
}

bool game_is_running(const Game *game) {
    return game != 0 && game->phase == GAME_RUNNING;
}
