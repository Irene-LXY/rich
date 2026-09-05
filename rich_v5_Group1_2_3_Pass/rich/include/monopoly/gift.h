#ifndef MONOPOLY_GIFT_H
#define MONOPOLY_GIFT_H

#include <stddef.h>

#define GIFT_MAX_PLAYERS 4U
#define GIFT_BONUS_MONEY 2000
#define GIFT_BONUS_POINTS 200

typedef enum GiftCode {
    GIFT_OK = 0,
    GIFT_INVALID_CHOICE,
    GIFT_QUIT,
    GIFT_FORTUNE_SELECTED, /* 由运行时调用 fortune_grant 发放免租效果。 */
    GIFT_ERR_INVALID_ARGUMENT = -1,
    GIFT_ERR_NOT_OPEN = -2,
    GIFT_ERR_ALREADY_OPEN = -3,
    GIFT_ERR_OVERFLOW = -4
} GiftCode;

/* A15 只保存礼品屋新增的数据；玩家资金仍由 GameRuntime 统一持有。 */
typedef struct GiftShopState {
    int points[GIFT_MAX_PLAYERS];
    size_t player_count;
    size_t active_player;
    int is_open;
} GiftShopState;

void gift_shop_init(GiftShopState *state, size_t player_count);
GiftCode gift_shop_begin(GiftShopState *state, size_t acting_player);

/* answer 接受 1、2、3 或 Quit；3 返回 GIFT_FORTUNE_SELECTED。
 * 错误输入会放弃机会并关闭礼品屋。 */
GiftCode gift_shop_answer(GiftShopState *state,
                          int *player_money,
                          size_t money_count,
                          const char *answer);

int gift_shop_points(const GiftShopState *state, size_t player_index);

#endif
