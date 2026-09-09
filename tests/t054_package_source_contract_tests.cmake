if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T054 package source contract requires SOURCE_DIR")
endif()

foreach(_required IN ITEMS
    cmake/NativeUIApplication.cmake
    cmake/NativeUIConfig.cmake.in
    cmake/NativeUIBuildTreeConfig.cmake.in
    tests/t054_application_contract_tests.cmake)
  if(NOT EXISTS "${SOURCE_DIR}/${_required}")
    message(FATAL_ERROR "T054 package source contract missing ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
string(REGEX MATCHALL "NativeUIApplication.cmake" _module_mentions "${_root_cmake}")
list(LENGTH _module_mentions _module_count)
if(_module_count LESS 2)
  message(FATAL_ERROR
    "T054 RED: CMakeLists.txt must install and copy NativeUIApplication.cmake into the build-tree package")
endif()

foreach(_config IN ITEMS cmake/NativeUIConfig.cmake.in cmake/NativeUIBuildTreeConfig.cmake.in)
  file(READ "${SOURCE_DIR}/${_config}" _config_text)
  string(FIND "${_config_text}" "NativeUIApplication.cmake" _application_include)
  if(_application_include EQUAL -1)
    message(FATAL_ERROR
      "T054 RED: ${_config} does not expose NativeUIApplication.cmake")
  endif()
endforeach()

message(STATUS "T054 package source exposure contract passed")
