cmake_minimum_required(VERSION 3.24)
project(NativeUI_T045_Semantics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(t045_semantics ../t045_semantics_tests.cpp)
target_include_directories(t045_semantics PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../include
  ${CMAKE_CURRENT_LIST_DIR}/..
)

if(MSVC)
  target_compile_options(t045_semantics PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(t045_semantics PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME t045_semantics COMMAND t045_semantics)
