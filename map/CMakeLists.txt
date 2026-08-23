cmake_minimum_required(VERSION 3.16)

project(rich_map LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(rich_map
    src/map.c
    src/game_interfaces.c
)
target_include_directories(rich_map PUBLIC include)

add_executable(rich src/main.c)
target_link_libraries(rich PRIVATE rich_map)

add_executable(map_tests tests/map_tests.c)
target_link_libraries(map_tests PRIVATE rich_map)

enable_testing()
add_test(NAME map_tests COMMAND map_tests)
