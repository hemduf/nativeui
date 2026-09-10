cmake_minimum_required(VERSION 3.24)
project(NativeUI_T067_Retained LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(NATIVEUI_BUILD_PLATFORM OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)

function(t067_strict_target target)
  target_link_libraries(${target} PRIVATE NativeUI::Core)
  target_compile_features(${target} PRIVATE cxx_std_20)
  nativeui_enable_project_warnings(${target})
  if(MSVC)
    target_compile_options(${target} PRIVATE /WX)
  else()
    target_compile_options(${target} PRIVATE -Werror)
  endif()
endfunction()

function(t067_add_test target source)
  add_executable(${target} "${NATIVEUI_SOURCE_DIR}/tests/${source}")
  target_include_directories(${target} PRIVATE "${NATIVEUI_SOURCE_DIR}/tests")
  t067_strict_target(${target})
  add_test(NAME ${target} COMMAND ${target})
endfunction()

enable_testing()
t067_add_test(t067_virtual_list_retained t067_retained_tests.cpp)
t067_add_test(t067_public_api t067_public_api_tests.cpp)
t067_add_test(t067_semantic_api t067_semantic_api_tests.cpp)
t067_add_test(t067_visual t067_visual_tests.cpp)

add_library(t067_virtual_list_header_compile OBJECT
  "${NATIVEUI_SOURCE_DIR}/tests/headers/virtual_list.cpp"
)
t067_strict_target(t067_virtual_list_header_compile)

add_library(t067_virtual_list_example_compile OBJECT
  "${NATIVEUI_SOURCE_DIR}/examples/features/t067_virtual_list.cpp"
)
target_include_directories(t067_virtual_list_example_compile PRIVATE
  "${NATIVEUI_SOURCE_DIR}/examples/features"
)
t067_strict_target(t067_virtual_list_example_compile)
