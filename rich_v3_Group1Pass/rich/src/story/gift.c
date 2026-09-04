#include "monopoly/gift.h"

#include <ctype.h>
#include <limits.h>
#include <string.h>

static int valid_player(const GiftShopState *state, size_t player_index)
{
    return state != NULL && player_index < state->player_count;
}

static int equals_ignore_case_trimmed(const char *text, const char *expected)
{
    const unsigned char *left = (const unsigned char *)text;
    const unsigned char *right = (const unsigned char *)expected;

    while (*left != '\0' && isspace(*left)) {
        ++left;
    }
    while (*left != '\0' && *right != '\0' &&
           tolower(*left) == tolower(*right)) {
        ++left;
        ++right;
    }
    while (*left != '\0' && isspace(*left)) {
        ++left;
    }
    return *left == '\0' && *right == '\0';
}

static int parse_choice(const char *answer)
{
    const unsigned char *cursor = (const unsigned char *)answer;
    int choice;

    while (*cursor != '\0' && isspace(*cursor)) {
        ++cursor;
    }
    if (*cursor < '1' || *cursor > '3') {
        return 0;
    }
    choice = *cursor - '0';
    ++cursor;
    while (*cursor != '\0' && isspace(*cursor)) {
        ++cursor;
    }
    return *cursor == '\0' ? choice : 0;
}

void gift_shop_init(GiftShopState *state, size_t player_count)
{
    if (state == NULL) {
        return;
    }
    (void)memset(state, 0, sizeof(*state));
    state->player_count = player_count <= GIFT_MAX_PLAYERS
        ? player_count : GIFT_MAX_PLAYERS;
    state->active_player = GIFT_MAX_PLAYERS;
}

GiftCode gift_shop_begin(GiftShopState *state, size_t acting_player)
{
    if (!valid_player(state, acting_player)) {
        return GIFT_ERR_INVALID_ARGUMENT;
    }
    if (state->is_open) {
        return GIFT_ERR_ALREADY_OPEN;
    }
    state->active_player = acting_player;
    state->is_open = 1;
    return GIFT_OK;
}

GiftCode gift_shop_answer(GiftShopState *state,
                          int *player_money,
                          size_t money_count,
                          const char *answer)
{
    size_t player_index;
    int choice;

    if (state == NULL || player_money == NULL || answer == NULL) {
        return GIFT_ERR_INVALID_ARGUMENT;
    }
    if (!state->is_open || !valid_player(state, state->active_player) ||
        state->active_player >= money_count) {
        return GIFT_ERR_NOT_OPEN;
    }

    if (equals_ignore_case_trimmed(answer, "Quit")) {
        state->is_open = 0;
        return GIFT_QUIT;
    }

    choice = parse_choice(answer);
    if (choice == 0) {
        state->is_open = 0;
        return GIFT_INVALID_CHOICE;
    }

    player_index = state->active_player;
    if ((choice == 1 && player_money[player_index] > INT_MAX - GIFT_BONUS_MONEY) ||
        (choice == 2 && state->points[player_index] > INT_MAX - GIFT_BONUS_POINTS)) {
        return GIFT_ERR_OVERFLOW;
    }

    if (choice == 1) {
        player_money[player_index] += GIFT_BONUS_MONEY;
    } else if (choice == 2) {
        state->points[player_index] += GIFT_BONUS_POINTS;
    }
    state->is_open = 0;
    return choice == 3 ? GIFT_FORTUNE_SELECTED : GIFT_OK;
}

int gift_shop_points(const GiftShopState *state, size_t player_index)
{
    return valid_player(state, player_index) ? state->points[player_index] : 0;
}
