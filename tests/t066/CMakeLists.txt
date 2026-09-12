cmake_minimum_required(VERSION 3.24)
if(APPLE)
  project(NativeUIT066WindowControls LANGUAGES C CXX OBJC)
else()
  project(NativeUIT066WindowControls LANGUAGES C CXX)
endif()

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM ON CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)

add_executable(nativeui_t066_window_control_state_tests
  "${NATIVEUI_SOURCE_DIR}/tests/t066_window_control_state_tests.cpp"
)
target_include_directories(nativeui_t066_window_control_state_tests PRIVATE
  "${NATIVEUI_SOURCE_DIR}/tests"
)
target_link_libraries(nativeui_t066_window_control_state_tests PRIVATE NativeUI::Core)
nativeui_enable_project_warnings(nativeui_t066_window_control_state_tests)
nativeui_enable_sanitizers(nativeui_t066_window_control_state_tests)

# The final-consumer attachment helper needs the exact pinned Pugl source root
# in this outer project scope. Recover it from the Pugl target already created
# by NativeUI rather than depending on a CPM build-directory layout.
if(TARGET nativeui_pugl_common)
  get_target_property(_t066_pugl_sources nativeui_pugl_common SOURCES)
elseif(TARGET nativeui_pugl)
  get_target_property(_t066_pugl_sources nativeui_pugl SOURCES)
else()
  message(FATAL_ERROR "T066 platform harness could not locate NativeUI's Pugl target")
endif()
list(GET _t066_pugl_sources 0 _t066_pugl_source)
get_filename_component(_t066_pugl_src_dir "${_t066_pugl_source}" DIRECTORY)
get_filename_component(NATIVEUI_PUGL_SOURCE_DIR "${_t066_pugl_src_dir}" DIRECTORY)
if(NOT EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
  message(FATAL_ERROR "T066 derived invalid Pugl source root: ${NATIVEUI_PUGL_SOURCE_DIR}")
endif()

add_executable(nativeui_t066_window_controls_platform_tests
  "${NATIVEUI_SOURCE_DIR}/tests/t066_window_controls_platform_tests.cpp"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_t066_window_controls_platform_tests
  CONSUMER_ID org.nativeui.test.t066-window-controls
)
nativeui_enable_project_warnings(nativeui_t066_window_controls_platform_tests)
nativeui_enable_sanitizers(nativeui_t066_window_controls_platform_tests)

add_executable(nativeui_t066_destroy_survivor_tests
  "${NATIVEUI_SOURCE_DIR}/tests/t066_destroy_survivor_tests.cpp"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_t066_destroy_survivor_tests
  CONSUMER_ID org.nativeui.test.t066-destroy-survivor
)
nativeui_enable_project_warnings(nativeui_t066_destroy_survivor_tests)
nativeui_enable_sanitizers(nativeui_t066_destroy_survivor_tests)

# Exercise the exact feature example with a normal consumer-scoped bridge.
# Root macOS CI intentionally shares one bridge across most examples for build
# speed; T066 changes the native window delegate close lifecycle, so its exact
# example must be qualified with the production consumer-isolation contract.
add_executable(nativeui_t066_window_controls_example
  "${NATIVEUI_SOURCE_DIR}/examples/features/t066_window_controls.cpp"
)
target_include_directories(nativeui_t066_window_controls_example PRIVATE
  "${NATIVEUI_SOURCE_DIR}/examples/features"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_t066_window_controls_example
  CONSUMER_ID org.nativeui.test.t066-window-controls-example
)
nativeui_enable_project_warnings(nativeui_t066_window_controls_example)
nativeui_enable_sanitizers(nativeui_t066_window_controls_example)
