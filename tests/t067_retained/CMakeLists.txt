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

add_executable(t067_virtual_list_retained
  "${NATIVEUI_SOURCE_DIR}/tests/t067_retained_tests.cpp"
)
target_include_directories(t067_virtual_list_retained PRIVATE
  "${NATIVEUI_SOURCE_DIR}/tests"
)
target_link_libraries(t067_virtual_list_retained PRIVATE NativeUI::Core)
target_compile_features(t067_virtual_list_retained PRIVATE cxx_std_20)
nativeui_enable_project_warnings(t067_virtual_list_retained)

if(MSVC)
  target_compile_options(t067_virtual_list_retained PRIVATE /WX)
else()
  target_compile_options(t067_virtual_list_retained PRIVATE -Werror)
endif()

enable_testing()
add_test(NAME t067_virtual_list_retained COMMAND t067_virtual_list_retained)
