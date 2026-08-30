#ifndef MONOPOLY_RUNTIME_H
#define MONOPOLY_RUNTIME_H

#include <stddef.h>

#include "monopoly/magic.h"

/* 集成层运行时：把 A4 回合管理、A5 地图、A8 掷骰移动串成一个可玩的游戏。
 * 通过不透明结构体对外暴露，内部持有地图、玩家、骰子、回合管理器等状态。 */
typedef struct GameRuntime GameRuntime;

typedef enum RuntimeContext {
    RUNTIME_CONTEXT_TURN_START = 0,
    RUNTIME_CONTEXT_GIFT_HOUSE,
    RUNTIME_CONTEXT_MAGIC_HOUSE,
    RUNTIME_CONTEXT_BUY_CONFIRM,
    RUNTIME_CONTEXT_UPGRADE_CONFIRM,
    RUNTIME_CONTEXT_TOOL_SHOP
} RuntimeContext;

/* 创建运行时。player_count: 2~4；initial_money: 每位玩家初始资金；
 * chosen_roles: 长度 player_count 的角色编号数组（1~4，对应钱夫人/阿土伯/孙小美/金贝贝）。
 * 失败返回 NULL。 */
GameRuntime *runtime_create(int player_count, int initial_money,
                            const int *chosen_roles);

/* 释放运行时。 */
void runtime_destroy(GameRuntime *runtime);

/* 开始回合循环（内部调用 A4 的 begin）。返回 0 成功，非 0 失败。 */
int runtime_begin(GameRuntime *runtime, char *message, size_t message_size);

/* 当前玩家掷骰子并移动（A8 逻辑）。返回 0 成功，非 0 失败。 */
int runtime_roll(GameRuntime *runtime, char *message, size_t message_size);

/* 测试用遥控骰子，对应需求命令 Step n；steps 必须大于 0。 */
int runtime_step(GameRuntime *runtime, int steps, char *message, size_t message_size);

/* 处理礼品屋或魔法屋正在等待的输入。0=已接受，1=输入无效，负数=无场景。 */
int runtime_answer(GameRuntime *runtime,
                   const char *answer,
                   char *message,
                   size_t message_size);

RuntimeContext runtime_context(const GameRuntime *runtime);

/* 注册 A16 的具体魔法。name/context 的生命周期必须覆盖 GameRuntime。 */
int runtime_register_magic_effect(GameRuntime *runtime, const MagicEffect *effect);

/* 查询当前玩家状态（位置、资金、所属地块等）。返回 0 成功。 */
int runtime_query(GameRuntime *runtime, char *message, size_t message_size);

/* 出售指定位置的自有房产（掷骰前操作）。返回 0 成功。 */
int runtime_sell(GameRuntime *runtime, int position, char *message, size_t message_size);

/* 放置道具（1 路障 / 3 炸弹），distance 为相对距离 -10~10。返回 0 成功。 */
int runtime_place_tool(GameRuntime *runtime, int tool, int distance,
                       char *message, size_t message_size);

/* 使用机器娃娃，清扫前方 10 步内的路障/炸弹。返回 0 成功。 */
int runtime_use_robot(GameRuntime *runtime, char *message, size_t message_size);

/* 道具屋购买道具（1 路障 / 2 机器娃娃 / 3 炸弹）。返回 0 成功。 */
int runtime_buy_tool(GameRuntime *runtime, int tool, char *message, size_t message_size);

/* 渲染地图到 message。返回 0 成功。 */
int runtime_render(GameRuntime *runtime, char *message, size_t message_size);

/* 输出帮助信息。返回 0 成功。 */
int runtime_help(GameRuntime *runtime, char *message, size_t message_size);

/* 当前玩家名称（用于提示），运行时为空返回空串。 */
const char *runtime_current_player_name(const GameRuntime *runtime);

/* 是否游戏已结束。 */
int runtime_is_finished(const GameRuntime *runtime);

/* 玩家拒绝购买/升级后，命令层仍可处理本回合末尾的 Help/Quit，
 * 并拒绝卖房或使用道具。完成该过渡后，下一玩家正式获得输入权。 */
int runtime_post_roll_transition_pending(const GameRuntime *runtime);
int runtime_complete_post_roll_transition(GameRuntime *runtime,
                                          char *message,
                                          size_t message_size);

/* 只读状态接口，供查询层和自动测试使用；player_index 从 0 开始。 */
int runtime_player_position(const GameRuntime *runtime, size_t player_index);
int runtime_player_money(const GameRuntime *runtime, size_t player_index);
int runtime_player_points(const GameRuntime *runtime, size_t player_index);
int runtime_player_god_rounds(const GameRuntime *runtime, size_t player_index);

#endif
