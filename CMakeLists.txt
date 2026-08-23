cmake_minimum_required(VERSION 3.16)
project(monopoly LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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
)
target_include_directories(monopoly_core PUBLIC include)

add_executable(monopoly src/main.c)
target_link_libraries(monopoly PRIVATE monopoly_core)

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

add_executable(test_map tests/test_map.c)
target_link_libraries(test_map PRIVATE monopoly_core)
add_test(NAME A5_map COMMAND test_map)
