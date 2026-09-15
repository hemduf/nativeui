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

# T069 made public-header isolation exhaustive instead of maintaining a stale
# hand-written list. Verify the discovery mechanism and the older T056 headers
# it must cover, without coupling this contract to header ordering.
foreach(_needle IN ITEMS
    "file(GLOB _nativeui_public_headers CONFIGURE_DEPENDS"
    "include/nativeui/*.hpp"
    "foreach(_header_file IN LISTS _nativeui_public_headers)"
    "file(GENERATE OUTPUT")
  string(FIND "${_root_cmake}" "${_needle}" _header_contract)
  if(_header_contract EQUAL -1)
    message(FATAL_ERROR
      "T056 package source contract: exhaustive public-header isolation is missing: ${_needle}")
  endif()
endforeach()
foreach(_header IN ITEMS embedded_resource resource_manager)
  if(NOT EXISTS "${SOURCE_DIR}/include/nativeui/${_header}.hpp")
    message(FATAL_ERROR
      "T056 package source contract: ${_header}.hpp is missing from exhaustive public-header discovery")
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
