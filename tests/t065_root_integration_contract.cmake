cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${SOURCE_DIR}/include/nativeui/nativeui.hpp" _umbrella)

function(require_text haystack needle description)
  string(FIND "${haystack}" "${needle}" _index)
  if(_index EQUAL -1)
    message(FATAL_ERROR "T065 root integration contract: missing ${description}: ${needle}")
  endif()
endfunction()

require_text("${_root_cmake}" "src/dispatcher.cpp" "dispatcher implementation in NativeUI::Core")
require_text("${_root_cmake}" "nativeui_add_core_test(nativeui_dispatcher_tests tests/dispatcher_tests.cpp)" "dispatcher core test registration")
require_text("${_root_cmake}" "nativeui_add_core_test(nativeui_dispatcher_edge_tests tests/dispatcher_edge_tests.cpp)" "dispatcher edge test registration")
require_text("${_root_cmake}" "foreach(_header IN ITEMS geometry constraints invalidation input gesture state dispatcher" "isolated dispatcher public-header compile coverage")
require_text("${_umbrella}" "#include <nativeui/dispatcher.hpp>" "dispatcher umbrella export")

message(STATUS "T065 root dispatcher integration contract passed")
