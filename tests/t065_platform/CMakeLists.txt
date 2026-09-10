cmake_minimum_required(VERSION 3.24)
if(APPLE)
  project(NativeUIT065Platform LANGUAGES C CXX OBJC)
else()
  project(NativeUIT065Platform LANGUAGES C CXX)
endif()

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM ON CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)
enable_testing()

if(TARGET nativeui_pugl_common)
  get_target_property(_t065_pugl_sources nativeui_pugl_common SOURCES)
elseif(TARGET nativeui_pugl)
  get_target_property(_t065_pugl_sources nativeui_pugl SOURCES)
else()
  message(FATAL_ERROR "T065 platform tests could not locate NativeUI's Pugl target")
endif()
list(GET _t065_pugl_sources 0 _t065_pugl_source)
get_filename_component(_t065_pugl_src_dir "${_t065_pugl_source}" DIRECTORY)
get_filename_component(NATIVEUI_PUGL_SOURCE_DIR "${_t065_pugl_src_dir}" DIRECTORY)
if(NOT EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
  message(FATAL_ERROR "T065 derived invalid Pugl source root: ${NATIVEUI_PUGL_SOURCE_DIR}")
endif()

add_executable(nativeui_t065_platform_contract
  "${NATIVEUI_SOURCE_DIR}/tests/t065_platform_tests.cpp"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_t065_platform_contract
  CONSUMER_ID org.nativeui.test.t065-platform
)
nativeui_enable_project_warnings(nativeui_t065_platform_contract)
nativeui_enable_sanitizers(nativeui_t065_platform_contract)

foreach(_mode IN ITEMS
    standalone-worker-wake
    embedded-host-checkpoint
    owner-isolation)
  string(REPLACE "-" "_" _suffix "${_mode}")
  add_test(
    NAME nativeui_t065_${_suffix}
    COMMAND nativeui_t065_platform_contract "--${_mode}"
  )
endforeach()

set_tests_properties(
  nativeui_t065_standalone_worker_wake
  nativeui_t065_embedded_host_checkpoint
  nativeui_t065_owner_isolation
  PROPERTIES
    LABELS "integration;platform;dispatcher;t065"
    TIMEOUT 20
)
