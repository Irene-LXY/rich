#ifndef MONOPOLY_AUTOMATION_H
#define MONOPOLY_AUTOMATION_H

#include <stddef.h>

/* 自动化测试适配层：把 JSON 用例里的 preset 装载进运行时，并把最终状态
 * 快照出来，供黑盒测试入口序列化成 Actual JSON。 */

typedef struct GameRuntime GameRuntime;

#define AUTOMATION_MAX_PLAYERS 4
#define AUTOMATION_MAX_PROPERTIES 70
#define AUTOMATION_MAX_MAP_ITEMS 70

typedef enum AutomationPlayerStatus {
    AUTOMATION_STATUS_NORMAL = 0,
    AUTOMATION_STATUS_HOSPITAL,
    AUTOMATION_STATUS_JAIL,
    AUTOMATION_STATUS_BANKRUPT
} AutomationPlayerStatus;

typedef enum AutomationMapItemType {
    AUTOMATION_ITEM_BLOCK = 1,
    AUTOMATION_ITEM_BOMB = 2
} AutomationMapItemType;

typedef enum AutomationPhase {
    AUTOMATION_PHASE_COMMAND = 0,
    AUTOMATION_PHASE_PROMPT,
    AUTOMATION_PHASE_ENDED
} AutomationPhase;

typedef struct AutomationPlayer {
    char symbol;                 /* Q / A / S / J */
    int fund;
    int credit;
    int position;
    AutomationPlayerStatus status;
    int remaining_rounds;
    int block;
    int robot;
    int bomb;
    int god_of_wealth_rounds;
} AutomationPlayer;

typedef struct AutomationProperty {
    int position;
    int owner_index;             /* 0-based */
    int level;
} AutomationProperty;

typedef struct AutomationMapItem {
    int position;
    AutomationMapItemType type;
} AutomationMapItem;

typedef struct AutomationPreset {
    AutomationPlayer players[AUTOMATION_MAX_PLAYERS];
    int player_count;
    int current_user_index;      /* 0-based；-1 表示未指定 */
    AutomationProperty properties[AUTOMATION_MAX_PROPERTIES];
    int property_count;
    AutomationMapItem map_items[AUTOMATION_MAX_MAP_ITEMS];
    int map_item_count;
} AutomationPreset;

typedef struct AutomationSnapshot {
    AutomationPlayer players[AUTOMATION_MAX_PLAYERS];
    int player_count;
    int current_user_index;      /* -1 表示无 */
    AutomationPhase phase;
    const char *pending_prompt;  /* COMMAND 阶段为 NULL */
    int game_status;             /* 0 RUNNING, 1 FINISHED */
    int winner_index;            /* -1 表示无 */
    AutomationProperty properties[AUTOMATION_MAX_PROPERTIES];
    int property_count;
    AutomationMapItem map_items[AUTOMATION_MAX_MAP_ITEMS];
    int map_item_count;
} AutomationSnapshot;

/* 依据 preset 创建运行时。返回 NULL 表示失败。 */
GameRuntime *runtime_load_preset(const AutomationPreset *preset);

/* 把运行时当前完整状态写入快照。 */
void runtime_snapshot(const GameRuntime *runtime, AutomationSnapshot *snapshot);

/* 强制结束整局游戏（对应 QUIT）。返回 0 成功。 */
int runtime_finish(GameRuntime *runtime);

#endif /* MONOPOLY_AUTOMATION_H */

