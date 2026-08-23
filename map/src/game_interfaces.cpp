#include "game_interfaces.h"

#include <array>
#include <chrono>
#include <sstream>
#include <unordered_map>

namespace rich {

namespace {

const char* ansiCode(ConsoleColor color) {
    switch (color) {
    case ConsoleColor::Red:    return "\033[31m";
    case ConsoleColor::Green:  return "\033[32m";
    case ConsoleColor::Blue:   return "\033[34m";
    case ConsoleColor::Yellow: return "\033[33m";
    case ConsoleColor::Default:return "\033[0m";
    }
    return "\033[0m";
}

struct DisplayCell {
    char symbol = ' ';
    ConsoleColor color = ConsoleColor::Default;
    bool colored = false;
};

} // namespace

RandomDice::RandomDice()
    : RandomDice(static_cast<unsigned int>(
          std::chrono::high_resolution_clock::now().time_since_epoch().count())) {}

RandomDice::RandomDice(unsigned int seed) : engine_(seed) {}

int RandomDice::roll() {
    return distribution_(engine_);
}

MoveContext movePlayer(const GameMap& map,
                       PlayerToken& player,
                       int steps,
                       const EnterCellHandler& onEnter) {
    MoveContext context;
    context.requestedSteps = steps;

    const std::vector<int> path = map.makePath(player.position, steps);
    for (int position : path) {
        player.position = position;
        ++context.completedSteps;

        if (onEnter && !onEnter(player, map.cellAt(position), context)) {
            context.interrupted = true;
            break;
        }
    }
    return context;
}

std::string renderMap(const GameMap& map,
                      const std::vector<PlayerToken>& players,
                      bool useAnsiColor,
                      bool showIndices) {
    std::array<std::array<DisplayCell, kMapWidth>, kMapHeight> canvas{};

    // 先画真实地图。
    for (int index = 0; index < kMapSize; ++index) {
        const auto [x, y] = GameMap::screenPosition(index);
        canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)].symbol =
            map.baseSymbol(index);
    }

    // 再叠加角色。多人同格时用人数2～4表示。
    std::array<std::vector<const PlayerToken*>, kMapSize> occupants;
    for (const PlayerToken& player : players) {
        if (player.active) {
            occupants[static_cast<std::size_t>(
                GameMap::normalizePosition(player.position))].push_back(&player);
        }
    }

    for (int index = 0; index < kMapSize; ++index) {
        const auto& here = occupants[static_cast<std::size_t>(index)];
        if (here.empty()) {
            continue;
        }
        const auto [x, y] = GameMap::screenPosition(index);
        DisplayCell& display = canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
        display.symbol = here.size() == 1
            ? here.front()->symbol
            : static_cast<char>('0' + here.size());
        display.color = here.front()->color;
        display.colored = true;
    }

    std::ostringstream output;
    for (int y = 0; y < kMapHeight; ++y) {
        for (int x = 0; x < kMapWidth; ++x) {
            const DisplayCell& display =
                canvas[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            if (useAnsiColor && display.colored) {
                output << ansiCode(display.color) << display.symbol << "\033[0m";
            } else {
                output << display.symbol;
            }
        }
        output << '\n';
    }

    if (showIndices) {
        output << "\n关键位置：S=0, H=14, T=28, G=35, P=49, M=63, 矿地=64～69\n";
    }
    return output.str();
}

} // namespace rich
