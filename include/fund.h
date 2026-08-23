#ifndef FUND_H
#define FUND_H

#include <stdint.h>

#include "game.h"

/*
 * 初始资金规则（功能点 A3）：
 *   - 游戏开始前发给本局所有玩家统一的初始资金；
 *   - 合法范围 1000~50000 元，默认 10000 元（直接回车采用默认值）；
 *   - 所有玩家的初始资金必须相同；
 *   - 确认后才写入玩家资产；取消或输入错误时不修改任何玩家资金。
 */
#define INITIAL_FUND_MIN     1000
#define INITIAL_FUND_MAX     50000
#define INITIAL_FUND_DEFAULT 10000

/* 校验初始资金数值是否合法（1000~50000） */
int initial_fund_valid(int64_t fund);

/*
 * 初始资金设置向导（在角色选择确认、游戏创建之后调用）：
 *   1. 输入初始资金：纯数字整数，允许首尾空格；直接回车采用默认值 10000；
 *      小数 / 文字 / 其他非法字符 / 超出范围 → 说明合法格式与范围，留在本步骤重新输入；
 *   2. 确认前显示“最终采用的初始资金”数值；
 *   3. 确认（Y）后调用 game_set_initial_fund() 把资金原子写入所有玩家；
 *      取消（N）→ 返回输入步骤重新设置，不修改任何玩家资金。
 *
 * 返回值：1 = 已确认并写入；0 = 输入结束（EOF），未写入任何资金。
 */
int fund_wizard(Game *game);

#endif /* FUND_H */
