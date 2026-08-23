#include "player.h"

#include <stdlib.h>

int player_selection_valid(int count, const int chosen[])
{
    int seen[CHARACTER_COUNT + 1] = {0}; /* seen[id] 标记角色 id 是否已被选 */
    int i;

    if (chosen == NULL) {
        return 0;
    }
    if (count < MIN_PLAYERS || count > MAX_PLAYERS) {
        return 0;
    }
    for (i = 0; i < count; ++i) {
        if (!character_id_valid(chosen[i])) {
            return 0; /* 角色编号必须是 1~4 */
        }
        if (seen[chosen[i]]) {
            return 0; /* 同一局内角色不得重复 */
        }
        seen[chosen[i]] = 1;
    }
    return 1;
}

Player *player_create_all(int count, const int chosen[])
{
    Player *players;
    int     i;

    /* 校验失败：不创建任何数据，避免半初始化状态 */
    if (!player_selection_valid(count, chosen)) {
        return NULL;
    }

    players = (Player *)malloc(sizeof(Player) * (size_t)count);
    if (players == NULL) {
        return NULL;
    }

    for (i = 0; i < count; ++i) {
        players[i].number    = i + 1;
        players[i].character = (CharacterId)chosen[i];
        players[i].fund      = 0;                    /* 初始资金确认前为 0，确认后统一写入 */
        players[i].status    = PLAYER_STATUS_NORMAL;
    }
    return players;
}

void player_destroy(Player *players)
{
    free(players);
}
