# 迭代一：自动化测试环境使用说明

## 本环境包含什么

- `src/test_runner.c`：C 语言测试运行器，读 JSON、调游戏引擎、比较 `actual` 和 `expected`；
- `src/game_engine.h`：测试环境与游戏代码之间的统一接口；
- `src/game_engine_stub.c`：TDD 红阶段占位实现；
- `testcases/iteration1/`：六个 Story 的 JSON 测试用例；
- `run_tests.ps1` / `run_tests.bat`：一键运行全部 JSON 用例；
- `iteration1_imported_cases.json`：从第一组统一测试用例中筛选出的迭代一用例清单；
- `ci/`：GitHub Actions 和 GitLab CI 模板；
- `COMMUNICATION.md`：需要与开发人员沟通的接口清单。

## 本地运行步骤

1. 进入本目录；
2. 双击或运行 `build_cl.bat`，编译出 `rich_test.exe`；
3. 运行 `run_tests.bat`，自动执行 `testcases/iteration1/` 下所有 JSON 用例。

当前开发还没完成，所以六个用例会显示 `ERROR`（错误码 `NOT_IMPLEMENTED`），这是 TDD 红阶段的正常结果。

## 六个 Story 对应文件

| Story | 文件 |
| --- | --- |
| 1 启动游戏 | `story01_start_game.json` |
| 2 选择玩家 | `story02_select_players.json` |
| 3 依次进行回合 | `story03_turn_sequence.json` |
| 4 显示地图 | `story04_map_display.json` |
| 5 掷骰子移动 | `story05_roll_move.json` |
| 6 Quit 退出 | `story06_quit.json` |

## 交给开发人员

把 [COMMUNICATION.md](COMMUNICATION.md) 发给开发人员，重点确认：

- `game_engine_execute` 函数签名；
- `map.json` 内容和格式；
- JSON 字段名与 `schema_version`；
- Story 1、2、4 的 JSON 命令扩展方式；
- QUIT 后的 `phase` 和 `game_status`。

## 接入 CI

如果使用 GitHub，把 `ci/github-actions-ci.yml` 放到仓库 `.github/workflows/`。
如果使用 GitLab，把 `ci/gitlab-ci.yml` 放到仓库根目录并改名为 `.gitlab-ci.yml`。

CI 每次提交都会自动编译并运行这些测试。
