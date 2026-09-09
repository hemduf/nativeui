cmake_minimum_required(VERSION 3.24)
project(NativeUIT065Contract LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM OFF CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." nativeui-build)
target_sources(nativeui_core PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp")

add_executable(t065_dispatcher_contract ../dispatcher_tests.cpp)
target_include_directories(t065_dispatcher_contract PRIVATE "${CMAKE_CURRENT_LIST_DIR}/..")
target_link_libraries(t065_dispatcher_contract PRIVATE NativeUI::Core)

if(MSVC)
  target_compile_options(t065_dispatcher_contract PRIVATE /W4 /permissive-)
else()
  target_compile_options(t065_dispatcher_contract PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME t065_dispatcher_contract COMMAND t065_dispatcher_contract)
