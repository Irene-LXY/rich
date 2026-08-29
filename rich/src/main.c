#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define MESSAGE_BUFFER_SIZE 8192
#define MAP_BUFFER_SIZE 4096

/* Windows 控制台：切到 UTF-8 代码页，并启用 ANSI 转义（清屏、颜色）。 */
static void setup_console(void) {
#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    if (handle != INVALID_HANDLE_VALUE && GetConsoleMode(handle, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        (void)SetConsoleMode(handle, mode);
    }
#endif
}

static void clear_screen(void) {
    fputs("\033[2J\033[H", stdout);
}

/* 清屏后重绘一帧：地图（若已开局）+ 提示消息。 */
static void render_frame(const Game *game, char *map_buffer, size_t map_size,
                         const char *message) {
    clear_screen();
    if (game->runtime != NULL &&
        runtime_render(game->runtime, map_buffer, map_size) == 0) {
        fputs(map_buffer, stdout);
    }
    fputs(message, stdout);
}

int main(int argc, char *argv[]) {
    Game game;
    char input[256];
    char message[MESSAGE_BUFFER_SIZE];
    char map_buffer[MAP_BUFFER_SIZE];

    setup_console();
    game_init(&game);
    if (application_start(&game, argc, argv, message, sizeof(message)) != STARTUP_OK) {
        fputs(message, stderr);
        return 1;
    }

    while (game_is_running(&game)) {
        render_frame(&game, map_buffer, sizeof(map_buffer), message);
        fputs("> ", stdout);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == 0) {
            fputs("\n", stdout);
            (void)game_end(&game, END_REASON_USER_QUIT);
            (void)snprintf(message, sizeof(message), "输入结束，游戏自动退出。\n");
            break;
        }
        (void)command_execute(&game, input, message, sizeof(message));
    }

    /* 退出循环后重绘最后一帧，展示结束原因。 */
    render_frame(&game, map_buffer, sizeof(map_buffer), message);
    runtime_destroy(game.runtime);
    game.runtime = NULL;
    return 0;
}
