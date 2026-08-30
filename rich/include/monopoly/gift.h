#ifndef MONOPOLY_GIFT_H
#define MONOPOLY_GIFT_H

#include <stddef.h>

#define GIFT_MAX_PLAYERS 4U
#define GIFT_BONUS_MONEY 2000
#define GIFT_BONUS_POINTS 200
#define GIFT_GOD_OF_WEALTH_ROUNDS 5

typedef enum GiftCode {
    GIFT_OK = 0,
    GIFT_INVALID_CHOICE,
    GIFT_QUIT,
    GIFT_TOLL_EXEMPTED,
    GIFT_NO_EXEMPTION,
    GIFT_ERR_INVALID_ARGUMENT = -1,
    GIFT_ERR_NOT_OPEN = -2,
    GIFT_ERR_ALREADY_OPEN = -3,
    GIFT_ERR_OVERFLOW = -4
} GiftCode;

/* A15 只保存礼品屋新增的数据；玩家资金仍由 GameRuntime 统一持有。 */
typedef struct GiftShopState {
    int points[GIFT_MAX_PLAYERS];
    int god_of_wealth_rounds[GIFT_MAX_PLAYERS];
    int god_just_granted[GIFT_MAX_PLAYERS];
    size_t player_count;
    size_t active_player;
    int is_open;
} GiftShopState;

void gift_shop_init(GiftShopState *state, size_t player_count);
GiftCode gift_shop_begin(GiftShopState *state, size_t acting_player);

/* answer 仅接受 1、2、3 或 Quit；错误输入会放弃机会并关闭礼品屋。 */
GiftCode gift_shop_answer(GiftShopState *state,
                          int *player_money,
                          size_t money_count,
                          const char *answer);

int gift_shop_points(const GiftShopState *state, size_t player_index);
int gift_shop_god_rounds(const GiftShopState *state, size_t player_index);
int gift_shop_is_toll_free(const GiftShopState *state, size_t player_index);

/* 在该玩家的回合真正结束时调用；刚获得财神的回合不消耗次数。 */
void gift_shop_finish_turn(GiftShopState *state, size_t player_index);

#endif
