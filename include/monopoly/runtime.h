#ifndef MONOPOLY_RUNTIME_H
#define MONOPOLY_RUNTIME_H

#include <stddef.h>
#include "monopoly/query.h"

/* 集成层运行时：把 A4 回合管理、A5 地图、A8 掷骰移动串成一个可玩的游戏。
 * 通过不透明结构体对外暴露，内部持有地图、玩家、骰子、回合管理器等状态。 */
typedef struct GameRuntime GameRuntime;

/* 创建运行时。player_count: 2~4；initial_money: 每位玩家初始资金。
 * 角色按 Q/A/S/J 顺序自动分配。失败返回 NULL。 */
GameRuntime *runtime_create(int player_count, int initial_money);

/* 释放运行时。 */
void runtime_destroy(GameRuntime *runtime);

/* 开始回合循环（内部调用 A4 的 begin）。返回 0 成功，非 0 失败。 */
int runtime_begin(GameRuntime *runtime, char *message, size_t message_size);

/* 当前玩家掷骰子并移动（A8 逻辑）。返回 0 成功，非 0 失败。 */
int runtime_roll(GameRuntime *runtime, char *message, size_t message_size);

/* 遥控骰子：直接使用指定正整数步数，不调用随机骰子。 */
int runtime_step(GameRuntime *runtime, int steps, char *message, size_t message_size);

/* 查询当前玩家状态（位置、资金、所属地块等）。返回 0 成功。 */
int runtime_query(GameRuntime *runtime, char *message, size_t message_size);

/* 供土地、道具、礼品、矿地和破产模块接入的资产更新接口。 */
int runtime_set_player_money(GameRuntime *runtime, int player_id, int money);
int runtime_add_player_points(GameRuntime *runtime, int player_id, int points);
int runtime_set_player_item_count(GameRuntime *runtime,
                                  int player_id,
                                  QueryItemType item_type,
                                  int count);
int runtime_set_player_fortune_turns(GameRuntime *runtime,
                                     int player_id,
                                     int turns);
int runtime_set_player_hospital_turns(GameRuntime *runtime,
                                      int player_id,
                                      int turns);
int runtime_set_player_prison_turns(GameRuntime *runtime,
                                    int player_id,
                                    int turns);
int runtime_set_player_bankrupt(GameRuntime *runtime,
                                int player_id,
                                int bankrupt);
int runtime_assign_property(GameRuntime *runtime,
                            int player_id,
                            int position,
                            int building_level);
int runtime_release_property(GameRuntime *runtime, int position);

/* 渲染地图到 message。返回 0 成功。 */
int runtime_render(GameRuntime *runtime, char *message, size_t message_size);

/* 输出帮助信息。返回 0 成功。 */
int runtime_help(GameRuntime *runtime, char *message, size_t message_size);

/* 当前玩家名称（用于提示），运行时为空返回空串。 */
const char *runtime_current_player_name(const GameRuntime *runtime);

/* 是否游戏已结束。 */
int runtime_is_finished(const GameRuntime *runtime);

#endif
