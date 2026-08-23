# P0-A15A16 最小提交版本

本目录只保留 A15 礼品屋、A16 魔法屋及其接入 `rich-main` 所需的代码、接口和测试。

## A15 礼品屋

- `include/monopoly/gift.h`：公共接口；
- `src/story/gift.c`：奖金 2000、点数卡 200、财神 5 轮；
- `tests/test_a15.c`：模块与主程序集成测试；
- `docs/A15_礼品屋接口文档.md`：接口和规则说明。

## A16 魔法屋

- `include/monopoly/magic.h`：公共接口及可注册魔法回调；
- `src/story/magic.c`：魔法选择、执行、拒绝和退出流程；
- `tests/test_a16.c`：模块与主程序集成测试；
- `docs/A16_魔法屋接口文档.md`：接口和扩展说明。

`src/runtime/runtime.c` 和 `src/commands/command_dispatcher.c` 是 A15/A16 接入 70 格地图、A4 回合管理及统一命令入口的必要代码，其余 `src/include` 文件是生成完整控制台程序所需的依赖。

## 编译与测试

```powershell
$env:PATH="C:\msys64\ucrt64\bin;C:\msys64\usr\bin;$env:PATH"
cmake -S . -B build-test -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-test
ctest --test-dir build-test --output-on-failure
```

预期结果：

```text
100% tests passed, 0 tests failed out of 2
```

生成文件：

- `build-test/monopoly.exe`：完整控制台程序；
- `build-test/test_a15.exe`：A15 自动测试；
- `build-test/test_a16.exe`：A16 自动测试。

手动测试特殊格子：启动 `monopoly.exe`，完成开局输入后使用 `Step 35` 进入礼品屋，使用 `Step 63` 进入魔法屋。
