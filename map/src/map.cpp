#include "map.h"

#include <stdexcept>

namespace rich {

namespace {

MapCell makeCell(int index, CellType type, int price = 0, int points = 0) {
    MapCell cell;
    cell.index = index;
    cell.type = type;
    cell.landPrice = price;
    cell.minePoints = points;
    return cell;
}

} // namespace

GameMap::GameMap() {
    initialize();
}

void GameMap::initialize() {
    // 先初始化所有格子的编号，随后按地图规则覆盖类型。
    for (int i = 0; i < kMapSize; ++i) {
        cells_[i] = makeCell(i, CellType::Land);
    }

    // 上边：S + 13块地 + H + 13块地 + T（地段1，共26块，每块200元）。
    cells_[0] = makeCell(0, CellType::Start);
    for (int i = 1; i <= 13; ++i) {
        cells_[i] = makeCell(i, CellType::Land, 200);
    }
    cells_[14] = makeCell(14, CellType::Hospital);
    for (int i = 15; i <= 27; ++i) {
        cells_[i] = makeCell(i, CellType::Land, 200);
    }
    cells_[28] = makeCell(28, CellType::ToolShop);

    // 右边：地段2，6块黄金地段，每块500元。
    for (int i = 29; i <= 34; ++i) {
        cells_[i] = makeCell(i, CellType::Land, 500);
    }

    // 下边按顺时针方向从右向左：G + 13块地 + P + 13块地 + M。
    cells_[35] = makeCell(35, CellType::GiftShop);
    for (int i = 36; i <= 48; ++i) {
        cells_[i] = makeCell(i, CellType::Land, 300);
    }
    cells_[49] = makeCell(49, CellType::Prison);
    for (int i = 50; i <= 62; ++i) {
        cells_[i] = makeCell(i, CellType::Land, 300);
    }
    cells_[63] = makeCell(63, CellType::MagicHouse);

    // 左边编号从下向上递增；奖励要求按画面从上到下为20、80、100、40、80、60。
    cells_[64] = makeCell(64, CellType::Mine, 0, 60);
    cells_[65] = makeCell(65, CellType::Mine, 0, 80);
    cells_[66] = makeCell(66, CellType::Mine, 0, 40);
    cells_[67] = makeCell(67, CellType::Mine, 0, 100);
    cells_[68] = makeCell(68, CellType::Mine, 0, 80);
    cells_[69] = makeCell(69, CellType::Mine, 0, 20);
}

const MapCell& GameMap::cellAt(int index) const {
    if (index < 0 || index >= kMapSize) {
        throw std::out_of_range("地图编号必须在0～69之间");
    }
    return cells_[static_cast<std::size_t>(index)];
}

MapCell& GameMap::cellAt(int index) {
    if (index < 0 || index >= kMapSize) {
        throw std::out_of_range("地图编号必须在0～69之间");
    }
    return cells_[static_cast<std::size_t>(index)];
}

int GameMap::normalizePosition(int position) {
    const int result = position % kMapSize;
    return result < 0 ? result + kMapSize : result;
}

int GameMap::destination(int currentPosition, int steps) const {
    return normalizePosition(normalizePosition(currentPosition) + steps);
}

std::vector<int> GameMap::makePath(int currentPosition, int steps) const {
    std::vector<int> path;
    const int direction = steps >= 0 ? 1 : -1;
    const int count = steps >= 0 ? steps : -steps;
    int position = normalizePosition(currentPosition);
    path.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        position = normalizePosition(position + direction);
        path.push_back(position);
    }
    return path;
}

std::pair<int, int> GameMap::screenPosition(int mapIndex) {
    if (mapIndex < 0 || mapIndex >= kMapSize) {
        throw std::out_of_range("地图编号必须在0～69之间");
    }

    if (mapIndex <= 28) {                  // 上边：从左到右
        return {mapIndex, 0};
    }
    if (mapIndex <= 34) {                  // 右边：从上到下
        return {kMapWidth - 1, mapIndex - 28};
    }
    if (mapIndex <= 63) {                  // 下边：从右到左
        return {63 - mapIndex, kMapHeight - 1};
    }
    // 左边：编号从下到上，64位于倒数第二行，69位于第二行。
    return {0, 70 - mapIndex};
}

char GameMap::baseSymbol(int index) const {
    switch (cellAt(index).type) {
    case CellType::Start:      return 'S';
    case CellType::Land:       return '0';
    case CellType::ToolShop:   return 'T';
    case CellType::GiftShop:   return 'G';
    case CellType::MagicHouse: return 'M';
    case CellType::Hospital:   return 'H';
    case CellType::Prison:     return 'P';
    case CellType::Mine:       return '$';
    }
    return '?';
}

std::string GameMap::cellDescription(int index) const {
    const MapCell& cell = cellAt(index);
    switch (cell.type) {
    case CellType::Start:      return "起点";
    case CellType::Land:       return "空地（价格" + std::to_string(cell.landPrice) + "元）";
    case CellType::ToolShop:   return "道具屋";
    case CellType::GiftShop:   return "礼品屋";
    case CellType::MagicHouse: return "魔法屋";
    case CellType::Hospital:   return "医院";
    case CellType::Prison:     return "监狱";
    case CellType::Mine:       return "矿地（" + std::to_string(cell.minePoints) + "点）";
    }
    return "未知格子";
}

const std::array<MapCell, kMapSize>& GameMap::cells() const noexcept {
    return cells_;
}

} // namespace rich
