cmake_minimum_required(VERSION 3.24)
if(APPLE)
  project(NativeUIT042Stress LANGUAGES C CXX OBJC)
else()
  project(NativeUIT042Stress LANGUAGES C CXX)
endif()

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

option(T042_BUILD_PLATFORM_STRESS "Build native platform stress fixtures" ON)

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM ${T042_BUILD_PLATFORM_STRESS} CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)
enable_testing()

add_executable(nativeui_t042_lifecycle_stress
  "${NATIVEUI_SOURCE_DIR}/tests/t042_lifecycle_stress.cpp"
)
target_include_directories(nativeui_t042_lifecycle_stress PRIVATE
  "${NATIVEUI_SOURCE_DIR}/tests"
)
target_link_libraries(nativeui_t042_lifecycle_stress PRIVATE NativeUI::Core)
nativeui_enable_project_warnings(nativeui_t042_lifecycle_stress)
nativeui_enable_sanitizers(nativeui_t042_lifecycle_stress)
add_test(NAME nativeui_t042_lifecycle_stress COMMAND nativeui_t042_lifecycle_stress)
set_tests_properties(nativeui_t042_lifecycle_stress PROPERTIES
  LABELS "lifecycle;stress;t042"
  TIMEOUT 20
)

if(T042_BUILD_PLATFORM_STRESS)
  # NativeUI's source-tree helper resolves the pinned Pugl root in its own
  # directory scope. This outer stress project intentionally keeps production
  # CMake untouched, so recover that exact root from the already-created Pugl
  # target instead of guessing a CPM _deps path.
  if(TARGET nativeui_pugl_common)
    get_target_property(_t042_pugl_sources nativeui_pugl_common SOURCES)
  elseif(TARGET nativeui_pugl)
    get_target_property(_t042_pugl_sources nativeui_pugl SOURCES)
  else()
    message(FATAL_ERROR "T042 platform stress could not locate NativeUI's Pugl target")
  endif()
  list(GET _t042_pugl_sources 0 _t042_pugl_source)
  get_filename_component(_t042_pugl_src_dir "${_t042_pugl_source}" DIRECTORY)
  get_filename_component(NATIVEUI_PUGL_SOURCE_DIR "${_t042_pugl_src_dir}" DIRECTORY)
  if(NOT EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
    message(FATAL_ERROR "T042 derived invalid Pugl source root: ${NATIVEUI_PUGL_SOURCE_DIR}")
  endif()

  add_executable(nativeui_t042_platform_stress
    "${NATIVEUI_SOURCE_DIR}/tests/t042_platform_stress.cpp"
  )
  _nativeui_attach_consumer_platform(
    TARGET nativeui_t042_platform_stress
    CONSUMER_ID org.nativeui.test.t042-platform-stress
  )
  nativeui_enable_project_warnings(nativeui_t042_platform_stress)
  nativeui_enable_sanitizers(nativeui_t042_platform_stress)

  add_test(
    NAME nativeui_t042_embedded_sequential_100
    COMMAND nativeui_t042_platform_stress --embedded-sequential
  )
  add_test(
    NAME nativeui_t042_embedded_two_live_50
    COMMAND nativeui_t042_platform_stress --embedded-two-live
  )
  add_test(
    NAME nativeui_t042_embedded_capture_focus_teardown
    COMMAND nativeui_t042_platform_stress --embedded-teardown
  )
  add_test(
    NAME nativeui_t042_standalone_sequential_50
    COMMAND ${CMAKE_COMMAND}
      "-DEXE=$<TARGET_FILE:nativeui_t042_platform_stress>"
      -P "${NATIVEUI_SOURCE_DIR}/tests/run_t042_standalone_stress.cmake"
  )
  add_test(
    NAME nativeui_t042_standalone_supported_multi_instance
    COMMAND nativeui_t042_platform_stress --standalone-contract
  )
  set_tests_properties(
    nativeui_t042_embedded_sequential_100
    nativeui_t042_embedded_two_live_50
    nativeui_t042_embedded_capture_focus_teardown
    nativeui_t042_standalone_sequential_50
    nativeui_t042_standalone_supported_multi_instance
    PROPERTIES
      LABELS "integration;platform;stress;t042"
      TIMEOUT 120
  )
endif()
