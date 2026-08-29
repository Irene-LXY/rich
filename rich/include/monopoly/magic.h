#ifndef MONOPOLY_MAGIC_H
#define MONOPOLY_MAGIC_H

#include <stddef.h>

typedef enum MagicTargetType {
    MAGIC_TARGET_NONE = 0,
    MAGIC_TARGET_PLAYER,
    MAGIC_TARGET_CELL,
    MAGIC_TARGET_CUSTOM
} MagicTargetType;

typedef struct MagicTarget {
    MagicTargetType type;
    int value;
} MagicTarget;

/* 具体魔法由其他 Story 注册；A16 只负责选择和调用。 */
typedef int (*MagicEffectHandler)(size_t caster_index,
                                  int effect_id,
                                  const MagicTarget *target,
                                  void *context);

typedef struct MagicEffect {
    int id;
    const char *name;
    MagicEffectHandler handler;
    void *context;
} MagicEffect;

typedef enum MagicCode {
    MAGIC_OK = 0,
    MAGIC_INVALID_CHOICE,
    MAGIC_EXIT,
    MAGIC_EFFECT_REJECTED,
    MAGIC_ERR_INVALID_ARGUMENT = -1,
    MAGIC_ERR_NOT_OPEN = -2,
    MAGIC_ERR_ALREADY_OPEN = -3
} MagicCode;

typedef struct MagicHouseState {
    size_t player_count;
    size_t active_player;
    int is_open;
    int exited_without_effect;
    int last_effect_id;
    MagicTarget last_target;
} MagicHouseState;

void magic_house_init(MagicHouseState *state, size_t player_count);
MagicCode magic_house_begin(MagicHouseState *state, size_t acting_player);

/* 无效编号或被回调拒绝时保持开启；F/f 退出魔法屋。 */
MagicCode magic_house_answer(MagicHouseState *state,
                             const char *answer,
                             const MagicEffect *effects,
                             size_t effect_count,
                             const MagicTarget *target);

#endif
