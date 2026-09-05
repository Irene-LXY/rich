# 交互黑盒测试（本组本地，C 版）

针对本组交互版程序 `monopoly.exe` 的黑盒测试，**不属于跨组 JSON 公共测试集**。
用来补 JSON 黑盒测不到的那部分：开局输入解析、命令文本解析、`query/help` 提示文字等。

> 界面文字、颜色、帮助逐字等内容各实现可以不同，所以这套用例只能给本组自己用，
> 不要放进共享的 `../testcases/`。

## 编译

在上一级目录 `automation/` 下用 CMake 自动发现编译器并构建（不指定具体编译器）：

```powershell
cmake -S . -B build
cmake --build build
```

生成 `run_interactive_tests`（Windows 下为 `run_interactive_tests.exe`）。

## 运行

```powershell
run_interactive_tests.exe --program "..\..\..\Desktop\main\rich\build\monopoly.exe" --cases cases
```

参数：

- `--program`：交互版程序路径（必填），指向 `monopoly.exe`，不是 `monopoly_test.exe`。
- `--cases`：交互用例目录（`cases/`）或单个用例 JSON。
- `--out`：结果文件，默认 `results_interactive.json`。
- `--quiet`：只打印汇总。

## 用例格式

```json
{
  "name": "启动并选择2名玩家",
  "instances": 1,
  "repetitions": 1,
  "input": ["2", "10000", "1", "2", "quit"],
  "expect": ["大富翁启动成功", "轮到玩家"],
  "forbid": ["启动失败"]
}
```

- `input`：按顺序通过 stdin 发送的一行行输入（自动每行补换行，发送完关闭 stdin）。
- `expect`：stdout 中必须出现的子串。
- `forbid`：stdout 中不得出现的子串。
- `instances`：可选，默认1；大于1时先并发启动全部实例，再分别检查每个实例的输出。
- `repetitions`：可选，默认1；完整重复执行该用例指定次数，每次重复均执行全部实例。

判定时先去掉 ANSI 清屏/颜色控制码和回车，再按子串匹配；程序由 stdin 关闭后自动结束，
因此每个用例的最后一行通常应给 `quit`（或让输入耗尽使其自动退出）。
任一重复轮次或任一实例断言失败，整个用例即为 FAIL。
