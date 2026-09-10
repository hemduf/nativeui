cmake_minimum_required(VERSION 3.24)
project(NativeUIT072Contract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(t072_core_stub INTERFACE)
add_library(NativeUI::Core ALIAS t072_core_stub)
target_include_directories(t072_core_stub INTERFACE
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/NativeUILinuxDbus.cmake")
nativeui_add_linux_dbus_transport()

function(t072_add_test name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
  )
  target_link_libraries(${name} PRIVATE nativeui_linux_dbus)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

t072_add_test(t072_build_contract build_contract.cpp)
t072_add_test(t072_validation_contract validation_contract.cpp)
