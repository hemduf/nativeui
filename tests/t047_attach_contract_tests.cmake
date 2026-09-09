if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T047 attach contract tests require SOURCE_DIR")
endif()

if(NOT EXISTS "${SOURCE_DIR}/cmake/NativeUIAttachPlatform.cmake")
  message(FATAL_ERROR
    "T047 RED: public NativeUIAttachPlatform.cmake does not exist yet")
endif()

set(_root "${CMAKE_CURRENT_BINARY_DIR}/t047-attach-contract")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

function(_t047_run_case case expect_success body)
  set(_src "${_root}/${case}-src")
  set(_build "${_root}/${case}-build")
  file(MAKE_DIRECTORY "${_src}")
  file(WRITE "${_src}/main.cpp" "int main() { return 0; }\n")
  file(WRITE "${_src}/module.cpp" "extern \"C\" int t047_fixture() { return 47; }\n")

  set(_project [=[
cmake_minimum_required(VERSION 3.24)
project(T047AttachContract LANGUAGES CXX)
include("@SOURCE_DIR@/cmake/NativeUIConsumerPlatform.cmake")
include("@SOURCE_DIR@/cmake/NativeUIAttachPlatform.cmake")
add_library(nativeui_core_stub INTERFACE)
add_library(NativeUI::Core ALIAS nativeui_core_stub)

# Keep this fixture focused on T047 validation. The production helper delegates
# to T053 after validation; this stub records the same target property that the
# T053 implementation publishes without requiring platform dependencies.
function(_nativeui_attach_consumer_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID;OUT_BRIDGE" "")
  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_CONSUMER_ID "${NUI_CONSUMER_ID}")
endfunction()

@CASE_BODY@
]=])
  set(CASE_BODY "${body}")
  string(CONFIGURE "${_project}" _configured @ONLY)
  file(WRITE "${_src}/CMakeLists.txt" "${_configured}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  set(_combined "${_stdout}\n${_stderr}")
  string(REGEX REPLACE "[ \t\r\n]+" " " _normalized "${_combined}")

  if(expect_success)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR
        "T047 ${case} unexpectedly failed (${_result})\n${_combined}")
    endif()
  else()
    if(_result EQUAL 0)
      message(FATAL_ERROR "T047 ${case} unexpectedly succeeded")
    endif()
    foreach(_expected IN LISTS ARGN)
      string(FIND "${_normalized}" "${_expected}" _found)
      if(_found EQUAL -1)
        message(FATAL_ERROR
          "T047 ${case} failed for the wrong reason\n"
          "missing diagnostic fragment: ${_expected}\noutput:\n${_combined}")
      endif()
    endforeach()
  endif()
endfunction()

_t047_run_case(valid_executable TRUE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.app)")
_t047_run_case(valid_module TRUE
  "add_library(app MODULE module.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID org.example.plugin-one)")
_t047_run_case(valid_shared TRUE
  "add_library(app SHARED module.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID io.example.Shared2)")

_t047_run_case(missing_target FALSE
  "nativeui_attach_platform(TARGET missing CONSUMER_ID com.example.missing)"
  "target does not exist" "missing")
_t047_run_case(static_target FALSE
  "add_library(app STATIC module.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.static)"
  "app" "STATIC_LIBRARY" "final target")
_t047_run_case(object_target FALSE
  "add_library(app OBJECT module.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.object)"
  "app" "OBJECT_LIBRARY" "final target")
_t047_run_case(interface_target FALSE
  "add_library(app INTERFACE)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.interface)"
  "app" "INTERFACE_LIBRARY" "final target")
_t047_run_case(imported_alias FALSE
  "add_library(external SHARED IMPORTED)\nadd_library(app ALIAS external)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.alias)"
  "app" "alias" "final target")

_t047_run_case(empty_id FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID \"\")"
  "CONSUMER_ID" "required")
_t047_run_case(single_segment FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID example)"
  "CONSUMER_ID 'example'" "reverse-DNS")
_t047_run_case(underscore FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example_bad.app)"
  "CONSUMER_ID 'com.example_bad.app'" "reverse-DNS")
_t047_run_case(whitespace FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID \"com.example bad\")"
  "reverse-DNS")
_t047_run_case(slash FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example/app)"
  "reverse-DNS")
_t047_run_case(colon FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example:app)"
  "reverse-DNS")
_t047_run_case(empty_segment FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com..app)"
  "reverse-DNS")
_t047_run_case(leading_hyphen FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.-example.app)"
  "reverse-DNS")
_t047_run_case(trailing_hyphen FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example-.app)"
  "reverse-DNS")

_t047_run_case(double_attach FALSE
  "add_executable(app main.cpp)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.first)\nnativeui_attach_platform(TARGET app CONSUMER_ID com.example.first)"
  "target 'app'" "already attached" "com.example.first")

message(STATUS "T047 final-target/identity/double-attach contract passed")
