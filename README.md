# RichMan 大富翁 —— A6（帮助命令）

基于《大富翁.pdf》与《大富翁游戏自动化测试JSON接口规范v1.1》开发的 C 语言项目。
本文件夹在既有功能（A2 角色选择、A3 初始资金、A17/A21 破产）基础上实现：

- **A6 帮助命令**：随时打开帮助界面查看完整指令集；**Help 命令忽略大小写**；
  列出全部命令的**用途、参数与简短示例**。其余命令（QUERY/ROLL/STEP/QUIT）
  仅识别，不实现其功能。

## 指令集（帮助界面内容）

| 命令 | 用途 | 参数 | 示例 |
|------|------|------|------|
| HELP | 打开帮助界面，随时查看完整指令集（忽略大小写） | 无 | `HELP` |
| QUERY | 查询当前玩家资金、点数、房产、剩余道具及财神等状态剩余轮数 | 无 | `QUERY` |
| ROLL | 掷骰子，随机生成 1~6 步并按点数移动 | 无 | `ROLL` |
| STEP n | 遥控骰子，指定任意行走步数，不使用随机点数 | n＝步数（正整数） | `STEP 5` |
| QUIT | 强制结束，整局游戏结束 | 无 | `QUIT` |

## 目录结构（标准 C 项目布局）

```
A6/
├── include/            头文件（模块接口声明）
│   ├── character.h     角色模块
│   ├── player.h        玩家模块（资金、状态）
│   ├── game.h          游戏模块（地产、资金写入、交易收费、破产）
│   ├── selection.h     角色选择向导（A2）
│   ├── fund.h          初始资金设置向导（A3）
│   ├── help.h          帮助模块（A6）★
│   ├── cli.h           指令输入循环（A6，“随时打开帮助”的宿主环境）★
│   ├── input.h         输入工具
│   ├── console.h       控制台 UTF-8 / ANSI 颜色
│   └── demo.h          破产机制演示（A17/A21）
├── src/                源文件（同名 .c 实现 + main.c）
├── bin/                二进制文件（编译输出 A6.exe）
├── obj/                编译中间产物
├── docs/               设计文档（角色选择 / 初始资金与破产 / 帮助）★
├── tests/              测试用例（角色选择 / 初始资金与破产 / 帮助）★
├── Makefile            gcc / MinGW 构建脚本
├── build.bat           MSVC 一键编译脚本（Windows）
└── README.md
```

## 编译运行

### 方式一：MSVC（Visual Studio 2022，Windows）

```bat
build.bat
bin\A6.exe
```

### 方式二：gcc / MinGW / Linux

```bash
make
./bin/A6            # Windows 下为 ./bin/A6.exe
```

### 帮助功能入口

```bat
bin\A6.exe --help    :: 不开局，直接查看完整指令集
bin\A6.exe           :: 开局（选人数→选角色→初始资金）后进入指令输入，
                        随时输入 help / HELP / hElP 打开帮助界面
bin\A6.exe --demo    :: 破产机制演示（A17/A21 回归）
```

## 关键行为

- **忽略大小写**：`help` / `HELP` / `Help` / `hElP` 均打开帮助；首尾空格容忍；
  整词匹配（`helps`、`hel`、`help me` 不触发）；
- **随时可用**：对局指令循环中每次输入都识别 HELP，可重复打开；
- **只实现查看**：QUERY / ROLL / STEP n 识别后提示“由对应模块实现”，不执行其逻辑；
  按接口规范，除 HELP 外命令统一使用大写（小写 `query` 判为未知命令并建议使用 HELP）。

详见 `docs/帮助模块设计说明.md` 与 `tests/帮助功能测试用例.md`。
