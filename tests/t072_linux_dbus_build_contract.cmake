cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_module "${SOURCE_DIR}/cmake/NativeUILinuxDbus.cmake")
set(_header "${SOURCE_DIR}/src/detail/linux_dbus.hpp")
set(_source "${SOURCE_DIR}/src/linux_dbus.cpp")

foreach(_required IN ITEMS "${_module}" "${_header}" "${_source}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "T072 RED: missing required internal Linux D-Bus transport file: ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${_module}" _dbus_module)

# The root owns only the Linux platform gate and module invocation; the module
# owns the private target/source details so those do not leak into unrelated
# platform configuration.
foreach(_needle IN ITEMS
    "NativeUILinuxDbus.cmake"
    "nativeui_add_linux_dbus_transport")
  string(FIND "${_root_cmake}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 contract missing root integration token: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "nativeui_linux_dbus"
    "src/linux_dbus.cpp"
    "dbus-1"
    "PkgConfig")
  string(FIND "${_dbus_module}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 contract missing Linux D-Bus module token: ${_needle}")
  endif()
endforeach()

file(GLOB_RECURSE _public_headers "${SOURCE_DIR}/include/nativeui/*.hpp")
foreach(_public_header IN LISTS _public_headers)
  file(READ "${_public_header}" _public_text)
  string(FIND "${_public_text}" "dbus/dbus.h" _leak)
  if(NOT _leak EQUAL -1)
    message(FATAL_ERROR "T072 public API leak: ${_public_header} includes dbus/dbus.h")
  endif()
endforeach()

message(STATUS "T072 Linux D-Bus build contract satisfied")
