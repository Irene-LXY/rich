#include "monopoly/character.h"

#include <stddef.h>

/* ANSI 终端颜色码 */
#define ANSI_RED    "\033[31m"
#define ANSI_GREEN  "\033[32m"
#define ANSI_BLUE   "\033[34m"
#define ANSI_YELLOW "\033[33m"

/* 角色表：与接口规范 3.1 玩家表一致（Q/钱夫人/红，A/阿土伯/绿，S/孙小美/蓝，J/金贝贝/黄） */
static const Character g_characters[CHARACTER_COUNT] = {
    { CHAR_QIAN_FUREN,  "钱夫人", "红色", 'Q', ANSI_RED    },
    { CHAR_A_TUBO,      "阿土伯", "绿色", 'A', ANSI_GREEN  },
    { CHAR_SUN_XIAOMEI, "孙小美", "蓝色", 'S', ANSI_BLUE   },
    { CHAR_JIN_BEIBEI,  "金贝贝", "黄色", 'J', ANSI_YELLOW },
};

const Character *character_table(void)
{
    return g_characters;
}

const Character *character_by_id(int id)
{
    if (!character_id_valid(id)) {
        return NULL;
    }
    return &g_characters[id - CHARACTER_MIN_ID];
}

int character_id_valid(long id)
{
    return id >= CHARACTER_MIN_ID && id <= CHARACTER_MAX_ID;
}
