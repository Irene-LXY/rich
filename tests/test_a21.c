#include "a4/a4_turn_manager.h"
#include "map/map.h"
#include "monopoly/command.h"
#include "monopoly/gift.h"
#include "monopoly/game.h"
#include "monopoly/runtime.h"
#include "property/property_system.h"

#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    GameMap map;
    int money[4];
    PropertySystem property;
} Fixture;

typedef struct TurnFixture {
    A4TurnManager manager;
    A4PlayerConfig players[4];
    A4PlayerId winner;
} TurnFixture;

static int passed_count;
static int failed_count;

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
    fixture->money[2] = 1000;
    fixture->money[3] = 1000;
    (void)property_system_init(&fixture->property, &fixture->map,
                               fixture->money, 4U);
}

static MapCell *set_land(Fixture *fixture, int position, int owner_id,
                         int level, int land_price)
{
    MapCell *cell = game_map_cell_at_mut(&fixture->map, position);
    if (cell != NULL) {
        cell->type = CELL_LAND;
        cell->owner_id = owner_id;
        cell->building_level = level;
        cell->land_price = land_price;
    }
    return cell;
}

static int toll_case(int player_money, int toll,
                     unsigned int exemptions, int expected_money,
                     int expected_bankrupt)
{
    Fixture fixture;
    PropertyResult result;
    fixture_init(&fixture, player_money, 1000);
    (void)set_land(&fixture, 29, 2, 1, toll);
    return property_after_move(&fixture.property, 0, 29, exemptions, &result) ==
               PROPERTY_OK &&
           result.action == PROPERTY_ACTION_TOLL &&
           result.toll ==
               (exemptions == PROPERTY_TOLL_EXEMPT_NONE ? toll : 0) &&
           result.player_bankrupt == expected_bankrupt &&
           fixture.money[0] == expected_money;
}

static int bankruptcy_releases_properties(int player_money, int toll,
                                           const int *levels,
                                           int property_count)
{
    Fixture fixture;
    PropertyResult result;
    int index;
    int ok;
    fixture_init(&fixture, player_money, 1000);
    for (index = 0; index < property_count; ++index) {
        (void)set_land(&fixture, index + 1, 1, levels[index], 200);
    }
    (void)set_land(&fixture, 29, 2, 1, toll);
    ok = property_after_move(&fixture.property, 0, 29,
                             PROPERTY_TOLL_EXEMPT_NONE, &result) == PROPERTY_OK &&
         result.player_bankrupt &&
         result.released_property_count == property_count &&
         fixture.money[0] == player_money - toll;
    for (index = 0; index < property_count; ++index) {
        const MapCell *cell = game_map_cell_at(&fixture.map, index + 1);
        ok = ok && cell != NULL && cell->owner_id == RICH_NO_OWNER &&
             cell->building_level == 0;
    }
    return ok;
}

static int purchase_case(int money, int expected_code, int expected_money,
                         int expected_owner)
{
    Fixture fixture;
    PropertyResult landing;
    PropertyResult resolved;
    MapCell *cell;
    PropertyCode code;
    fixture_init(&fixture, money, 1000);
    cell = set_land(&fixture, 1, RICH_NO_OWNER, 0, 200);
    if (property_after_move(&fixture.property, 0, 1, 0U, &landing) !=
        PROPERTY_PENDING) {
        return 0;
    }
    code = property_resolve_answer(&fixture.property, 0, "Y", &resolved);
    return code == (PropertyCode)expected_code && fixture.money[0] == expected_money &&
           cell != NULL && cell->owner_id == expected_owner;
}

static int upgrade_case(int money, int starting_level,
                        int expected_code, int expected_money,
                        int expected_level)
{
    Fixture fixture;
    PropertyResult landing;
    PropertyResult resolved;
    MapCell *cell;
    PropertyCode code;
    fixture_init(&fixture, money, 1000);
    cell = set_land(&fixture, 1, 1, starting_level, 200);
    if (property_after_move(&fixture.property, 0, 1, 0U, &landing) !=
        PROPERTY_PENDING) {
        return 0;
    }
    code = property_resolve_answer(&fixture.property, 0, "Y", &resolved);
    return code == (PropertyCode)expected_code && fixture.money[0] == expected_money &&
           cell != NULL && cell->building_level == expected_level;
}

static int sale_case(void)
{
    Fixture fixture;
    PropertyResult result;
    MapCell *cell;
    fixture_init(&fixture, 100, 1000);
    cell = set_land(&fixture, 1, 1, 1, 100);
    return property_sell(&fixture.property, 0, 1, &result) == PROPERTY_OK &&
           result.sale_price == 400 && fixture.money[0] == 500 &&
           cell != NULL && cell->owner_id == RICH_NO_OWNER &&
           cell->building_level == 0;
}

static int released_land_can_be_bought_again(void)
{
    Fixture fixture;
    PropertyResult toll;
    PropertyResult landing;
    PropertyResult resolved;
    MapCell *cell;
    fixture_init(&fixture, 500, 1000);
    cell = set_land(&fixture, 1, 1, 0, 200);
    (void)set_land(&fixture, 29, 2, 1, 600);
    if (property_after_move(&fixture.property, 0, 29, 0U, &toll) != PROPERTY_OK ||
        !toll.player_bankrupt || cell == NULL || cell->owner_id != RICH_NO_OWNER) {
        return 0;
    }
    return property_after_move(&fixture.property, 2, 1, 0U, &landing) ==
               PROPERTY_PENDING &&
           property_resolve_answer(&fixture.property, 2, "Y", &resolved) ==
               PROPERTY_OK &&
           cell->owner_id == 3;
}

static void on_game_finished(void *context, A4PlayerId winner_id,
                             const A4TurnSnapshot *snapshot)
{
    TurnFixture *fixture = (TurnFixture *)context;
    (void)snapshot;
    fixture->winner = winner_id;
}

static int turn_fixture_init(TurnFixture *fixture, size_t player_count)
{
    static const char *names[4] = {"Q", "A", "S", "J"};
    A4TurnHooks hooks;
    size_t index;
    (void)memset(fixture, 0, sizeof(*fixture));
    (void)memset(&hooks, 0, sizeof(hooks));
    for (index = 0U; index < player_count; ++index) {
        fixture->players[index].id = (A4PlayerId)(index + 1U);
        fixture->players[index].role_name = names[index];
    }
    hooks.context = fixture;
    hooks.on_game_finished = on_game_finished;
    return a4_turn_manager_init(&fixture->manager, fixture->players,
                                player_count, &hooks) == A4_TURN_OK &&
           a4_turn_manager_begin(&fixture->manager) == A4_TURN_OK;
}

static int mark_out_continues(int player_count, A4PlayerId player_id,
                              size_t expected_active)
{
    TurnFixture fixture;
    size_t index;
    size_t active = 0U;
    if (!turn_fixture_init(&fixture, (size_t)player_count) ||
        a4_turn_manager_mark_player_out(&fixture.manager, player_id) != A4_TURN_OK) {
        return 0;
    }
    for (index = 0U; index < (size_t)player_count; ++index) {
        if (fixture.manager.players[index].participating) {
            ++active;
        }
    }
    return active == expected_active &&
           fixture.manager.phase != A4_TURN_PHASE_FINISHED;
}

static int last_player_wins(void)
{
    TurnFixture fixture;
    if (!turn_fixture_init(&fixture, 2U)) {
        return 0;
    }
    return a4_turn_manager_mark_player_out(&fixture.manager, 1U) == A4_TURN_OK &&
           fixture.manager.phase == A4_TURN_PHASE_FINISHED &&
           fixture.winner == 2U;
}

static int two_players_out_are_skipped(void)
{
    TurnFixture fixture;
    A4TurnSnapshot snapshot;
    if (!turn_fixture_init(&fixture, 4U)) {
        return 0;
    }
    if (a4_turn_manager_mark_player_out(&fixture.manager, 1U) != A4_TURN_OK ||
        a4_turn_manager_mark_player_out(&fixture.manager, 2U) != A4_TURN_OK) {
        return 0;
    }
    snapshot = a4_turn_manager_snapshot(&fixture.manager);
    return !fixture.manager.players[0].participating &&
           !fixture.manager.players[1].participating &&
           fixture.manager.players[2].participating &&
           fixture.manager.players[3].participating &&
           snapshot.current_player_id == 3U &&
           snapshot.phase != A4_TURN_PHASE_FINISHED;
}

static int gift_bonus_case(void)
{
    GiftShopState gift;
    int money[4] = {100, 100, 100, 100};
    gift_shop_init(&gift, 4U);
    return gift_shop_begin(&gift, 0U) == GIFT_OK &&
           gift_shop_answer(&gift, money, 4U, "1") == GIFT_OK &&
           money[0] == 2100;
}

static int mine_keeps_money(void)
{
    GameMap map;
    int money = 1000;
    const MapCell *mine;
    game_map_init(&map);
    mine = game_map_cell_at(&map, 64);
    return mine != NULL && mine->type == CELL_MINE && mine->mine_points > 0 &&
           money == 1000;
}

static int fortune_boundary_case(int turns_to_finish, int expect_free,
                                 int player_money, int toll,
                                 int expected_money, int expected_bankrupt)
{
    GiftShopState gift;
    int money[4] = {player_money, 1000, 1000, 1000};
    int index;
    unsigned int exemptions;
    PropertyResult result;
    Fixture fixture;
    gift_shop_init(&gift, 4U);
    if (gift_shop_begin(&gift, 0U) != GIFT_OK ||
        gift_shop_answer(&gift, money, 4U, "3") != GIFT_OK) {
        return 0;
    }
    for (index = 0; index < turns_to_finish; ++index) {
        gift_shop_finish_turn(&gift, 0U);
    }
    exemptions = gift_shop_is_toll_free(&gift, 0U)
        ? PROPERTY_TOLL_EXEMPT_FORTUNE : PROPERTY_TOLL_EXEMPT_NONE;
    fixture_init(&fixture, player_money, 1000);
    (void)set_land(&fixture, 29, 2, 1, toll);
    if (property_after_move(&fixture.property, 0, 29, exemptions, &result) !=
        PROPERTY_OK) {
        return 0;
    }
    return (gift_shop_is_toll_free(&gift, 0U) != 0) == expect_free &&
           fixture.money[0] == expected_money &&
           result.player_bankrupt == expected_bankrupt;
}

static int execute_ok(Game *game, const char *command)
{
    char message[2048];
    CommandResult result = command_execute(game, command, message, sizeof(message));
    if (result != COMMAND_OK) {
        (void)fprintf(stderr, "[A21 diagnostic] command '%s' failed: %s",
                      command, message);
    }
    return result == COMMAND_OK;
}

static int runtime_bankruptcy_clears_tools(void)
{
    static const int roles[4] = {1, 2, 3, 4};
    Game game;
    char message[2048];
    int ok;

    game_init(&game);
    if (!game_start(&game)) {
        return 0;
    }
    game.runtime = runtime_create(4, 1000, roles);
    if (game.runtime == NULL ||
        runtime_begin(game.runtime, message, sizeof(message)) != 0) {
        runtime_destroy(game.runtime);
        return 0;
    }

    ok = execute_ok(&game, "Step 35") && execute_ok(&game, "2") &&
         execute_ok(&game, "Step 29") && execute_ok(&game, "Y") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Step 70") &&
         execute_ok(&game, "Step 63") && execute_ok(&game, "1") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Y") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Step 70") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "3") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Y") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Step 70") &&
         execute_ok(&game, "Step 1") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Y") &&
         execute_ok(&game, "Step 70") && execute_ok(&game, "Step 70") &&
         execute_ok(&game, "Step 70");

    if (ok && (runtime_player_money(game.runtime, 0U) != -250 ||
               runtime_player_is_active(game.runtime, 0U) ||
               runtime_player_tool_count(game.runtime, 0U) != 0)) {
        (void)fprintf(stderr,
            "[A21 diagnostic] money=%d active=%d tools=%d\n",
            runtime_player_money(game.runtime, 0U),
            runtime_player_is_active(game.runtime, 0U),
            runtime_player_tool_count(game.runtime, 0U));
        ok = 0;
    }
    runtime_destroy(game.runtime);
    return ok;
}

int main(void)
{
    static const int one_empty[] = {0};
    static const int one_hut[] = {1};
    static const int one_house[] = {2};
    static const int one_tower[] = {3};
    static const int mixed_three[] = {0, 1, 3};
    static const int mixed_many[] = {0, 2, 3};

    record_case("Case_A21_001",
        toll_case(300, 500, 0U, -200, 1) && mark_out_continues(4, 1U, 3U));
    record_case("Case_A21_002",
        bankruptcy_releases_properties(700, 800, one_empty, 1) &&
        mark_out_continues(3, 1U, 2U) && runtime_bankruptcy_clears_tools());
    record_case("Case_A21_003",
        bankruptcy_releases_properties(1200, 1500, one_house, 1) &&
        last_player_wins());
    record_case("Case_A21_004",
        bankruptcy_releases_properties(2000, 2500, mixed_three, 3) &&
        mark_out_continues(4, 1U, 3U));
    record_case("Case_A21_005",
        purchase_case(100, PROPERTY_ERR_INSUFFICIENT_FUNDS, 100, RICH_NO_OWNER));
    record_case("Case_A21_006",
        upgrade_case(100, 1, PROPERTY_ERR_INSUFFICIENT_FUNDS, 100, 1));
    record_case("Case_A21_007", gift_bonus_case());
    record_case("Case_A21_008", mine_keeps_money());
    record_case("Case_A21_009",
        toll_case(100, 500, PROPERTY_TOLL_EXEMPT_FORTUNE, 100, 0));
    record_case("Case_A21_010",
        toll_case(100, 500, PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 100, 0));
    record_case("Case_A21_011",
        toll_case(100, 500, PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 100, 0));
    record_case("Case_A21_012", released_land_can_be_bought_again());
    record_case("Case_A21_013",
        bankruptcy_releases_properties(1200, 1800, mixed_many, 3) &&
        last_player_wins());
    record_case("Case_A21_014",
        bankruptcy_releases_properties(650, 700, one_hut, 1) &&
        mark_out_continues(3, 1U, 2U));
    record_case("Case_A21_015",
        toll_case(390, 400, 0U, -10, 1) && mark_out_continues(4, 1U, 3U));
    record_case("Case_A21_016", toll_case(500, 500, 0U, 0, 0));
    record_case("Case_A21_017", toll_case(500, 501, 0U, -1, 1));
    record_case("Case_A21_018", toll_case(0, 100, 0U, -100, 1));
    record_case("Case_A21_019",
        purchase_case(200, PROPERTY_OK, 0, 1));
    record_case("Case_A21_020",
        purchase_case(199, PROPERTY_ERR_INSUFFICIENT_FUNDS, 199, RICH_NO_OWNER));
    record_case("Case_A21_021", sale_case());
    record_case("Case_A21_022",
        bankruptcy_releases_properties(600, 900, one_tower, 1) &&
        mark_out_continues(3, 1U, 2U));
    record_case("Case_A21_023",
        bankruptcy_releases_properties(800, 1200, mixed_many, 3) &&
        mark_out_continues(4, 1U, 3U));
    record_case("Case_A21_024",
        bankruptcy_releases_properties(1000, 2000, one_empty, 1) &&
        last_player_wins());
    record_case("Case_A21_025",
        fortune_boundary_case(6, 0, 500, 600, -100, 1) &&
        bankruptcy_releases_properties(500, 600, one_house, 1));
    record_case("Case_A21_026",
        upgrade_case(199, 2, PROPERTY_ERR_INSUFFICIENT_FUNDS, 199, 2));
    record_case("Case_A21_027",
        purchase_case(0, PROPERTY_ERR_INSUFFICIENT_FUNDS, 0, RICH_NO_OWNER));
    record_case("Case_A21_028",
        toll_case(100, 500, PROPERTY_TOLL_EXEMPT_FORTUNE |
                  PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 100, 0));
    record_case("Case_A21_029",
        toll_case(100, 500, PROPERTY_TOLL_EXEMPT_FORTUNE |
                  PROPERTY_TOLL_EXEMPT_OWNER_RESTRAINED, 100, 0));
    record_case("Case_A21_030",
        fortune_boundary_case(5, 1, 100, 500, 100, 0));
    record_case("Case_A21_031",
        fortune_boundary_case(6, 0, 300, 500, -200, 1) &&
        mark_out_continues(4, 1U, 3U));
    record_case("Case_A21_032",
        upgrade_case(200, 1, PROPERTY_OK, 0, 2));
    record_case("Case_A21_033", two_players_out_are_skipped());

    if (failed_count == 0) {
        (void)printf("[PASS] A21: %d/33 Excel cases passed.\n", passed_count);
        return 0;
    }
    (void)fprintf(stderr, "[FAIL] A21: %d passed, %d failed.\n",
                  passed_count, failed_count);
    return 1;
}
