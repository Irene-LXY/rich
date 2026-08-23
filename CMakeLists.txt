cmake_minimum_required(VERSION 3.16)
project(rich_a4_turn_manager VERSION 1.0.0 LANGUAGES C)

add_library(a4_turn_manager STATIC
    src/a4_turn_manager.c
)
target_include_directories(a4_turn_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(a4_turn_manager PUBLIC c_std_11)

if(MSVC)
    target_compile_options(a4_turn_manager PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(a4_turn_manager PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(a4_demo examples/demo.c)
target_link_libraries(a4_demo PRIVATE a4_turn_manager)

enable_testing()
add_executable(a4_turn_manager_tests tests/test_a4_turn_manager.c)
target_link_libraries(a4_turn_manager_tests PRIVATE a4_turn_manager)
add_test(NAME a4_turn_manager_tests COMMAND a4_turn_manager_tests)

if(MSVC)
    target_compile_options(a4_demo PRIVATE /W4 /utf-8 /permissive-)
    target_compile_options(a4_turn_manager_tests PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(a4_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(a4_turn_manager_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
