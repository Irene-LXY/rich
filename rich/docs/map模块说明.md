# 大富翁基础地图模块（C语言）

本阶段使用纯 C11 实现 70 格矩形环形地图、地图绘制和角色/骰子接入接口，不包含土地交易、资产和格子事件的完整业务逻辑。

## 地图编号

- 0～28：上边，从左向右（0 为起点）
- 29～34：右边，从上向下（6 块黄金地段）
- 35～63：下边，从右向左
- 64～69：左边，从下向上（6 个矿地，69 为最后一格）

地图为 29×8 的矩形外框。`GameMap` 中的一维数组是唯一真实数据；`render_map` 生成显示画面，角色覆盖不会改变格子类型。

## 接口

- `PlayerToken`：地图所需的最小角色结构体，可由后续完整 `Player` 转换得到。
- `Dice`：由上下文指针和函数指针组成的 C 语言骰子接口；`RandomDice` 是当前 1～6 随机实现。
- `move_player`：逐格移动，并在每次进入格子时调用 C 回调函数。回调返回 0 即停止，可用于路障、炸弹。
- `render_map`：接收角色数组；单人显示角色字符，多人同格显示人数。
- `MapCell`：已预留所有者、建筑等级、路障、炸弹字段。

## 编译与运行

使用 CMake（安装了 C 编译工具链时）：

```text
cmake -S . -B build_c
cmake --build build_c
build_c\rich.exe
```

本机使用 gcc 时可选择 Ninja 生成器：

```text
cmake -S . -B build_c -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build_c
build_c\rich.exe
```

也可以直接使用 gcc：

```text
gcc -std=c11 -Iinclude src/main.c src/map.c src/game_interfaces.c -o rich.exe
rich.exe
```

演示程序支持 `roll`、`step n`、`map`、`where`、`help` 和 `quit`。这些命令用于验证接口，不代替最终游戏回合模块。

## 测试

```text
cmake -S . -B build_c -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build_c
ctest --test-dir build_c --output-on-failure
```
