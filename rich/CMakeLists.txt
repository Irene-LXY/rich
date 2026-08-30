cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
project(monopoly LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# rich.exe 不依赖额外的 MSVC 运行库 DLL，便于按说明书单文件启动。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_library(monopoly_core
    src/core/game.c
    src/startup/startup.c
    src/commands/command_dispatcher.c
    src/commands/quit_command.c
    src/commands/query_format.c
    src/runtime/runtime.c
    src/story/gift.c
    src/story/magic.c
    src/character.c
    src/a4/a4_turn_manager.c
    src/map/map.c
    src/map/game_interfaces.c
    src/property/property_system.c
)
target_include_directories(monopoly_core PUBLIC include)

add_executable(monopoly src/main.c)
target_link_libraries(monopoly PRIVATE monopoly_core)

# 说明书指定的正式启动文件名；保留 monopoly 以兼容既有脚本。
add_executable(rich src/main.c)
target_link_libraries(rich PRIVATE monopoly_core)

# 独立的 JSON 黑盒测试入口；不改变 rich.exe 的交互启动与业务行为。
add_executable(monopoly_test src/test_runner_main.c)
target_link_libraries(monopoly_test PRIVATE monopoly_core)

enable_testing()
add_executable(test_a15 tests/test_a15.c)
target_link_libraries(test_a15 PRIVATE monopoly_core)
add_test(NAME A15_gift_shop COMMAND test_a15)

add_executable(test_a16 tests/test_a16.c)
target_link_libraries(test_a16 PRIVATE monopoly_core)
add_test(NAME A16_magic_house COMMAND test_a16)

add_executable(test_a1 tests/test_a1.c)
target_link_libraries(test_a1 PRIVATE monopoly_core)
add_test(NAME A1_startup COMMAND test_a1)

add_executable(test_a20 tests/test_a20.c)
target_link_libraries(test_a20 PRIVATE monopoly_core)
add_test(NAME A20_quit COMMAND test_a20)

add_executable(test_a4 tests/test_a4_turn_manager.c)
target_link_libraries(test_a4 PRIVATE monopoly_core)
add_test(NAME A4_turn_manager COMMAND test_a4)

add_executable(test_a4_runtime tests/test_a4_runtime.c)
target_link_libraries(test_a4_runtime PRIVATE monopoly_core)
add_test(NAME A4_runtime_workbook_regressions COMMAND test_a4_runtime)

add_executable(test_map tests/test_map.c)
target_link_libraries(test_map PRIVATE monopoly_core)
add_test(NAME A5_map COMMAND test_map)

add_executable(test_property tests/test_property.c)
target_link_libraries(test_property PRIVATE monopoly_core)
add_test(NAME A9_A10_A11_property COMMAND test_property)
