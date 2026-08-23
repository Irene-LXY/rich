#ifndef RICH_GAME_INTERFACES_H
#define RICH_GAME_INTERFACES_H

#include "map.h"

#include <functional>
#include <random>
#include <string>
#include <vector>

namespace rich {

enum class ConsoleColor {
    Default,
    Red,
    Green,
    Blue,
    Yellow
};

// 地图模块只依赖这份轻量角色数据；完整 Player 可在后续扩展或转换成它。
struct PlayerToken {
    int id = 0;
    std::string name;
    char symbol = '?';
    ConsoleColor color = ConsoleColor::Default;
    int position = 0;
    bool active = true;
};

// 骰子抽象接口：正式随机骰子和测试用固定骰子都可接入。
class IDice {
public:
    virtual ~IDice() = default;
    virtual int roll() = 0;
};

class RandomDice final : public IDice {
public:
    RandomDice();
    explicit RandomDice(unsigned int seed);
    int roll() override;

private:
    std::mt19937 engine_;
    std::uniform_int_distribution<int> distribution_{1, 6};
};

struct MoveContext {
    int requestedSteps = 0;
    int completedSteps = 0;
    bool interrupted = false;
};

// 每进入一个格子调用一次。返回 false 可中断移动，供后续路障、炸弹使用。
using EnterCellHandler =
    std::function<bool(PlayerToken&, const MapCell&, const MoveContext&)>;

MoveContext movePlayer(const GameMap& map,
                       PlayerToken& player,
                       int steps,
                       const EnterCellHandler& onEnter = {});

// 角色只覆盖显示层。同格多人显示人数，不会修改地图真实数据。
std::string renderMap(const GameMap& map,
                      const std::vector<PlayerToken>& players,
                      bool useAnsiColor = false,
                      bool showIndices = false);

} // namespace rich

#endif
