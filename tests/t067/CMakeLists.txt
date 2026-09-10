cmake_minimum_required(VERSION 3.24)
project(NativeUI_T067_VirtualList LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(t067_virtual_list ../t067_virtual_list_contract.cpp)
target_include_directories(t067_virtual_list PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../include
)

if(MSVC)
  target_compile_options(t067_virtual_list PRIVATE /W4 /WX /permissive-)
else()
  target_compile_options(t067_virtual_list PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME t067_virtual_list COMMAND t067_virtual_list)
