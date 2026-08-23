#ifndef RICH_MAP_H
#define RICH_MAP_H

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace rich {

constexpr int kMapSize = 70;
constexpr int kMapWidth = 29;
constexpr int kMapHeight = 8;
constexpr int kNoOwner = -1;

// 地图格子的真实类型。显示角色时只覆盖画面，不修改这里的数据。
enum class CellType {
    Start,
    Land,
    ToolShop,
    GiftShop,
    MagicHouse,
    Hospital,
    Prison,
    Mine
};

struct MapCell {
    int index = 0;
    CellType type = CellType::Land;
    int landPrice = 0;
    int minePoints = 0;

    // 以下字段留给土地、房屋和道具模块直接使用。
    int ownerId = kNoOwner;
    int buildingLevel = 0;
    bool hasBlock = false;
    bool hasBomb = false;
};

class GameMap {
public:
    GameMap();

    const MapCell& cellAt(int index) const;
    MapCell& cellAt(int index);

    // 将任意整数折算到 0～69，便于处理前进、后退和环形移动。
    static int normalizePosition(int position);

    // 返回从当前位置顺时针走 steps 步后的位置；steps 也可为负数。
    int destination(int currentPosition, int steps) const;

    // 返回移动过程中依次进入的所有格子，不包含起始格。
    std::vector<int> makePath(int currentPosition, int steps) const;

    // 把一维地图编号转换为矩形画面坐标。
    static std::pair<int, int> screenPosition(int mapIndex);

    char baseSymbol(int index) const;
    std::string cellDescription(int index) const;
    const std::array<MapCell, kMapSize>& cells() const noexcept;

private:
    std::array<MapCell, kMapSize> cells_{};
    void initialize();
};

} // namespace rich

#endif
