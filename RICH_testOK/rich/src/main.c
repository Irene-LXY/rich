#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "monopoly/startup.h"

#include "a4/a4_turn_manager.h"
#include "map/game_interfaces.h"
#include "map/map.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

#define MESSAGE_BUFFER_SIZE 2048
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

/* ------------------------------------------------------------------------- */
/* 极小 JSON 解析器（只用于 --run-test 模式）                                 */
/* ------------------------------------------------------------------------- */
typedef enum {
    J_NULL,
    J_BOOL,
    J_NUMBER,
    J_STRING,
    J_ARRAY,
    J_OBJECT
} JType;

typedef struct JVal {
    JType type;
    int boolean;
    double number;
    char *string;
    struct JVal **items;
    int item_count;
    char **keys;
    struct JVal **values;
    int member_count;
} JVal;

static void json_skip_ws(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r') {
        (*p)++;
    }
}

static JVal *json_new(JType type) {
    JVal *v = (JVal *)calloc(1, sizeof(JVal));
    if (v != NULL) {
        v->type = type;
    }
    return v;
}

static void json_free(JVal *v) {
    int i;
    if (v == NULL) return;
    if (v->string != NULL) free(v->string);
    for (i = 0; i < v->item_count; ++i) json_free(v->items[i]);
    for (i = 0; i < v->member_count; ++i) {
        free(v->keys[i]);
        json_free(v->values[i]);
    }
    free(v->items);
    free(v->keys);
    free(v->values);
    free(v);
}

static JVal *json_parse_value(const char **p);

static JVal *json_parse_string(const char **p) {
    JVal *v;
    size_t cap = 32;
    size_t len = 0;
    char *buf;
    if (**p != '"') return NULL;
    (*p)++;
    buf = (char *)malloc(cap);
    if (buf == NULL) return NULL;
    while (**p != '\0' && **p != '"') {
        char c;
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                default: c = **p; break;
            }
        } else {
            c = **p;
        }
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
            if (buf == NULL) return NULL;
        }
        buf[len++] = c;
        (*p)++;
    }
    if (**p != '"') {
        free(buf);
        return NULL;
    }
    (*p)++;
    buf[len] = '\0';
    v = json_new(J_STRING);
    if (v == NULL) {
        free(buf);
        return NULL;
    }
    v->string = buf;
    return v;
}

static JVal *json_parse_number(const char **p) {
    JVal *v = json_new(J_NUMBER);
    if (v == NULL) return NULL;
    v->number = strtod(*p, (char **)p);
    return v;
}

static JVal *json_parse_array(const char **p) {
    JVal *v = json_new(J_ARRAY);
    if (v == NULL) return NULL;
    (*p)++;
    json_skip_ws(p);
    if (**p == ']') {
        (*p)++;
        return v;
    }
    for (;;) {
        JVal *item = json_parse_value(p);
        if (item == NULL) {
            json_free(v);
            return NULL;
        }
        v->items = (JVal **)realloc(v->items, sizeof(JVal *) * (v->item_count + 1));
        v->items[v->item_count++] = item;
        json_skip_ws(p);
        if (**p == ',') {
            (*p)++;
            json_skip_ws(p);
            continue;
        }
        if (**p == ']') {
            (*p)++;
            return v;
        }
        json_free(v);
        return NULL;
    }
}

static JVal *json_parse_object(const char **p) {
    JVal *v = json_new(J_OBJECT);
    if (v == NULL) return NULL;
    (*p)++;
    json_skip_ws(p);
    if (**p == '}') {
        (*p)++;
        return v;
    }
    for (;;) {
        JVal *key;
        JVal *value;
        if (**p != '"') {
            json_free(v);
            return NULL;
        }
        key = json_parse_string(p);
        if (key == NULL) {
            json_free(v);
            return NULL;
        }
        json_skip_ws(p);
        if (**p != ':') {
            json_free(key);
            json_free(v);
            return NULL;
        }
        (*p)++;
        json_skip_ws(p);
        value = json_parse_value(p);
        if (value == NULL) {
            json_free(key);
            json_free(v);
            return NULL;
        }
        v->keys = (char **)realloc(v->keys, sizeof(char *) * (v->member_count + 1));
        v->values = (JVal **)realloc(v->values, sizeof(JVal *) * (v->member_count + 1));
        v->keys[v->member_count] = key->string;
        key->string = NULL;
        json_free(key);
        v->values[v->member_count] = value;
        v->member_count++;
        json_skip_ws(p);
        if (**p == ',') {
            (*p)++;
            json_skip_ws(p);
            continue;
        }
        if (**p == '}') {
            (*p)++;
            return v;
        }
        json_free(v);
        return NULL;
    }
}

static JVal *json_parse_value(const char **p) {
    json_skip_ws(p);
    if (**p == '{') return json_parse_object(p);
    if (**p == '[') return json_parse_array(p);
    if (**p == '"') return json_parse_string(p);
    if (**p == '-' || (**p >= '0' && **p <= '9')) return json_parse_number(p);
    if (strncmp(*p, "true", 4) == 0) {
        JVal *v = json_new(J_BOOL);
        v->boolean = 1;
        *p += 4;
        return v;
    }
    if (strncmp(*p, "false", 5) == 0) {
        JVal *v = json_new(J_BOOL);
        v->boolean = 0;
        *p += 5;
        return v;
    }
    if (strncmp(*p, "null", 4) == 0) {
        *p += 4;
        return json_new(J_NULL);
    }
    return NULL;
}

static JVal *json_get(JVal *obj, const char *key) {
    int i;
    if (obj == NULL || obj->type != J_OBJECT) return NULL;
    for (i = 0; i < obj->member_count; ++i) {
        if (strcmp(obj->keys[i], key) == 0) return obj->values[i];
    }
    return NULL;
}

static JVal *json_at(JVal *arr, int index) {
    if (arr == NULL || arr->type != J_ARRAY || index < 0 || index >= arr->item_count) return NULL;
    return arr->items[index];
}

static const char *json_string(JVal *v) {
    return (v != NULL && v->type == J_STRING) ? v->string : NULL;
}

static int json_int(JVal *v, int fallback) {
    return (v != NULL && v->type == J_NUMBER) ? (int)v->number : fallback;
}

/* ------------------------------------------------------------------------- */
/* --run-test 模式                                                           */
/* ------------------------------------------------------------------------- */
typedef struct {
    GameMap map;
    PlayerToken players[A4_MAX_PLAYERS];
    int money[A4_MAX_PLAYERS];
    int points[A4_MAX_PLAYERS];
    A4TurnManager turn_manager;
    A4TurnHooks hooks;
    int player_count;
    int finished;
} TestGame;

static int symbol_to_id(char sym) {
    switch (toupper((unsigned char)sym)) {
        case 'Q': return 1;
        case 'A': return 2;
        case 'S': return 3;
        case 'J': return 4;
        default: return 0;
    }
}

static char id_to_symbol(int id) {
    switch (id) {
        case 1: return 'Q';
        case 2: return 'A';
        case 3: return 'S';
        case 4: return 'J';
        default: return '?';
    }
}

static int find_player_index(TestGame *game, const char *sym) {
    int i;
    if (sym == NULL || sym[0] == '\0') return -1;
    for (i = 0; i < game->player_count; ++i) {
        if (toupper((unsigned char)game->players[i].symbol) == toupper((unsigned char)sym[0])) {
            return i;
        }
    }
    return -1;
}

static A4MoveResult test_roll_and_move(
    void *context,
    const A4TurnSnapshot *snapshot,
    int forced_steps,
    int *actual_steps)
{
    TestGame *game = (TestGame *)context;
    int player_id = (int)snapshot->current_player_id;
    int idx = -1;
    int j;
    int steps = forced_steps > 0 ? forced_steps : 1;
    MoveContext move_ctx;

    for (j = 0; j < game->player_count; ++j) {
        if (game->players[j].id == player_id) {
            idx = j;
            break;
        }
    }
    if (idx < 0) {
        if (actual_steps != NULL) *actual_steps = 0;
        return A4_MOVE_FAILED;
    }
    move_ctx = move_player(&game->map, &game->players[idx], steps, NULL, NULL);
    if (actual_steps != NULL) *actual_steps = move_ctx.completed_steps;
    return A4_MOVE_RESOLVED;
}

static int load_preset(TestGame *game, JVal *root) {
    JVal *preset = json_get(root, "preset");
    JVal *users = json_get(preset, "users");
    JVal *players_json = json_get(preset, "players");
    A4PlayerConfig configs[A4_MAX_PLAYERS];
    char role_names[A4_MAX_PLAYERS][2];
    int i;

    memset(game, 0, sizeof(*game));
    game_map_init(&game->map);
    if (users == NULL || users->type != J_ARRAY) return 1;
    game->player_count = users->item_count;
    if (game->player_count < (int)A4_MIN_PLAYERS || game->player_count > (int)A4_MAX_PLAYERS) return 1;

    for (i = 0; i < game->player_count; ++i) {
        const char *sym = json_string(json_at(users, i));
        int id = symbol_to_id(sym == NULL ? '?' : sym[0]);
        if (id == 0) return 1;
        game->players[i].id = id;
        game->players[i].name = "player";
        game->players[i].symbol = id_to_symbol(id);
        game->players[i].color = COLOR_DEFAULT;
        game->players[i].position = 0;
        game->players[i].active = 1;
        game->money[i] = 10000;
        game->points[i] = 0;
    }

    if (players_json != NULL && players_json->type == J_ARRAY) {
        for (i = 0; i < players_json->item_count; ++i) {
            JVal *pj = json_at(players_json, i);
            const char *sym = json_string(json_get(pj, "id"));
            int idx = find_player_index(game, sym);
            JVal *fund = json_get(pj, "fund");
            JVal *credit = json_get(pj, "credit");
            JVal *position = json_get(pj, "position");
            JVal *status = json_get(pj, "status");
            if (idx < 0) continue;
            if (fund != NULL && fund->type == J_NUMBER) game->money[idx] = (int)fund->number;
            if (credit != NULL && credit->type == J_NUMBER) game->points[idx] = (int)credit->number;
            if (position != NULL && position->type == J_NUMBER) game->players[idx].position = (int)position->number;
            if (status != NULL && status->type == J_STRING && strcmp(status->string, "BANKRUPT") == 0) {
                game->players[idx].active = 0;
            }
        }
    }

    for (i = 0; i < game->player_count; ++i) {
        configs[i].id = (A4PlayerId)game->players[i].id;
        role_names[i][0] = game->players[i].symbol;
        role_names[i][1] = '\0';
        configs[i].role_name = role_names[i];
    }

    game->hooks.context = game;
    game->hooks.roll_and_move = test_roll_and_move;
    if (a4_turn_manager_init(&game->turn_manager, configs, (size_t)game->player_count, &game->hooks) != A4_TURN_OK) return 1;
    if (a4_turn_manager_begin(&game->turn_manager) != A4_TURN_OK) return 1;

    {
        JVal *current = json_get(preset, "current_user");
        const char *cur = json_string(current);
        int cur_idx = find_player_index(game, cur);
        if (cur_idx >= 0) game->turn_manager.current_player_index = (size_t)cur_idx;
    }
    return 0;
}

static void execute_actions(TestGame *game, JVal *root) {
    JVal *preset = json_get(root, "preset");
    JVal *actions = json_get(root, "actions");
    JVal *dice_json = json_get(preset, "dice_sequence");
    int dice[128];
    int dice_count = 0;
    int dice_index = 0;
    int i;

    if (dice_json != NULL && dice_json->type == J_ARRAY) {
        dice_count = dice_json->item_count;
        for (i = 0; i < dice_count && i < (int)(sizeof(dice)/sizeof(dice[0])); ++i) {
            JVal *d = json_at(dice_json, i);
            dice[i] = (d != NULL && d->type == J_NUMBER) ? (int)d->number : 1;
        }
    }
    if (actions == NULL || actions->type != J_ARRAY) return;

    for (i = 0; i < actions->item_count; ++i) {
        JVal *action = json_at(actions, i);
        const char *cmd = json_string(json_get(action, "command"));
        char upper[64];
        size_t k;
        A4TurnSnapshot snap;
        if (cmd == NULL) continue;
        strncpy(upper, cmd, sizeof(upper) - 1);
        upper[sizeof(upper) - 1] = '\0';
        for (k = 0; upper[k] != '\0'; ++k) upper[k] = (char)toupper((unsigned char)upper[k]);

        if (strcmp(upper, "QUIT") == 0) {
            game->finished = 1;
            (void)a4_turn_manager_finish(&game->turn_manager, 0U);
            return;
        }
        snap = a4_turn_manager_snapshot(&game->turn_manager);
        if (strcmp(upper, "ROLL") == 0) {
            int steps = dice_index < dice_count ? dice[dice_index++] : 1;
            (void)a4_turn_manager_roll(&game->turn_manager, snap.current_player_id, steps);
        } else if (strcmp(upper, "STEP") == 0) {
            JVal *steps_json = json_get(json_get(action, "params"), "steps");
            int steps = (steps_json != NULL && steps_json->type == J_NUMBER) ? (int)steps_json->number : 1;
            (void)a4_turn_manager_roll(&game->turn_manager, snap.current_player_id, steps);
        }
    }
}

static void print_actual_json(TestGame *game, JVal *root) {
    A4TurnSnapshot snap = a4_turn_manager_snapshot(&game->turn_manager);
    const char *case_id = json_string(json_get(root, "case_id"));
    int i, j;

    printf("{\n");
    printf("  \"schema_version\": \"1.0\",\n");
    printf("  \"case_id\": \"%s\",\n", case_id != NULL ? case_id : "");
    printf("  \"users\": [");
    for (i = 0; i < game->player_count; ++i) {
        printf("%s\"%c\"", i == 0 ? "" : ", ", game->players[i].symbol);
    }
    printf("],\n");

    {
        int idx = 0;
        for (j = 0; j < game->player_count; ++j) {
            if (game->players[j].id == (int)snap.current_player_id) {
                idx = j;
                break;
            }
        }
        printf("  \"current_user\": \"%c\",\n", game->players[idx].symbol);
    }
    printf("  \"phase\": \"%s\",\n", game->finished ? "ENDED" : "COMMAND");
    printf("  \"pending_prompt\": null,\n");
    printf("  \"game_status\": \"%s\",\n", game->finished ? "FINISHED" : "RUNNING");
    printf("  \"winner\": null,\n");

    printf("  \"players\": [\n");
    for (i = 0; i < game->player_count; ++i) {
        printf("    {\n");
        printf("      \"id\": \"%c\",\n", game->players[i].symbol);
        printf("      \"fund\": %d,\n", game->money[i]);
        printf("      \"credit\": %d,\n", game->points[i]);
        printf("      \"position\": %d,\n", game->players[i].position);
        printf("      \"status\": \"%s\",\n", game->players[i].active ? "NORMAL" : "BANKRUPT");
        printf("      \"remaining_rounds\": 0,\n");
        printf("      \"items\": {\"BLOCK\": 0, \"ROBOT\": 0, \"BOMB\": 0},\n");
        printf("      \"god_of_wealth_rounds\": 0\n");
        printf("    }%s\n", i + 1 == game->player_count ? "" : ",");
    }
    printf("  ],\n");

    printf("  \"properties\": [],\n");
    printf("  \"map_items\": [],\n");
    printf("  \"display_players\": [");
    {
        int first = 1;
        for (i = 0; i < RICH_MAP_SIZE; ++i) {
            int visible = -1;
            for (j = 0; j < game->player_count; ++j) {
                if (game->players[j].active && game->players[j].position == i) {
                    if (j == (int)game->turn_manager.current_player_index) {
                        visible = j;
                        break;
                    }
                    if (visible < 0) visible = j;
                }
            }
            if (visible >= 0) {
                printf("%s{\"position\": %d, \"visible_user\": \"%c\"}",
                       first ? "" : ", ", i, game->players[visible].symbol);
                first = 0;
            }
        }
    }
    printf("]\n");
    printf("}\n");
}

static int run_json_test(const char *path) {
    FILE *fp = fopen(path, "rb");
    long size;
    char *text;
    const char *p;
    JVal *root;
    TestGame game;

    if (fp == NULL) return 1;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);
    text = (char *)malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(fp);
        return 1;
    }
    if (size > 0) (void)fread(text, 1, (size_t)size, fp);
    text[size] = '\0';
    fclose(fp);

    p = text;
    root = json_parse_value(&p);
    free(text);
    if (root == NULL) return 1;
    if (load_preset(&game, root) != 0) {
        json_free(root);
        return 1;
    }
    execute_actions(&game, root);
    print_actual_json(&game, root);
    json_free(root);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *json_test_path = NULL;
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--run-test") == 0 && i + 1 < argc) {
            json_test_path = argv[i + 1];
            i++;
        }
    }
    if (json_test_path != NULL) {
        return run_json_test(json_test_path);
    }

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

    /* 退出循环后重绘最后一帧，展示结束原因（如 Quit 确认）。 */
    render_frame(&game, map_buffer, sizeof(map_buffer), message);
    return 0;
}
