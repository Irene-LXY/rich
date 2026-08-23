#include "game_interfaces.h"

#include <cassert>
#include <iostream>

int main() {
    rich::GameMap map;

    // 基本编号、环形移动和关键格子。
    assert(map.cells().size() == 70);
    assert(map.baseSymbol(0) == 'S');
    assert(map.baseSymbol(14) == 'H');
    assert(map.baseSymbol(28) == 'T');
    assert(map.baseSymbol(35) == 'G');
    assert(map.baseSymbol(49) == 'P');
    assert(map.baseSymbol(63) == 'M');
    assert(map.baseSymbol(69) == '$');
    assert(map.destination(68, 4) == 2);
    assert(map.destination(1, -3) == 68);

    int lands200 = 0;
    int lands500 = 0;
    int lands300 = 0;
    int mines = 0;
    for (const rich::MapCell& cell : map.cells()) {
        if (cell.type == rich::CellType::Land && cell.landPrice == 200) ++lands200;
        if (cell.type == rich::CellType::Land && cell.landPrice == 500) ++lands500;
        if (cell.type == rich::CellType::Land && cell.landPrice == 300) ++lands300;
        if (cell.type == rich::CellType::Mine) ++mines;
    }
    assert(lands200 == 26);
    assert(lands500 == 6);
    assert(lands300 == 26);
    assert(mines == 6);

    // 矿地在画面上从上到下的奖励。
    assert(map.cellAt(69).minePoints == 20);
    assert(map.cellAt(68).minePoints == 80);
    assert(map.cellAt(67).minePoints == 100);
    assert(map.cellAt(66).minePoints == 40);
    assert(map.cellAt(65).minePoints == 80);
    assert(map.cellAt(64).minePoints == 60);

    // 角色覆盖不应改变底图。
    rich::PlayerToken player{1, "钱夫人", 'Q', rich::ConsoleColor::Red, 1, true};
    const std::string rendered = rich::renderMap(map, {player});
    assert(rendered.find('Q') != std::string::npos);
    assert(map.baseSymbol(1) == '0');

    // 进入格子回调能中断移动，供路障和炸弹复用。
    int entered = 0;
    const rich::MoveContext result = rich::movePlayer(
        map, player, 6,
        [&entered](rich::PlayerToken&, const rich::MapCell&, const rich::MoveContext&) {
            ++entered;
            return entered < 3;
        });
    assert(result.interrupted);
    assert(result.completedSteps == 3);
    assert(player.position == 4);

    // 固定种子骰子始终返回合法点数。
    rich::RandomDice dice(12345);
    for (int i = 0; i < 100; ++i) {
        const int value = dice.roll();
        assert(value >= 1 && value <= 6);
    }

    std::cout << "All map tests passed.\n";
    return 0;
}
