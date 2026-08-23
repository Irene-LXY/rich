# 大富翁（C11）集成主干

大富翁桌面游戏的 C11 实现，采用敏捷迭代、多人并行开发。当前已集成多个 Story，可完成「开局引导 → 掷骰移动 → 回合切换 → 查询 / 退出」的最小可玩流程。

## 已实现功能

| Story | 功能 | 模块 |
|-------|------|------|
| A1 | 命令启动、启动参数检查、开局引导、实例隔离 | `src/startup/` |
| A4 | 回合管理（2~4 名玩家依次行动、跳过回合） | `src/a4/` |
| A5 | 70 格地图、玩家移动、骰子、地图渲染 | `src/map/` |
| A8 | 掷骰子移动、落地处理（空地 / 医院 / 监狱 / 矿地等） | `src/runtime/` |
| A20 | 输入 `Quit` 强制结束整局游戏 | `src/commands/` |

> 待接入：购买空地、过路费、房产升级、道具屋 / 魔法屋 / 礼品屋流程、路障 / 炸弹、破产胜负判定等，详见 `docs/接口文档.md`。

## 项目架构

```text
include/
├── monopoly/   主干公共接口（game / command / startup / runtime）
├── a4/         回合管理接口
└── map/        地图接口
src/
├── main.c      程序入口
├── core/       游戏状态核心
├── startup/    启动引导
├── commands/   命令分发与实现
├── runtime/    集成层（粘合 A4 + map + A8）
├── a4/         回合管理实现
└── map/        地图、移动、骰子、渲染实现
tests/          C 语言自动化测试
docs/           需求、接口、迭代说明
scripts/        构建脚本
.github/workflows/ 持续集成
```

## 环境

- C11 编译器：GCC、Clang 或 MSVC；
- CMake 3.16 及以上；
- 不使用平台专属系统调用，测试不依赖 PowerShell、批处理或固定文件路径。

## 构建与运行

```sh
cmake -S . -B build
cmake --build build
```

Windows（多配置生成器，默认 Debug）：

```powershell
.\build\Debug\monopoly.exe
```

Linux / macOS（单配置生成器）：

```sh
./build/monopoly
```

> 中文在 Windows 控制台乱码时，先执行 `chcp 65001`。

## 游戏命令

| 命令 | 说明 |
|------|------|
| `Roll` | 当前玩家掷骰子移动 |
| `Query` | 查询当前玩家位置与资金 |
| `Map` | 显示地图 |
| `Help` | 显示帮助 |
| `Quit` | 结束整局游戏 |

开局引导流程：输入玩家人数（2-4）→ 输入初始资金 → 角色自动分配（Q / A / S / J）。

## 运行测试

```sh
ctest --test-dir build -C Debug --output-on-failure
```

覆盖 4 个测试：`A1_startup`、`A4_turn_manager`、`A5_map`、`A20_quit`。CI 会在 Windows、Linux、macOS 上执行相同步骤。

## 文档

- `docs/接口文档.md`：各模块接口与 Story 接入点（开发必读）；
- `docs/ITERATION_1_PLAN.md`：迭代计划与 8 人分工；
- `docs/A1_REQUIREMENT.md`、`docs/A20_REQUIREMENT.md`：需求说明；
- `docs/legacy/`：历史版本归档。
