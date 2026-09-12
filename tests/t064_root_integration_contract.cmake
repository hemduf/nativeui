cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${SOURCE_DIR}/include/nativeui/nativeui.hpp" _umbrella)
set(_header_fixture "${SOURCE_DIR}/tests/headers/desktop_services.cpp")

function(require_text haystack needle description)
  string(FIND "${haystack}" "${needle}" _index)
  if(_index EQUAL -1)
    message(FATAL_ERROR "T064 root integration contract: missing ${description}: ${needle}")
  endif()
endfunction()

require_text("${_root_cmake}" "src/desktop_services.cpp" "DesktopServices implementation in NativeUI::Core")
require_text("${_root_cmake}" "nativeui_t064_root_integration_contract" "root contract registration")
require_text("${_root_cmake}" "state dispatcher desktop_services text_edit" "isolated DesktopServices public-header compile coverage")
require_text("${_umbrella}" "#include <nativeui/desktop_services.hpp>" "DesktopServices umbrella export")

if(NOT EXISTS "${_header_fixture}")
  message(FATAL_ERROR "T064 root integration contract: missing isolated public-header fixture: ${_header_fixture}")
endif()

message(STATUS "T064 root DesktopServices integration contract passed")
