# A9 / A10 / A11 房产模块

本目录独立保存 A8 之后新增的全部内容，不修改项目根目录的 A8 文件。

```text
A9_A10_A11/
├─ include/property_system.h
├─ src/property_system.c
├─ tests/test_property_system.c
├─ docs/A9_A10_A11接口文档.md
└─ CMakeLists.txt
```

构建与测试：

```powershell
cmake -S A9_A10_A11 -B A9_A10_A11/build
cmake --build A9_A10_A11/build
ctest --test-dir A9_A10_A11/build --output-on-failure
```

模块通过 `roll.h`、`GameState` 和 `MoveResult` 对接 A8。调用方须在执行 A8 Roll 前保存移动玩家下标，并在 A8 成功后调用 `property_after_move`。
