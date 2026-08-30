#include "property/property_system.h"

#include "map/game_interfaces.h"
#include "monopoly/command.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    GameMap map;
    int money[4];
    PropertySystem property;
} Fixture;

static int passed_count;
static int failed_count;
static int integration_assertions;
static int integration_failures;

#define INTEGRATION_CHECK(condition) do { \
    ++integration_assertions; \
    if (!(condition)) { \
        ++integration_failures; \
        (void)fprintf(stderr, "[FAIL] property runtime integration line %d\n", \
                      __LINE__); \
    } \
} while (0)

static void record_case(const char *case_id, int passed)
{
    if (passed) {
        ++passed_count;
        (void)printf("[PASS] %s\n", case_id);
    } else {
        ++failed_count;
        (void)fprintf(stderr, "[FAIL] %s\n", case_id);
    }
}

static void fixture_init(Fixture *fixture, int player_money, int owner_money)
{
    game_map_init(&fixture->map);
    fixture->money[0] = player_money;
    fixture->money[1] = owner_money;
    fixture->money[2] = 0;
    fixture->money[3] = 0;
    (void)property_system_init(&fixture->property, &fixture->map,
                               fixture->money, 4U);
}

static MapCell *set_land(Fixture *fixture, int position,
                         int owner_id, int level)
{
    MapCell *cell = game_map_cell_at_mut(&fixture->map, position);
    if (cell != NULL) {
        cell->owner_id = owner_id;
        cell->building_level = level;
    }
    return cell;
}

static int run_purchase_case(int position, int initial_money,
                             int expected_money, int check_color)
{
    Fixture fixture;
    PropertyResult landing;
    PropertyResult resolved;
    MapCell *cell;
    int ok;
    fixture_init(&fixture, initial_money, 0);
    cell = game_map_cell_at_mut(&fixture.map, position);
    ok = property_after_move(&fixture.property, 0, position,
                             PROPERTY_TOLL_EXEMPT_NONE, &landing) ==
             PROPERTY_PENDING &&
         landing.action == PROPERTY_ACTION_BUY &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_OK &&
         cell != NULL && cell->owner_id == 1 && cell->building_level == 0 &&
         fixture.money[0] == expected_money &&
         game_map_base_symbol(&fixture.map, position) == '0';
    if (check_color) {
        PlayerToken players[2] = {
            {1, "P1", 'Q', COLOR_RED, 0, 1},
            {2, "P2", 'A', COLOR_GREEN, 0, 1}
        };
        char rendered[4096];
        ok = ok && render_map(&fixture.map, players, 2U, 1, 0,
                              rendered, sizeof(rendered)) &&
             strstr(rendered, "\033[31m0\033[0m") != NULL;
    }
    return ok;
}

static void test_a9(void)
{
    Fixture fixture;
    PropertyResult landing;
    PropertyResult resolved;
    MapCell *cell;
    int parsed;
    int ok;

    record_case("Case_A9_001", run_purchase_case(1, 500, 300, 1));
    record_case("Case_A9_002", run_purchase_case(29, 500, 0, 0));
    record_case("Case_A9_003", run_purchase_case(36, 300, 0, 0));

    fixture_init(&fixture, 100, 0);
    cell = game_map_cell_at_mut(&fixture.map, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_ERR_INSUFFICIENT_FUNDS &&
         cell != NULL && cell->owner_id == RICH_NO_OWNER &&
         fixture.money[0] == 100;
    record_case("Case_A9_004", ok);

    fixture_init(&fixture, 0, 0);
    cell = game_map_cell_at_mut(&fixture.map, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_ERR_INSUFFICIENT_FUNDS &&
         cell != NULL && cell->owner_id == RICH_NO_OWNER &&
         fixture.money[0] == 0;
    record_case("Case_A9_005", ok);

    fixture_init(&fixture, -100, 0);
    ok = property_parse_money("-100", &parsed) ==
             PROPERTY_ERR_INVALID_ARGUMENT &&
         property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_ERR_INVALID_ARGUMENT;
    record_case("Case_A9_006", ok);

    record_case("Case_A9_007",
        property_parse_money("abc", &parsed) == PROPERTY_ERR_INVALID_ARGUMENT);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_OK &&
         cell != NULL && cell->building_level == 2 &&
         fixture.money[0] == 300 &&
         game_map_base_symbol(&fixture.map, 1) == '2';
    record_case("Case_A9_008", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 2);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_OK &&
         cell != NULL && cell->building_level == 3 &&
         fixture.money[0] == 300 &&
         game_map_base_symbol(&fixture.map, 1) == '3';
    record_case("Case_A9_009", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 3);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_ERR_MAX_LEVEL &&
         !property_has_pending(&fixture.property) &&
         cell != NULL && cell->building_level == 3 && fixture.money[0] == 500;
    record_case("Case_A9_010", ok);

    fixture_init(&fixture, 100, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "Y", &resolved) ==
             PROPERTY_ERR_INSUFFICIENT_FUNDS &&
         cell != NULL && cell->building_level == 1 && fixture.money[0] == 100;
    record_case("Case_A9_011", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "N", &resolved) ==
             PROPERTY_OK && !resolved.accepted &&
         cell != NULL && cell->building_level == 1 && fixture.money[0] == 500;
    record_case("Case_A9_012", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "A", &resolved) ==
             PROPERTY_ERR_INVALID_DECISION &&
         property_has_pending(&fixture.property) &&
         cell != NULL && cell->building_level == 1 && fixture.money[0] == 500;
    (void)property_resolve_answer(&fixture.property, 0, "N", &resolved);
    record_case("Case_A9_013", ok);

    fixture_init(&fixture, 500, 500);
    cell = set_land(&fixture, 1, 2, 0);
    ok = property_after_move(&fixture.property, 0, 1,
             PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, &landing) == PROPERTY_OK &&
         landing.action == PROPERTY_ACTION_TOLL &&
         !property_has_pending(&fixture.property) &&
         cell != NULL && cell->owner_id == 2 && fixture.money[0] == 500;
    record_case("Case_A9_014", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "y", &resolved) ==
             PROPERTY_OK && resolved.accepted &&
         cell != NULL && cell->building_level == 2;
    record_case("Case_A9_015", ok);

    fixture_init(&fixture, 500, 0);
    cell = set_land(&fixture, 1, 1, 1);
    ok = property_after_move(&fixture.property, 0, 1, 0U, &landing) ==
             PROPERTY_PENDING &&
         property_resolve_answer(&fixture.property, 0, "n", &resolved) ==
             PROPERTY_OK && !resolved.accepted &&
         cell != NULL && cell->building_level == 1 && fixture.money[0] == 500;
    record_case("Case_A9_016", ok);
}

static int toll_case(int position, int level, int player_money,
                     unsigned int exemptions, int expected_toll,
                     int expected_player_money, int expected_owner_money,
                     int expected_paid, int expected_bankrupt)
{
    Fixture fixture;
    PropertyResult result;
    fixture_init(&fixture, player_money, 1000);
    (void)set_land(&fixture, position, 2, level);
    return property_after_move(&fixture.property, 0, position,
                               exemptions, &result) == PROPERTY_OK &&
           result.action == PROPERTY_ACTION_TOLL &&
           result.toll == expected_toll &&
           result.amount_paid == expected_paid &&
           result.player_bankrupt == expected_bankrupt &&
           fixture.money[0] == expected_player_money &&
           fixture.money[1] == expected_owner_money;
}

static void test_a10(void)
{
    Fixture fixture;
    PropertyResult result;
    MapCell *cell;
    int before_player;
    int before_owner;
    int ok;

    record_case("Case_A10_001", toll_case(5, 0, 1000, 0U, 100, 900, 1100, 100, 0));
    record_case("Case_A10_002", toll_case(8, 1, 1000, 0U, 200, 800, 1200, 200, 0));
    record_case("Case_A10_003", toll_case(10, 2, 1000, 0U, 300, 700, 1300, 300, 0));
    record_case("Case_A10_004", toll_case(12, 3, 1000, 0U, 400, 600, 1400, 400, 0));
    record_case("Case_A10_005", toll_case(29, 0, 1000, 0U, 250, 750, 1250, 250, 0));
    record_case("Case_A10_006", toll_case(30, 1, 1000, 0U, 500, 500, 1500, 500, 0));
    record_case("Case_A10_007", toll_case(31, 2, 1000, 0U, 750, 250, 1750, 750, 0));
    record_case("Case_A10_008", toll_case(32, 3, 1000, 0U, 1000, 0, 2000, 1000, 0));
    record_case("Case_A10_009", toll_case(36, 0, 1000, 0U, 150, 850, 1150, 150, 0));
    record_case("Case_A10_010", toll_case(40, 1, 1000, 0U, 300, 700, 1300, 300, 0));
    record_case("Case_A10_011", toll_case(45, 2, 1000, 0U, 450, 550, 1450, 450, 0));
    record_case("Case_A10_012", toll_case(50, 3, 1000, 0U, 600, 400, 1600, 600, 0));

    record_case("Case_A10_013", toll_case(36, 2, 1000,
        PROPERTY_TOLL_EXEMPT_FORTUNE, 0, 1000, 1000, 0, 0));
    record_case("Case_A10_014", toll_case(5, 3, 1000,
        PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 0, 1000, 1000, 0, 0));
    record_case("Case_A10_015", toll_case(29, 2, 1000,
        PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 0, 1000, 1000, 0, 0));
    record_case("Case_A10_016", toll_case(36, 3, 1000,
        PROPERTY_TOLL_EXEMPT_FORTUNE |
        PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED,
        0, 1000, 1000, 0, 0));
    record_case("Case_A10_017", toll_case(5, 1, 1000,
        PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 0, 1000, 1000, 0, 0));
    record_case("Case_A10_018", toll_case(29, 1, 1000,
        PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 0, 1000, 1000, 0, 0));
    record_case("Case_A10_019", toll_case(8, 1, 200, 0U,
        200, 0, 1200, 200, 0));

    fixture_init(&fixture, 500, 1000);
    (void)set_land(&fixture, 1, 1, 2);
    (void)set_land(&fixture, 29, 2, 2);
    ok = property_after_move(&fixture.property, 0, 29, 0U, &result) ==
             PROPERTY_OK && result.toll == 750 && result.amount_paid == 500 &&
         result.player_bankrupt && result.released_property_count == 1 &&
         fixture.money[0] == -250 && fixture.money[1] == 1500 &&
         fixture.map.cells[1].owner_id == RICH_NO_OWNER &&
         fixture.map.cells[1].building_level == 0;
    record_case("Case_A10_020", ok);

    record_case("Case_A10_021", toll_case(5, 0, 0, 0U,
        100, -100, 1000, 0, 1));
    record_case("Case_A10_022", toll_case(42, 1, 350, 0U,
        300, 50, 1300, 300, 0));

    fixture_init(&fixture, 500, 1000);
    cell = set_land(&fixture, 18, 2, 2);
    if (cell != NULL) {
        cell->has_block = 0;
    }
    ok = property_after_move(&fixture.property, 0, 18, 0U, &result) ==
             PROPERTY_OK && result.toll == 300 && fixture.money[0] == 200 &&
         fixture.money[1] == 1300 && cell != NULL && !cell->has_block;
    record_case("Case_A10_023", ok);

    fixture_init(&fixture, 1000, 1000);
    (void)set_land(&fixture, 29, 2, 3);
    before_player = fixture.money[0];
    before_owner = fixture.money[1];
    ok = property_after_move(&fixture.property, 0, 28, 0U, &result) ==
             PROPERTY_NOT_APPLICABLE &&
         fixture.money[0] == before_player && fixture.money[1] == before_owner;
    record_case("Case_A10_024", ok);

    record_case("Case_A10_025", toll_case(29, 3, 1000,
        PROPERTY_TOLL_EXEMPT_FORTUNE, 0, 1000, 1000, 0, 0));
}

static int sale_formula_case(int position, int level, int expected_sale,
                             int keep_another_property)
{
    Fixture fixture;
    PropertyResult result;
    MapCell *target;
    int spare_position = position == 1 ? 2 : 1;
    fixture_init(&fixture, 100, 0);
    target = set_land(&fixture, position, 1, level);
    if (keep_another_property) {
        (void)set_land(&fixture, spare_position, 1, 0);
    }
    return property_sell(&fixture.property, 0, position, &result) ==
               PROPERTY_OK &&
           result.sale_price == expected_sale &&
           fixture.money[0] == 100 + expected_sale &&
           target != NULL && target->owner_id == RICH_NO_OWNER &&
           target->building_level == 0 &&
           property_count_player_properties(&fixture.property, 0) ==
               (keep_another_property ? 1 : 0);
}

static void test_a11(void)
{
    static const int positions[12] = {
        1, 1, 1, 1, 29, 29, 29, 29, 36, 36, 36, 36
    };
    static const int levels[12] = {
        0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3
    };
    static const int prices[12] = {
        400, 800, 1200, 1600,
        1000, 2000, 3000, 4000,
        600, 1200, 1800, 2400
    };
    static const int special_positions[7] = {0, 14, 28, 35, 49, 63, 65};
    Fixture fixture;
    PropertyResult result;
    PropertySellPermission permission;
    PropertyCode code;
    char case_id[32];
    int position;
    int index;
    int ok;

    for (index = 0; index < 12; ++index) {
        (void)snprintf(case_id, sizeof(case_id), "Case_A11_%03d", index + 1);
        record_case(case_id, sale_formula_case(
            positions[index], levels[index], prices[index], index < 11));
    }

    fixture_init(&fixture, 100, 100);
    (void)set_land(&fixture, 1, 2, 0);
    code = property_sell(&fixture.property, 0, 1, &result);
    record_case("Case_A11_013", code == PROPERTY_ERR_NOT_OWNER &&
        strcmp(property_code_string(code), "该房产不属于你") == 0);

    fixture_init(&fixture, 100, 100);
    code = property_sell(&fixture.property, 0, 1, &result);
    record_case("Case_A11_014", code == PROPERTY_ERR_NO_PROPERTY &&
        strcmp(property_code_string(code), "该位置没有房产") == 0);

    for (index = 0; index < 7; ++index) {
        fixture_init(&fixture, 100, 100);
        code = property_sell(&fixture.property, 0,
                             special_positions[index], &result);
        (void)snprintf(case_id, sizeof(case_id),
                       "Case_A11_%03d", index + 15);
        record_case(case_id, code == PROPERTY_ERR_NOT_SELLABLE &&
            strcmp(property_code_string(code), "该位置不可出售") == 0);
    }

    code = property_parse_position("-1", &position);
    record_case("Case_A11_022", code == PROPERTY_ERR_INVALID_POSITION &&
        strcmp(property_code_string(code), "无效的位置编号") == 0);
    code = property_parse_position("70", &position);
    record_case("Case_A11_023", code == PROPERTY_ERR_INVALID_POSITION &&
        strcmp(property_code_string(code), "无效的位置编号") == 0);
    code = property_parse_position("3.5", &position);
    record_case("Case_A11_024", code == PROPERTY_ERR_INVALID_POSITION_FORMAT &&
        strcmp(property_code_string(code), "请输入有效的整数位置") == 0);
    code = property_parse_position("abc", &position);
    record_case("Case_A11_025", code == PROPERTY_ERR_INVALID_POSITION_FORMAT &&
        strcmp(property_code_string(code), "请输入有效的整数位置") == 0);

    permission.is_current_turn = 1;
    permission.is_pre_roll = 0;
    permission.is_restrained = 0;
    fixture_init(&fixture, 100, 100);
    (void)set_land(&fixture, 1, 1, 0);
    code = property_sell_checked(&fixture.property, 0, 1, permission, &result);
    record_case("Case_A11_026", code == PROPERTY_ERR_NOT_PRE_ROLL &&
        strcmp(property_code_string(code),
               "掷骰后无法出售房产，请下回合操作") == 0);

    permission.is_current_turn = 0;
    permission.is_pre_roll = 1;
    code = property_sell_checked(&fixture.property, 0, 1, permission, &result);
    record_case("Case_A11_027", code == PROPERTY_ERR_NOT_CURRENT_TURN &&
        strcmp(property_code_string(code), "请等待你的回合") == 0);

    permission.is_current_turn = 1;
    permission.is_pre_roll = 1;
    permission.is_restrained = 1;
    code = property_sell_checked(&fixture.property, 0, 1, permission, &result);
    record_case("Case_A11_028", code == PROPERTY_ERR_PLAYER_RESTRAINED &&
        strcmp(property_code_string(code), "你正在住院，无法操作") == 0);

    fixture_init(&fixture, 100, 100);
    (void)set_land(&fixture, 6, 1, 0);
    code = property_parse_sell_command("SELL 6", &position);
    ok = code == PROPERTY_OK && position == 6 &&
         property_sell(&fixture.property, 0, position, &result) == PROPERTY_OK &&
         result.sale_price == 400 && fixture.money[0] == 500;
    record_case("Case_A11_029", ok);

    fixture_init(&fixture, 100, 100);
    (void)set_land(&fixture, 34, 1, 0);
    code = property_parse_sell_command("Sell 34", &position);
    ok = code == PROPERTY_OK && position == 34 &&
         property_sell(&fixture.property, 0, position, &result) == PROPERTY_OK &&
         result.sale_price == 1000 && fixture.money[0] == 1100;
    record_case("Case_A11_030", ok);
}

static void test_runtime_integration(void)
{
    static const int roles[2] = {1, 2};
    Game game;
    char message[2048];
    game_init(&game);
    (void)game_start(&game);
    game.runtime = runtime_create(2, 1000, roles);
    INTEGRATION_CHECK(game.runtime != NULL);
    if (game.runtime == NULL) {
        return;
    }
    INTEGRATION_CHECK(runtime_begin(game.runtime, message, sizeof(message)) == 0);

    INTEGRATION_CHECK(command_execute(&game, "Step 1", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(game.context == CONTEXT_BUY_CONFIRM);
    INTEGRATION_CHECK(command_execute(&game, "A", message,
                                      sizeof(message)) == COMMAND_INVALID);
    INTEGRATION_CHECK(game.context == CONTEXT_BUY_CONFIRM);
    INTEGRATION_CHECK(command_execute(&game, "y", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 0) == 800);

    INTEGRATION_CHECK(command_execute(&game, "Step 1", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 0) == 900);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 1) == 900);

    INTEGRATION_CHECK(command_execute(&game, "Step 70", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(game.context == CONTEXT_UPGRADE_CONFIRM);
    INTEGRATION_CHECK(command_execute(&game, "Y", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 0) == 700);

    INTEGRATION_CHECK(command_execute(&game, "Step 70", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 0) == 900);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 1) == 700);

    INTEGRATION_CHECK(command_execute(&game, "SELL 1", message,
                                      sizeof(message)) == COMMAND_OK);
    INTEGRATION_CHECK(runtime_player_money(game.runtime, 0) == 1700);
    runtime_destroy(game.runtime);
    game.runtime = NULL;
}

int main(void)
{
    test_a9();
    test_a10();
    test_a11();
    test_runtime_integration();
    (void)printf("A9/A10/A11 property cases: %d/71 passed\n", passed_count);
    if (passed_count + failed_count != 71) {
        (void)fprintf(stderr, "case count mismatch: %d\n",
                      passed_count + failed_count);
        return 2;
    }
    (void)printf("property runtime integration: %d assertions, %d failed\n",
                 integration_assertions, integration_failures);
    return failed_count == 0 && integration_failures == 0 ? 0 : 1;
}
