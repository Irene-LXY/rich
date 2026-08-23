cmake_minimum_required(VERSION 3.16)

project(rich_map LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rich_map
    src/map.cpp
    src/game_interfaces.cpp
)
target_include_directories(rich_map PUBLIC include)

add_executable(rich src/main.cpp)
target_link_libraries(rich PRIVATE rich_map)

add_executable(map_tests tests/map_tests.cpp)
target_link_libraries(map_tests PRIVATE rich_map)

enable_testing()
add_test(NAME map_tests COMMAND map_tests)
