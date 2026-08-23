# A7 Query / A18 Step 合并包

本目录按 `rich-main` 的相对目录结构整理了合并所需的全部新增和修改文件。

## 使用方法

如果目标 `rich-main` 与本次使用的第一版主干完全一致，可以把本目录中的内容复制到 `rich-main` 根目录并允许覆盖同名文件。

如果团队已经继续修改了 `CMakeLists.txt`、`runtime.c` 或命令分发器，请不要直接覆盖，应按照下方清单进行代码合并，以免丢失其他成员的新功能。

## 1. A7/A18专用根本功能文件

这些是新增文件，应整体加入：

```text
A7_A18/monopoly/query.h
A7_A18/monopoly/step.h
A7_A18/query_format.c
A7_A18/query_command.c
A7_A18/step_command.c
A7_A18/README.md
```

## 2. 必须合并的公共集成文件

这些文件包含把A7/A18接入主程序所需的修改：

```text
CMakeLists.txt
include/monopoly/command.h
include/monopoly/runtime.h
src/commands/command_dispatcher.c
src/runtime/runtime.c
src/main.c
```

各文件作用：

- `CMakeLists.txt`：编译三个A7/A18源文件并添加头文件搜索路径和测试目标。
- `command.h`：声明Query和Step命令入口。
- `runtime.h`：声明Query、遥控骰子及资产更新接口。
- `command_dispatcher.c`：识别并转发Query和Step命令。
- `runtime.c`：保存资金、点数、道具、财神等状态，收集Query数据，并通过强制步数执行Step。
- `main.c`：把输出缓冲区扩大到8192字节，防止房产较多时Query输出被截断。

## 3. 测试和文档

以下文件不属于运行时根本功能，但建议同时合并：

```text
tests/test_a7_a18.c
docs/A7_A18_INTERFACES.md
README.md
```

## 4. 构建验证

在合并后的 `rich-main` 根目录执行：

```text
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=gcc
cmake --build build
ctest --test-dir build --output-on-failure
```

预期测试：

```text
A20_quit
A1_startup
A7_query_A18_step
```

三项均应通过。

## 5. 功能验证

程序启动并完成玩家数量和初始资金设置后，可输入：

```text
Query
Step 143
```

`Query` 应显示完整资产；`Step 143` 应明确提示未使用随机点数，并从起点移动到3号位置。
