#include "game_interfaces.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string lowerCopy(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

void printHelp() {
    std::cout
        << "当前地图演示命令：\n"
        << "  roll       掷1～6点骰子并移动演示角色\n"
        << "  step n     指定移动n步，用于测试环形地图\n"
        << "  map        重新显示地图\n"
        << "  where      显示当前位置和格子真实类型\n"
        << "  help       显示帮助\n"
        << "  quit       退出\n";
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    rich::GameMap map;
    rich::RandomDice dice;

    // 临时演示角色。后续完整玩家模块只需向renderMap传入PlayerToken列表。
    rich::PlayerToken demoPlayer{1, "钱夫人", 'Q', rich::ConsoleColor::Red, 0, true};
    std::vector<rich::PlayerToken> players{demoPlayer};

    std::cout << "大富翁基础地图（70格，顺时针编号）\n";
    std::cout << rich::renderMap(map, players, false, true);
    printHelp();

    std::string line;
    while (true) {
        std::cout << "\n地图演示> ";
        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);
        std::string command;
        input >> command;
        command = lowerCopy(command);

        if (command == "quit") {
            break;
        }
        if (command == "help") {
            printHelp();
            continue;
        }
        if (command == "map") {
            std::cout << rich::renderMap(map, players, false, true);
            continue;
        }
        if (command == "where") {
            const int position = players.front().position;
            std::cout << players.front().name << "位于 " << position << "："
                      << map.cellDescription(position) << '\n';
            continue;
        }

        int steps = 0;
        if (command == "roll") {
            steps = dice.roll();
            std::cout << "骰子点数：" << steps << '\n';
        } else if (command == "step") {
            std::string extra;
            if (!(input >> steps) || (input >> extra)) {
                std::cout << "用法：step n（例如 step 5）\n";
                continue;
            }
        } else if (command.empty()) {
            continue;
        } else {
            std::cout << "未知命令，请输入 help 查看帮助。\n";
            continue;
        }

        // 当前只演示移动。回调已预留给矿地、路障、炸弹等“经过事件”。
        const rich::MoveContext result = rich::movePlayer(
            map, players.front(), steps,
            [](rich::PlayerToken&, const rich::MapCell&, const rich::MoveContext&) {
                return true;
            });
        std::cout << "实际移动 " << result.completedSteps << " 步，当前位置："
                  << players.front().position << "（"
                  << map.cellDescription(players.front().position) << "）\n";
        std::cout << rich::renderMap(map, players, false, false);
    }

    std::cout << "地图演示已退出。\n";
    return 0;
}
