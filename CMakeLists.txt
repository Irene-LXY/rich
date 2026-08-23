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
    A7_A18/query_command.c
    A7_A18/step_command.c
    A7_A18/query_format.c
    src/runtime/runtime.c
    A4/src/a4_turn_manager.c
    map/src/map.c
    map/src/game_interfaces.c
)
target_include_directories(monopoly_core PUBLIC
    include
    A7_A18
    A4/include
    map/include
)

add_executable(monopoly src/main.c)
target_link_libraries(monopoly PRIVATE monopoly_core)

enable_testing()
add_executable(test_a20 tests/test_a20.c)
target_link_libraries(test_a20 PRIVATE monopoly_core)
add_test(NAME A20_quit COMMAND test_a20)

add_executable(test_a1 tests/test_a1.c)
target_link_libraries(test_a1 PRIVATE monopoly_core)
add_test(NAME A1_startup COMMAND test_a1)

add_executable(test_a7_a18 tests/test_a7_a18.c)
target_link_libraries(test_a7_a18 PRIVATE monopoly_core)
add_test(NAME A7_query_A18_step COMMAND test_a7_a18)
