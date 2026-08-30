# A3 Default Money and A21 Tests Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make an empty initial-money input select 10000 yuan and provide a one-command automated suite covering every Excel A21 case ID.

**Architecture:** Keep the A3 fix at the command/startup boundary: only the initial-money setup state may forward an empty line. Build A21 as a focused C executable using the existing public property, gift, and turn-manager APIs, with data-driven fixtures for repeated bankruptcy variants and one result line per Excel case ID. Register both tests in CTest and wrap the A21 target in a PowerShell runner.

**Tech Stack:** C11, CMake/CTest, PowerShell 7 or Windows PowerShell, Ninja or the default CMake generator.

---

### Task 1: Add the failing A3 regression test

**Files:**
- Create: `tests/test_a3.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create a real command-path test that calls `application_start`, submits role combination `12`, then calls `command_execute(&game, "", ...)`. Assert `COMMAND_OK`, `game.runtime != NULL`, both player balances equal `10000`, and the message does not contain `命令不能为空`.

- [ ] **Step 2: Register the test**

```cmake
add_executable(test_a3 tests/test_a3.c)
target_link_libraries(test_a3 PRIVATE monopoly_core)
add_test(NAME A3_default_initial_money COMMAND test_a3)
```

- [ ] **Step 3: Verify RED**

Run:

```powershell
cmake -S . -B build-test -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target test_a3
ctest --test-dir build-test -R A3_default_initial_money --output-on-failure
```

Expected: the assertion for `COMMAND_OK` fails because the current result is `COMMAND_INVALID` with `命令不能为空`.

### Task 2: Implement the minimal A3 fix

**Files:**
- Modify: `src/commands/command_dispatcher.c`
- Test: `tests/test_a3.c`

- [ ] **Step 1: Forward empty input only during initial-money setup**

Before emitting the general empty-command error, add the narrow condition:

```c
if (*text == '\0' && game->runtime == NULL &&
    game->setup_step == SETUP_INITIAL_MONEY) {
    return startup_handle_input(game, text, message, message_size);
}
```

- [ ] **Step 2: Make startup select the documented default**

In `SETUP_INITIAL_MONEY`, use `10000` when `input[0] == '\0'`; otherwise retain strict integer parsing and the `1000~50000` range.

- [ ] **Step 3: Verify GREEN and no empty-command regression**

Run the A3 test, then `ctest --test-dir build-test -R "A3_default_initial_money|A20_quit" --output-on-failure`. Expected: both tests pass; A20 still rejects blank normal commands.

### Task 3: Add the A21 case harness

**Files:**
- Create: `tests/test_a21.c`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create deterministic fixtures**

Define a fixture containing `GameMap`, four money balances, and `PropertySystem`. Add helpers to initialize it, assign land, execute a toll, purchase/upgrade, sell, and verify released land. Define a `record_case("Case_A21_NNN", condition)` function that prints one PASS/FAIL line.

- [ ] **Step 2: Encode cases 001–033**

Use data tables for repeated toll bankruptcy cases and explicit tests for purchase/upgrade rejection, gift bonus, mine/no-money-change behavior, toll exemptions, sale, exact-zero boundaries, fifth/sixth fortune-turn boundaries, and A4 player removal/game-over behavior. Every ID from `Case_A21_001` through `Case_A21_033` must be passed exactly once to `record_case`.

- [ ] **Step 3: Register the executable**

```cmake
add_executable(test_a21 tests/test_a21.c)
target_link_libraries(test_a21 PRIVATE monopoly_core)
add_test(NAME A21_bankruptcy COMMAND test_a21)
```

- [ ] **Step 4: Run the new test and inspect failures**

Run `cmake --build build-test --target test_a21` and `ctest --test-dir build-test -R A21_bankruptcy --output-on-failure`. Any case failure must be traced to the production module before changing code.

### Task 4: Fix only A21 behaviors proven failing

**Files:**
- Modify only the production files identified by failing `Case_A21_*` assertions
- Test: `tests/test_a21.c`

- [ ] **Step 1: State one root-cause hypothesis per failure group**

For example, if bankruptcy leaves inventory behind, identify the exact transition that releases property and marks the player out but omits inventory clearing.

- [ ] **Step 2: Apply the smallest production change**

Do not alter unrelated command, map, property price, or story behavior. Re-run `A21_bankruptcy` after each failure group until all case IDs pass.

- [ ] **Step 3: Run related suites**

Run `ctest --test-dir build-test -R "A9_A10_A11_property|A15_gift_shop|A4_turn_manager|A4_runtime_workbook_regressions|A21_bankruptcy" --output-on-failure`.

### Task 5: Add the one-command PowerShell runner

**Files:**
- Create: `scripts/run_a21_tests.ps1`

- [ ] **Step 1: Implement repository-relative execution**

The script resolves the project root from `$PSScriptRoot`, chooses Ninja when available, configures `build-a21`, builds target `test_a21`, runs `ctest -R '^A21_bankruptcy$' --output-on-failure`, and propagates every failing native exit code.

- [ ] **Step 2: Verify from outside the scripts directory**

Run `powershell -ExecutionPolicy Bypass -File .\scripts\run_a21_tests.ps1`. Expected: 1/1 A21 CTest test passes and the script exits 0.

### Task 6: Final verification and usage documentation

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Document usage**

Add the command above, expected PASS summary, prerequisites, and the direct CMake/CTest fallback.

- [ ] **Step 2: Run full verification**

```powershell
cmake --build build-test
ctest --test-dir build-test --output-on-failure
powershell -ExecutionPolicy Bypass -File .\scripts\run_a21_tests.ps1
```

Expected: build exits 0, complete CTest reports 0 failures, and the wrapper exits 0.

- [ ] **Step 3: Review the diff**

Run `git diff --check` and `git status --short`. Confirm only the A3 fix, A3/A21 tests, runner, README, design, and plan files changed. Commit steps remain blocked until the repository has a configured author identity.
