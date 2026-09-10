cmake_minimum_required(VERSION 3.24)
project(NativeUI_T067_VirtualList LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(t067_add_contract name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
  )
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

enable_testing()
t067_add_contract(t067_virtual_list ../t067_virtual_list_contract.cpp)
t067_add_contract(t067_virtual_list_window ../t067_virtual_list_window_contract.cpp)
