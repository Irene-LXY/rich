#include "console.h"

#ifdef _WIN32
#include <windows.h>
#endif

void console_setup(void)
{
#ifdef _WIN32
    HANDLE hOut;
    DWORD  mode = 0;

    /* 源文件为 UTF-8：切换控制台代码页，保证中文正常显示 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* 启用 ANSI 转义序列（角色颜色显示） */
    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}
