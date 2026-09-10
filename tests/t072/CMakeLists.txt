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

add_executable(t072_build_contract build_contract.cpp)
target_include_directories(t072_build_contract PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/../../src"
)
target_link_libraries(t072_build_contract PRIVATE nativeui_linux_dbus)

if(MSVC)
  target_compile_options(t072_build_contract PRIVATE /W4 /permissive-)
else()
  target_compile_options(t072_build_contract PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME t072_build_contract COMMAND t072_build_contract)
