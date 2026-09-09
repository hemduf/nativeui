cmake_minimum_required(VERSION 3.24)
if(APPLE)
  project(NativeUIT060Application LANGUAGES C CXX OBJC)
else()
  project(NativeUIT060Application LANGUAGES C CXX)
endif()

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM ON CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)
enable_testing()

# NativeUI's source-tree helper resolves the pinned Pugl root in its own
# directory scope. Recover that exact source root from the already-created Pugl
# target so this outer acceptance project does not guess a CPM _deps path.
if(TARGET nativeui_pugl_common)
  get_target_property(_t060_pugl_sources nativeui_pugl_common SOURCES)
elseif(TARGET nativeui_pugl)
  get_target_property(_t060_pugl_sources nativeui_pugl SOURCES)
else()
  message(FATAL_ERROR "T060 application tests could not locate NativeUI's Pugl target")
endif()
list(GET _t060_pugl_sources 0 _t060_pugl_source)
get_filename_component(_t060_pugl_src_dir "${_t060_pugl_source}" DIRECTORY)
get_filename_component(NATIVEUI_PUGL_SOURCE_DIR "${_t060_pugl_src_dir}" DIRECTORY)
if(NOT EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
  message(FATAL_ERROR "T060 derived invalid Pugl source root: ${NATIVEUI_PUGL_SOURCE_DIR}")
endif()

add_executable(nativeui_t060_application_contract
  "${NATIVEUI_SOURCE_DIR}/tests/t060_application_tests.cpp"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_t060_application_contract
  CONSUMER_ID org.nativeui.test.t060-application
)
nativeui_enable_project_warnings(nativeui_t060_application_contract)
nativeui_enable_sanitizers(nativeui_t060_application_contract)

foreach(_mode IN ITEMS
    multi-window
    explicit-only
    request-quit
    poll-contract
    invalid-timeout-nan
    invalid-timeout-inf
    rejected-window
    repeated-secondary)
  string(REPLACE "-" "_" _test_suffix "${_mode}")
  add_test(
    NAME nativeui_t060_${_test_suffix}
    COMMAND nativeui_t060_application_contract "--${_mode}"
  )
endforeach()

add_test(
  NAME nativeui_t060_application_lifetime_violation
  COMMAND nativeui_t060_application_contract --lifetime-violation
)
set_tests_properties(nativeui_t060_application_lifetime_violation PROPERTIES WILL_FAIL TRUE)

set_tests_properties(
  nativeui_t060_multi_window
  nativeui_t060_explicit_only
  nativeui_t060_request_quit
  nativeui_t060_poll_contract
  nativeui_t060_invalid_timeout_nan
  nativeui_t060_invalid_timeout_inf
  nativeui_t060_rejected_window
  nativeui_t060_repeated_secondary
  nativeui_t060_application_lifetime_violation
  PROPERTIES
    LABELS "integration;platform;lifecycle;t060"
    TIMEOUT 30
)

add_executable(nativeui_example_t060_multi_window_application
  "${NATIVEUI_SOURCE_DIR}/examples/features/t060_multi_window_application.cpp"
)
target_include_directories(nativeui_example_t060_multi_window_application PRIVATE
  "${NATIVEUI_SOURCE_DIR}/examples/features"
)
_nativeui_attach_consumer_platform(
  TARGET nativeui_example_t060_multi_window_application
  CONSUMER_ID org.nativeui.example.t060-multi-window-application
)
nativeui_enable_project_warnings(nativeui_example_t060_multi_window_application)
nativeui_enable_sanitizers(nativeui_example_t060_multi_window_application)
add_test(
  NAME nativeui_example_t060_multi_window_application_self_test
  COMMAND nativeui_example_t060_multi_window_application --self-test
)
set_tests_properties(nativeui_example_t060_multi_window_application_self_test PROPERTIES
  LABELS "example;feature;t060"
  TIMEOUT 30
)
