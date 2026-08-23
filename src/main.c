#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main(int argc, char *argv[]) {
    Game game;
    char input[256];
    char message[8192];

#ifdef _WIN32
    (void)SetConsoleOutputCP(CP_UTF8);
    (void)SetConsoleCP(CP_UTF8);
#endif

    game_init(&game);
    if (application_start(&game, argc, argv, message, sizeof(message)) != STARTUP_OK) {
        fputs(message, stderr);
        return 1;
    }

    fputs(message, stdout);
    puts("输入 Help 查看命令；输入 Quit 可强制结束整局游戏。");
    while (game_is_running(&game)) {
        fputs("> ", stdout);
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == 0) {
            puts("输入结束，游戏自动退出。");
            (void)game_end(&game, END_REASON_USER_QUIT);
            break;
        }
        (void)command_execute(&game, input, message, sizeof(message));
        fputs(message, stdout);
    }
    runtime_destroy(game.runtime);
    game.runtime = NULL;
    return 0;
}
