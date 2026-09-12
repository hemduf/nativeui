cmake_minimum_required(VERSION 3.24)
project(NativeUI_T045_Semantics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

function(nativeui_add_semantics_test target source)
  add_executable(${target} ${source})
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}/..
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  add_test(NAME ${target} COMMAND ${target})
endfunction()

enable_testing()
nativeui_add_semantics_test(t045_semantics ../t045_semantics_tests.cpp)
nativeui_add_semantics_test(t068_semantic_proxy t068_semantic_proxy_tests.cpp)
nativeui_add_semantics_test(t068_semantic_diff_regression t068_semantic_diff_regression_tests.cpp)
nativeui_add_semantics_test(t068_semantic_action_router t068_semantic_action_router_tests.cpp)
nativeui_add_semantics_test(t068_semantic_read_only t068_semantic_read_only_tests.cpp)
target_sources(t068_semantic_action_router PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp
)
target_link_libraries(t068_semantic_action_router PRIVATE Threads::Threads)
