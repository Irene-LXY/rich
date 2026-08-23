#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/startup.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    Game game;
    char input[256];
    /* Query最多列出58处房产，需要比普通提示更大的输出缓冲区。 */
    char message[8192];

    game_init(&game);
    if (application_start(&game, argc, argv, message, sizeof(message)) != STARTUP_OK) {
        fputs(message, stderr);
        return 1;
    }

    fputs(message, stdout);
    puts("可输入 Query 查询资产，或输入 Step n 使用遥控骰子。");
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
    return 0;
}
