#ifndef CHARACTER_H
#define CHARACTER_H

/*
 * 角色模块：大富翁开局可选角色定义
 *
 * 编号  角色名   颜色  地图显示符号（拼音首字母）
 *  1    钱夫人   红色        Q
 *  2    阿土伯   绿色        A
 *  3    孙小美   蓝色        S
 *  4    金贝贝   黄色        J
 */

typedef enum {
    CHAR_QIAN_FUREN  = 1,   /* 钱夫人 */
    CHAR_A_TUBO      = 2,   /* 阿土伯 */
    CHAR_SUN_XIAOMEI = 3,   /* 孙小美 */
    CHAR_JIN_BEIBEI  = 4    /* 金贝贝 */
} CharacterId;

#define CHARACTER_MIN_ID 1
#define CHARACTER_MAX_ID 4
#define CHARACTER_COUNT  4

/* 角色静态描述信息（只读） */
typedef struct {
    CharacterId id;         /* 角色编号（1~4） */
    const char *name;       /* 中文名 */
    const char *color_name; /* 颜色中文名 */
    char        symbol;     /* 地图显示符号：Q / A / S / J */
    const char *ansi_color; /* 终端 ANSI 颜色码 */
} Character;

/* 获取全部角色表（固定 CHARACTER_COUNT 项，按编号 1~4 排列） */
const Character *character_table(void);

/* 按编号查询角色；非法编号返回 NULL */
const Character *character_by_id(int id);

/* 判断编号是否合法（1~4）：合法返回 1，否则返回 0 */
int character_id_valid(long id);

#endif /* CHARACTER_H */
