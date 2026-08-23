# 大富翁（C11）集成主干

这是供8人并行开发和自动化测试使用的最小主干。目前实现：

- A1：单一命令启动、启动参数检查、开局引导入口和实例隔离；
- A7：`Query` 显示资金、点数、房产、剩余道具、财神、住院、监狱及破产状态；
- A18：`Step n` 遥控骰子指定任意正整数步数，完全绕过随机点数；
- A20：输入 `Quit` 强制结束整局游戏。

## 目录

```text
include/monopoly/     全组共用的公共接口
src/core/             游戏状态等公共核心
src/commands/         各命令功能；每个Story尽量独立文件
tests/                C语言自动化测试
docs/                 Story、冲突记录、迭代与合并说明
.github/workflows/    Windows/Linux/macOS持续集成
```

## 环境

- C11 编译器：GCC、Clang 或 MSVC；
- CMake 3.16及以上；
- 不使用平台专属系统调用，测试不依赖 PowerShell、批处理或固定文件路径。

## 构建与运行

```sh
cmake -S . -B build
cmake --build build
```

如果 Windows 已安装在默认位置的 MSYS2 UCRT64 GCC，也可直接执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_and_test_windows.ps1
```

脚本会临时把 `C:\msys64\ucrt64\bin` 加入当前进程 PATH，防止 GCC 的 `cc1.exe` 因找不到 `libisl-23.dll` 而弹出系统错误；不会永久修改系统环境变量。

Windows多配置生成器运行：

```powershell
.\build\Debug\monopoly.exe
```

Linux/macOS或单配置生成器运行：

```sh
./build/monopoly
```

输入：

```text
Query
Step 143
Quit
```

`Step n` 中的 `n` 不限制为 1～6；因为行走步数必须为正，`Step 0`、负数、非整数和多余参数会被拒绝。

## 自动运行全部测试

```sh
ctest --test-dir build -C Debug --output-on-failure
```

CI 会在 Windows、Linux、macOS 上自动执行相同的配置、构建和测试步骤。

## 当前自动化测试范围

A1：

- 单一命令正常启动；
- 启动后进入玩家人数步骤并显示完整开局顺序；
- 非法参数拒绝且不留下状态；
- 同一实例重复启动；
- 两个游戏实例相互隔离。

A20：

- `quit` 的大小写组合；
- 命令首尾空白；
- 回合开始、购买确认、礼品屋、魔法屋、住院、入狱、过路费结算等运行场景；
- 空输入、拼写错误、多余参数；
- 游戏开始前执行；
- Quit 后其他命令与重复 Quit 不再改变状态。

A7/A18：

- Query完整资产字段与房产等级；
- Query大小写和多余参数校验；
- 资金、点数、房产、道具、财神等后续模块接入接口；
- Step参数边界、非整数和多余参数；
- `Step 143`、`Step 70`的确定性环形移动；
- Step路径不调用随机骰子。

接口说明见 `docs/A7_A18_INTERFACES.md`。
