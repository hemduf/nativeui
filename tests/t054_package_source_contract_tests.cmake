if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T054 package source contract requires SOURCE_DIR")
endif()

foreach(_required IN ITEMS
    cmake/NativeUIApplication.cmake
    cmake/NativeUIConfig.cmake.in
    cmake/NativeUIBuildTreeConfig.cmake.in
    cmake/NativeUIConsumerPlatform.cmake
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

# CMake explicitly forbids enable_language() from function scope. Platform
# attachment is implemented by functions, so every required compiler language
# must already be enabled by a file-scope package/config entry point before any
# helper creates C/Objective-C platform targets. This regression protects the
# CXX-only external-consumer contract exercised by t054_package_tests.cmake.
file(READ "${SOURCE_DIR}/cmake/NativeUIConsumerPlatform.cmake" _platform_module)
string(FIND "${_platform_module}" "enable_language(" _function_scope_enable)
if(NOT _function_scope_enable EQUAL -1)
  message(FATAL_ERROR
    "T054 RED: NativeUIConsumerPlatform.cmake must not call enable_language() from helper function scope")
endif()

foreach(_config IN ITEMS cmake/NativeUIConfig.cmake.in cmake/NativeUIBuildTreeConfig.cmake.in)
  file(READ "${SOURCE_DIR}/${_config}" _config_text)
  string(FIND "${_config_text}" "NativeUIApplication.cmake" _application_include)
  if(_application_include EQUAL -1)
    message(FATAL_ERROR
      "T054 RED: ${_config} does not expose NativeUIApplication.cmake")
  endif()
  foreach(_language IN ITEMS C OBJC)
    string(FIND "${_config_text}" "enable_language(${_language})" _language_enable)
    if(_language_enable EQUAL -1)
      message(FATAL_ERROR
        "T054 RED: ${_config} must enable ${_language} at package file scope before platform helpers run")
    endif()
  endforeach()
endforeach()

message(STATUS "T054 package source exposure/language-scope contract passed")
