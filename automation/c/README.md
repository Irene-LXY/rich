# 大富翁自动化测试脚本（C 语言版）

这是测试人员侧的自动化测试运行器，用纯 C 编写，**不依赖任何第三方库**，
可跨 Windows / Linux / macOS 编译。

职责：

1. 读取测试用例 JSON；
2. 调用开发人员提供的程序；
3. 按 `大富翁游戏自动化测试JSON接口规范v1.1` 做 Expected 部分匹配判定；
4. 汇总输出 PASS / FAIL / ERROR，并写 `results.json`（可选 JUnit XML）。

## 编译

在上一级目录 `automation/` 下用 CMake 自动发现编译器并构建（不指定具体编译器）：

```bash
cmake -S . -B build
cmake --build build
```

生成 `run_tests`（Windows 下为 `run_tests.exe`）。源码使用纯 ASCII 字符，不依赖 `/utf-8` 编译选项。

## 运行

```text
run_tests.exe --program <程序命令> --cases <用例文件或目录> [选项]
```

示例：

```bat
run_tests.exe --program rich.exe --cases testcases
run_tests.exe --program "..\..\..\Desktop\main\rich\build\monopoly_test.exe" --cases testcases --junit junit.xml
run_tests.exe --program rich.exe --cases testcases\land\TC-LAND-001.json --out results.json
```

参数：

- `--program <cmd>`：程序命令（原样执行，后面会自动追加两个文件参数）。
- `--cases <path>`：单个用例 JSON 或目录（递归扫描 `*.json`）。
- `--map <path>`：显式地图文件（覆盖用例里的 `map_file`）。
- `--map-dir <dir>`：`map_file` 的解析目录，默认 `spec/`（相对当前工作目录）。
- `--out <file>`：结果文件，默认 `results.json`。
- `--junit <file>`：可选 JUnit XML。
- `--quiet`：只输出汇总。

退出码：全部通过为 `0`，存在 FAIL/ERROR 为 `1`（便于接 CI）。

## 程序调用契约（重要）

> 接口规范没有规定“测试程序如何调用游戏程序”，下面的方式由测试侧自行定义。

调用方式：

```text
<program> <test_case.json> <map.json>
```

第 1 个参数是测试用例文件路径，第 2 个参数是解析后的地图文件路径。

程序必须向 stdout 输出一个 JSON 对象，二选一：

1. 完整 Actual 状态（接口规范表 28 中 `"actual"` 的值）。
2. 完整结果报告（含 `"result": "PASS"|"FAIL"|"ERROR"` 和 `"errors"`）。

如果程序只输出了 Actual 状态但没包一层 `"actual"`，运行器也能识别。
程序崩溃、超时、输出非 JSON、无输出都会被判为 ERROR。

## 判定规则（部分匹配）

与接口规范第 11 节一致：

- 标量完全相等，且类型一致（`true` 不等于 `1`）。
- 对象递归部分匹配，只比对 `expected` 中写出的键。
- 数组按主键匹配、不按顺序、不比长度：
  - `players` → `id`
  - `properties` → `position`
  - `map_items` → `position`
  - `display_players` → `position`
- `properties_absent`：列出的 position 不得存在已购地产。
- `map_items_absent`：列出的 position 不得存在地图道具。

> 接口规范只点名了上面四个“按主键”的数组。其他数组（如 `users`、`dice_sequence`）
> 由测试侧规定为“有序、严格相等”。

## 测试用例与地图

测试用例 JSON 结构、`map.json` 结构，以及 70 格权威地图布局，请见上一级
`../README.md` 和 `../spec/map.json`。本目录可以直接复用它们。

## 已知限制 / 自行定义部分

以下内容在接口规范中没有规定，由测试侧自行定义；若后续调整请优先改这里：

1. 程序调用方式：`<program> <test_case.json> <map.json>`。
2. 程序 stdout 输出契约（Actual 状态或 result 报告）。
3. `map.json` 的具体结构（70 格布局严格依据客户需求 PDF）。
4. 非主键数组的“有序、严格相等”比较规则。
5. 进程级错误码 `PROCESS_ERROR`（程序启动失败等，超出规范表 36）。
6. 进程启动使用 `popen`，未实现单用例超时；程序路径/参数如含引号等特殊字符，
   建议放在无空格、无特殊字符的路径下，或自行用包装脚本启动。
7. Windows 下按系统 ANSI 代码页处理命令行路径（中文版 Windows 为 GBK），
   与中文路径兼容；JSON 文件本身始终按 UTF-8 读取。
