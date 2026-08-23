#ifndef CLI_H
#define CLI_H

#include "game.h"

/*
 * 指令输入循环（A6 帮助功能的宿主环境，演示“随时打开帮助界面”）：
 *
 *   - HELP（忽略大小写）：打开帮助界面，查看完整指令集 —— 本版本完整实现；
 *   - QUIT：强制结束（演示性退出；完整 Quit 功能由对应模块实现）；
 *   - ROLL / STEP n / SELL n / BLOCK n / BOMB n / ROBOT / QUERY：
 *     已识别，功能由对应模块实现，本版本仅提示；
 *   - 其他输入：提示未知命令并建议使用 HELP；
 *   - EOF（Ctrl+Z 回车 / 管道结束）：退出指令输入。
 *
 * 说明：HELP 忽略大小写是本功能的明确要求；其余命令按接口规范统一使用大写。
 */
void cli_command_loop(Game *game);

#endif /* CLI_H */
