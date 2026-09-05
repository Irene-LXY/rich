cmake_minimum_required(VERSION 3.16)
project(rich_automation LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(run_tests c/run_tests.c)
add_executable(run_interactive_tests interactive/run_interactive_tests.c)

# MinGW 默认可能使用 MSVCRT 的 printf 解析器。仅在 MinGW 下启用
# ANSI stdio，使 <inttypes.h> 的 PRI* 宏与实际运行库保持一致。
if(MINGW)
    target_compile_definitions(run_tests PRIVATE __USE_MINGW_ANSI_STDIO=1)
    target_compile_definitions(run_interactive_tests PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()
