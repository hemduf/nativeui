if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T056 package source contract requires SOURCE_DIR")
endif()

foreach(_required IN ITEMS
    cmake/NativeUIBinaryData.cmake
    cmake/NativeUIEmbedResource.cmake
    cmake/NativeUIConfig.cmake.in
    cmake/NativeUIBuildTreeConfig.cmake.in
    include/nativeui/embedded_resource.hpp
    include/nativeui/nativeui.hpp
    tests/t056_package_tests.cmake
    tests/t056_external_consumer/CMakeLists.txt
    tests/t056_external_consumer/main.cpp
    tests/t056_external_consumer/header.cpp
    tests/t056_external_consumer/umbrella.cpp
    tests/t056_external_consumer/resources/message.txt
    tests/t056_external_consumer/resources/other.txt)
  if(NOT EXISTS "${SOURCE_DIR}/${_required}")
    message(FATAL_ERROR "T056 package source contract missing ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
foreach(_module IN ITEMS NativeUIBinaryData.cmake NativeUIEmbedResource.cmake)
  string(REGEX MATCHALL "${_module}" _module_mentions "${_root_cmake}")
  list(LENGTH _module_mentions _module_count)
  if(_module_count LESS 2)
    message(FATAL_ERROR
      "T056 package source contract: CMakeLists.txt must install and copy ${_module} into the build-tree package")
  endif()
endforeach()

# Public-header isolation is an extensible list. Do not make T056 depend on
# adjacency between its header and later public headers: adding a new isolated
# compile probe must not invalidate the older EmbeddedResourceEntry contract.
string(REGEX MATCH "foreach\\(_header IN ITEMS [^\n]*\\)" _header_compile_list "${_root_cmake}")
if(_header_compile_list STREQUAL "")
  message(FATAL_ERROR "T056 package source contract: public-header compile list is missing")
endif()
foreach(_header IN ITEMS embedded_resource resource_manager)
  string(FIND " ${_header_compile_list} " " ${_header} " _header_compile)
  if(_header_compile EQUAL -1)
    message(FATAL_ERROR
      "T056 package source contract: ${_header}.hpp lacks isolated public-header compilation")
  endif()
endforeach()

foreach(_config IN ITEMS cmake/NativeUIConfig.cmake.in cmake/NativeUIBuildTreeConfig.cmake.in)
  file(READ "${SOURCE_DIR}/${_config}" _config_text)
  string(FIND "${_config_text}" "NativeUIBinaryData.cmake" _binary_data_include)
  if(_binary_data_include EQUAL -1)
    message(FATAL_ERROR "T056 package source contract: ${_config} does not expose NativeUIBinaryData.cmake")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/include/nativeui/nativeui.hpp" _umbrella)
string(FIND "${_umbrella}" "#include <nativeui/embedded_resource.hpp>" _umbrella_include)
if(_umbrella_include EQUAL -1)
  message(FATAL_ERROR "T056 package source contract: nativeui.hpp does not expose EmbeddedResourceEntry")
endif()

message(STATUS "T056 package source contract passed")
