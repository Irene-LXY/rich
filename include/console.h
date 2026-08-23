#ifndef CONSOLE_H
#define CONSOLE_H

/*
 * 控制台初始化：
 * Windows 下将控制台输入/输出切换为 UTF-8 并启用 ANSI 颜色（虚拟终端处理）；
 * 其他平台为空操作。
 */
void console_setup(void);

#endif /* CONSOLE_H */
