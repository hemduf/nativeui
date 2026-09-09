if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T054 argument-boundary regression requires SOURCE_DIR")
endif()
file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR_CMAKE)

set(_root "${CMAKE_CURRENT_BINARY_DIR}/t054-argument-boundary")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

set(_generator_args)
if(WIN32)
  find_program(_ninja NAMES ninja ninja-build)
  if(_ninja)
    list(APPEND _generator_args -G Ninja)
  endif()
endif()

function(_t054_argument_case name product_expression expected_output)
  set(_src "${_root}/${name}-src")
  set(_build "${_root}/${name}-build")
  file(MAKE_DIRECTORY "${_src}")
  file(WRITE "${_src}/main.c" "int main(void) { return 0; }\n")

  set(_project [=[
cmake_minimum_required(VERSION 3.24)
project(T054ArgumentBoundary LANGUAGES NONE)
add_library(nativeui_core_stub INTERFACE)
add_library(NativeUI::Core ALIAS nativeui_core_stub)
function(_nativeui_attach_consumer_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID;OUT_BRIDGE" "")
endfunction()
include("@SOURCE_DIR_CMAKE@/cmake/NativeUIAttachPlatform.cmake")
include("@SOURCE_DIR_CMAKE@/cmake/NativeUIApplication.cmake")
nativeui_add_application(App
  PRODUCT_NAME @PRODUCT_EXPRESSION@
  BUNDLE_ID com.example.argument-boundary
  VERSION 1.2.3
  SOURCES main.c)
get_target_property(_output App OUTPUT_NAME)
if(NOT _output STREQUAL [==[@EXPECTED_OUTPUT@]==])
  message(FATAL_ERROR "PRODUCT_NAME argument boundary changed: '${_output}'")
endif()
]=])
  set(PRODUCT_EXPRESSION "${product_expression}")
  set(EXPECTED_OUTPUT "${expected_output}")
  string(CONFIGURE "${_project}" _configured @ONLY)
  file(WRITE "${_src}/CMakeLists.txt" "${_configured}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}" ${_generator_args}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "T054 ${name} failed; semicolon-bearing PRODUCT_NAME must retain its value\n"
      "${_stdout}\n${_stderr}")
  endif()
endfunction()

# A normal quoted CMake argument containing semicolons is legal for the frozen
# PRODUCT_NAME filename grammar and must round-trip unchanged.
_t054_argument_case(quoted_semicolons
  [==["Alpha;Beta;Omega"]==] "Alpha;Beta;Omega")

# When a semicolon-delimited component itself equals a helper keyword, callers
# can use normal CMake list escaping to keep that semicolon inside the original
# macro value. The helper must remove only CMake's list escape and preserve the
# actual product filename bytes.
_t054_argument_case(escaped_keyword_component
  [==["Alpha\;VERSION\;Omega"]==] "Alpha;VERSION;Omega")

message(STATUS "T054 semicolon argument-boundary regression passed")
