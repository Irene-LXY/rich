# 大富翁基础地图模块

本阶段实现 70 格矩形环形地图、地图绘制和角色/骰子接入接口，不包含土地交易、资产和格子事件的完整业务逻辑。

## 地图编号

- 0～28：上边，从左向右（0 为起点）
- 29～34：右边，从上向下（6 块黄金地段）
- 35～63：下边，从右向左
- 64～69：左边，从下向上（6 个矿地，69 为最后一格）

地图为 29×8 的矩形外框。`GameMap` 中的一维数组是唯一真实数据；`renderMap` 生成显示画面，角色覆盖不会改变格子类型。

## 接口

- `PlayerToken`：地图所需的最小角色信息，可由后续完整 `Player` 转换得到。
- `IDice`：骰子抽象；`RandomDice` 是当前 1～6 随机实现。
- `movePlayer`：逐格移动，并在每次进入格子时调用回调。回调返回 `false` 即停止，可用于路障、炸弹。
- `renderMap`：接收任意角色列表；单人显示角色字符，多人同格显示人数。
- `MapCell`：已预留所有者、建筑等级、路障、炸弹字段。

## 编译与运行

使用 CMake（安装了 Visual Studio C++ 工具链时）：

```text
cmake -S . -B build
cmake --build build
build\rich.exe
```

本机只有 g++ 时可选择 Ninja 生成器：

```text
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
build\rich.exe
```

也可以直接使用 g++：

```text
g++ -std=c++17 -Iinclude src/main.cpp src/map.cpp src/game_interfaces.cpp -o rich.exe
rich.exe
```

演示程序支持 `roll`、`step n`、`map`、`where`、`help` 和 `quit`。这些命令用于验证接口，不代替最终游戏回合模块。

## 测试

```text
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build
ctest --test-dir build --output-on-failure
```
