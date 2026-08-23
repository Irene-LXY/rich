#include "game_interfaces.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define INPUT_SIZE 128
#define MAP_OUTPUT_SIZE 4096

static void lower_string(char *text) {
    while (*text != '\0') {
        *text = (char)tolower((unsigned char)*text);
        ++text;
    }
}

static void print_help(void) {
    printf("当前地图演示命令：\n");
    printf("  roll       掷1～6点骰子并移动演示角色\n");
    printf("  step n     指定移动n步，用于测试环形地图\n");
    printf("  map        重新显示地图\n");
    printf("  where      显示当前位置和格子真实类型\n");
    printf("  help       显示帮助\n");
    printf("  quit       退出\n");
}

static void print_map(const GameMap *map, const PlayerToken *players, size_t count) {
    char output[MAP_OUTPUT_SIZE];
    if (render_map(map, players, count, 0, 1, output, sizeof(output))) {
        printf("%s", output);
    } else {
        printf("地图绘制失败：输出缓冲区不足。\n");
    }
}

static int allow_enter(PlayerToken *player,
                       const MapCell *cell,
                       const MoveContext *context,
                       void *user_data) {
    (void)player;
    (void)cell;
    (void)context;
    (void)user_data;
    return 1;
}

int main(void) {
    GameMap map;
    RandomDice random_dice;
    Dice dice;
    PlayerToken players[1] = {{1, "钱夫人", 'Q', COLOR_RED, 0, 1}};
    char line[INPUT_SIZE];

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    game_map_init(&map);
    random_dice_init(&random_dice, 0);
    dice = random_dice_as_interface(&random_dice);

    printf("大富翁基础地图（纯C语言，70格，顺时针编号）\n");
    print_map(&map, players, 1);
    print_help();

    for (;;) {
        char command[16] = {0};
        char extra[16] = {0};
        int steps = 0;
        MoveContext result;
        char description[128];

        printf("\n地图演示> ");
        if (fgets(line, sizeof(line), stdin) == NULL) break;
        if (sscanf(line, "%15s", command) != 1) continue;
        lower_string(command);

        if (strcmp(command, "quit") == 0) break;
        if (strcmp(command, "help") == 0) {
            print_help();
            continue;
        }
        if (strcmp(command, "map") == 0) {
            print_map(&map, players, 1);
            continue;
        }
        if (strcmp(command, "where") == 0) {
            game_map_cell_description(&map, players[0].position,
                                      description, sizeof(description));
            printf("%s位于 %d：%s\n", players[0].name,
                   players[0].position, description);
            continue;
        }

        if (strcmp(command, "roll") == 0) {
            if (sscanf(line, "%15s %15s", command, extra) != 1) {
                printf("用法：roll\n");
                continue;
            }
            steps = dice_roll(&dice);
            printf("骰子点数：%d\n", steps);
        } else if (strcmp(command, "step") == 0) {
            if (sscanf(line, "%15s %d %15s", command, &steps, extra) != 2) {
                printf("用法：step n（例如 step 5）\n");
                continue;
            }
        } else {
            printf("未知命令，请输入 help 查看帮助。\n");
            continue;
        }

        result = move_player(&map, &players[0], steps, allow_enter, NULL);
        game_map_cell_description(&map, players[0].position,
                                  description, sizeof(description));
        printf("实际移动 %d 步，当前位置：%d（%s）\n",
               result.completed_steps, players[0].position, description);
        print_map(&map, players, 1);
    }

    printf("地图演示已退出。\n");
    return 0;
}
