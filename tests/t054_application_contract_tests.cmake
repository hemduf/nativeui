if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T054 application contract requires SOURCE_DIR")
endif()
file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR_CMAKE)

if(NOT EXISTS "${SOURCE_DIR}/cmake/NativeUIApplication.cmake")
  message(FATAL_ERROR "T054 RED: cmake/NativeUIApplication.cmake does not exist")
endif()

set(_root "${CMAKE_CURRENT_BINARY_DIR}/t054-application-contract")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

set(_generator_args)
if(WIN32)
  find_program(_ninja NAMES ninja ninja-build)
  if(_ninja)
    list(APPEND _generator_args -G Ninja)
  endif()
endif()

function(_t054_run_case name expect_success body)
  set(_src "${_root}/${name}-src")
  set(_build "${_root}/${name}-build")
  file(MAKE_DIRECTORY "${_src}")
  file(WRITE "${_src}/main.cpp" "int main() { return 0; }\n")
  file(WRITE "${_src}/extra.cpp" "int t054_extra() { return 54; }\n")
  file(WRITE "${_src}/UPPER.CPP" "int t054_upper() { return 54; }\n")
  file(WRITE "${_src}/icon.icns" "t054 deterministic icns fixture\n")
  file(WRITE "${_src}/icon.ico" "t054 deterministic ico fixture\n")
  file(WRITE "${_src}/wrong.txt" "wrong icon extension\n")

  set(_project [=[
cmake_minimum_required(VERSION 3.24)
project(T054ApplicationContract LANGUAGES CXX)
add_library(nativeui_core_stub INTERFACE)
add_library(NativeUI::Core ALIAS nativeui_core_stub)

# Keep the contract fixture focused on T054 while exercising the real T047
# public validation wrapper. Stub only T053's private platform implementation so
# configure-only cases do not pull native SDK dependencies.
function(_nativeui_attach_consumer_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID;OUT_BRIDGE" "")
  set_property(TARGET "${NUI_TARGET}" PROPERTY NATIVEUI_CONSUMER_ID "${NUI_CONSUMER_ID}")
endfunction()
include("@SOURCE_DIR_CMAKE@/cmake/NativeUIAttachPlatform.cmake")
include("@SOURCE_DIR_CMAKE@/cmake/NativeUIApplication.cmake")
@CASE_BODY@
]=])
  set(CASE_BODY "${body}")
  string(CONFIGURE "${_project}" _configured @ONLY)
  file(WRITE "${_src}/CMakeLists.txt" "${_configured}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}" ${_generator_args}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  set(_combined "${_stdout}\n${_stderr}")
  string(REGEX REPLACE "[ \t\r\n]+" " " _normalized "${_combined}")

  if(expect_success)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "T054 ${name} unexpectedly failed (${_result})\n${_combined}")
    endif()
  else()
    if(_result EQUAL 0)
      message(FATAL_ERROR "T054 ${name} unexpectedly succeeded")
    endif()
    foreach(_expected IN LISTS ARGN)
      string(FIND "${_normalized}" "${_expected}" _found)
      if(_found EQUAL -1)
        message(FATAL_ERROR
          "T054 ${name} failed for the wrong reason\n"
          "missing diagnostic fragment: ${_expected}\noutput:\n${_combined}")
      endif()
    endforeach()
  endif()
endfunction()

set(_valid_body [=[
nativeui_add_application(App
  PRODUCT_NAME "NativeUI Café"
  BUNDLE_ID com.example.nativeui-test
  VERSION 001.02.0003
  SOURCES main.cpp)
get_target_property(_output App OUTPUT_NAME)
get_target_property(_consumer App NATIVEUI_CONSUMER_ID)
get_target_property(_type App TYPE)
if(NOT _output STREQUAL "NativeUI Café")
  message(FATAL_ERROR "PRODUCT_NAME was not preserved exactly: '${_output}'")
endif()
if(NOT _consumer STREQUAL "com.example.nativeui-test")
  message(FATAL_ERROR "BUNDLE_ID was not delegated verbatim: '${_consumer}'")
endif()
if(NOT _type STREQUAL "EXECUTABLE")
  message(FATAL_ERROR "T054 helper did not create an executable: '${_type}'")
endif()
if(APPLE)
  get_target_property(_bundle App MACOSX_BUNDLE)
  if(NOT _bundle)
    message(FATAL_ERROR "T054 macOS target is not a MACOSX_BUNDLE")
  endif()
elseif(WIN32)
  get_target_property(_gui App WIN32_EXECUTABLE)
  if(NOT _gui)
    message(FATAL_ERROR "T054 Windows target is not a WIN32 executable")
  endif()
else()
  get_target_property(_bundle App MACOSX_BUNDLE)
  get_target_property(_gui App WIN32_EXECUTABLE)
  if(_bundle OR _gui)
    message(FATAL_ERROR "T054 Linux/other target gained bundle/GUI subsystem side effects")
  endif()
endif()
# Caller remains free to extend the target after helper invocation.
target_sources(App PRIVATE extra.cpp)
target_compile_definitions(App PRIVATE T054_CALLER_EXTENSION=1)
]=])
_t054_run_case(valid TRUE "${_valid_body}")

# Semicolon is a valid portable filename character and therefore part of the
# frozen PRODUCT_NAME acceptance surface. This regression guards the original
# one-value CMake argument boundary rather than reparsing a flattened list.
_t054_run_case(product_semicolon TRUE [=[
nativeui_add_application(App
  PRODUCT_NAME "Semi;Colon"
  BUNDLE_ID com.example.semicolon
  VERSION 1.2.3
  SOURCES main.cpp)
get_target_property(_output App OUTPUT_NAME)
if(NOT _output STREQUAL "Semi;Colon")
  message(FATAL_ERROR "semicolon PRODUCT_NAME was not preserved: '${_output}'")
endif()
]=])

_t054_run_case(reordered_keywords TRUE [=[
nativeui_add_application(App
  SOURCES main.cpp
  VERSION 1.2.3
  PRODUCT_NAME "Reordered App"
  BUNDLE_ID com.example.reordered)
]=])

# SOURCES follows normal CMake target-source semantics. Uppercase caller-owned
# source names are data, not helper syntax.
_t054_run_case(uppercase_source TRUE [=[
nativeui_add_application(App
  PRODUCT_NAME App
  BUNDLE_ID com.example.uppercase-source
  VERSION 1.2.3
  SOURCES main.cpp UPPER.CPP)
]=])

# Non-consuming platform icon keywords must be accepted without touching the
# path, so one portable call can mention both platform assets.
if(APPLE)
  set(_ignored_keyword WINDOWS_ICON)
  set(_consumed_keyword MACOS_ICON)
  set(_good_extension icon.icns)
elseif(WIN32)
  set(_ignored_keyword MACOS_ICON)
  set(_consumed_keyword WINDOWS_ICON)
  set(_good_extension icon.ico)
else()
  set(_ignored_keyword MACOS_ICON)
  set(_consumed_keyword WINDOWS_ICON)
  set(_good_extension does-not-exist.ico)
endif()
_t054_run_case(icon_nonconsuming_missing TRUE
  "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.icon VERSION 1.2.3 SOURCES main.cpp ${_ignored_keyword} does-not-exist.invalid)")
_t054_run_case(icon_consuming_valid TRUE
  "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.icon VERSION 1.2.3 SOURCES main.cpp ${_consumed_keyword} ${_good_extension})")
if(APPLE OR WIN32)
  _t054_run_case(icon_consuming_missing FALSE
    "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.icon VERSION 1.2.3 SOURCES main.cpp ${_consumed_keyword} missing.invalid)"
    "does not exist")
  _t054_run_case(icon_consuming_wrong_extension FALSE
    "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.icon VERSION 1.2.3 SOURCES main.cpp ${_consumed_keyword} wrong.txt)"
    "must use the")
endif()

_t054_run_case(missing_target FALSE
  "nativeui_add_application()" "target name is required")
_t054_run_case(duplicate_target FALSE
  "add_executable(App main.cpp)\nnativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.app VERSION 1.2.3 SOURCES main.cpp)"
  "already exists")
_t054_run_case(missing_product FALSE
  "nativeui_add_application(App BUNDLE_ID com.example.app VERSION 1.2.3 SOURCES main.cpp)"
  "PRODUCT_NAME" "required")
_t054_run_case(missing_bundle FALSE
  "nativeui_add_application(App PRODUCT_NAME App VERSION 1.2.3 SOURCES main.cpp)"
  "BUNDLE_ID" "required")
_t054_run_case(missing_version FALSE
  "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.app SOURCES main.cpp)"
  "VERSION" "MAJOR.MINOR.PATCH")
_t054_run_case(missing_sources FALSE
  "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.app VERSION 1.2.3)"
  "SOURCES" "at least one")
_t054_run_case(unknown_keyword FALSE
  "nativeui_add_application(App PRODUCT_NAME App BUNDLE_ID com.example.app VERSION 1.2.3 SOURCES main.cpp ICON icon.ico)"
  "unknown" "ICON")

# Validation matrices run in script mode so dozens of fatal cases stay isolated
# without repeatedly detecting a compiler. Values are embedded with bracket
# quoting rather than -D cache arguments so trailing spaces, semicolons and
# backslashes are preserved exactly.
function(_t054_validation_case mode name value expect_success)
  set(_case_script "${_root}/validation_${mode}_${name}.cmake")
  file(WRITE "${_case_script}"
    "set(SOURCE_DIR [==[${SOURCE_DIR}]==])\n"
    "set(MODE [==[${mode}]==])\n"
    "set(VALUE [==[${value}]==])\n"
    [=[
include("${SOURCE_DIR}/cmake/NativeUIApplication.cmake")
if(MODE STREQUAL "product")
  _nativeui_validate_application_product_name("${VALUE}")
elseif(MODE STREQUAL "version")
  _nativeui_validate_application_version("${VALUE}")
elseif(MODE STREQUAL "bundle")
  function(_nativeui_attach_consumer_platform)
  endfunction()
  include("${SOURCE_DIR}/cmake/NativeUIAttachPlatform.cmake")
  _nativeui_validate_consumer_id("${VALUE}")
else()
  message(FATAL_ERROR "unknown validation mode")
endif()
]=])
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -P "${_case_script}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  if(expect_success)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "T054 ${mode}/${name} unexpectedly failed\n${_stdout}\n${_stderr}")
    endif()
  elseif(_result EQUAL 0)
    message(FATAL_ERROR "T054 ${mode}/${name} unexpectedly succeeded")
  endif()
endfunction()

foreach(_value IN ITEMS "NativeUI Café" "Semi;Colon" "A B" "名前")
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(product "valid_${_case}" "${_value}" TRUE)
endforeach()
foreach(_value IN ITEMS "." ".." "Bad/Name" "Bad\\Name" "Bad<Name" "Bad>Name" "Bad:Name" "Bad\"Name" "Bad|Name" "Bad?Name" "Bad*Name" "Bad " "Bad." CON con.txt PrN aux.log NUL COM1 com9.bin LPT1 lpt9.out)
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(product "invalid_${_case}" "${_value}" FALSE)
endforeach()

foreach(_value IN ITEMS com.example.app org.nativeui.product-one io.example.Shared2)
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(bundle "valid_${_case}" "${_value}" TRUE)
endforeach()
foreach(_value IN ITEMS example com_example.app "com.example_bad.app" "com.example bad" "com.example/app" "com.example:app" "com..app" "com.-example.app" "com.example-.app")
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(bundle "invalid_${_case}" "${_value}" FALSE)
endforeach()

foreach(_value IN ITEMS 1.2.3 001.02.0003 999999999999999999999.0.1)
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(version "valid_${_case}" "${_value}" TRUE)
endforeach()
foreach(_value IN ITEMS v1.2.3 1.2 1.2.3.4 1.2.3-beta 1.2.3+meta)
  string(MAKE_C_IDENTIFIER "${_value}" _case)
  _t054_validation_case(version "invalid_${_case}" "${_value}" FALSE)
endforeach()

message(STATUS "T054 configure/public-helper contract passed")