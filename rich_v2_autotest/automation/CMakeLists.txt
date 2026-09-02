cmake_minimum_required(VERSION 3.16)
project(rich_automation LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(run_tests c/run_tests.c)
add_executable(run_interactive_tests interactive/run_interactive_tests.c)
