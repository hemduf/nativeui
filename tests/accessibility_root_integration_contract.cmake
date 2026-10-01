cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/tests/CMakeLists.txt" _tests_cmake)
file(READ "${SOURCE_DIR}/include/nativeui/nativeui.hpp" _umbrella)
set(_header_fixture "${SOURCE_DIR}/tests/headers/semantics.cpp")

function(require_text haystack needle description)
  string(FIND "${haystack}" "${needle}" _index)
  if(_index EQUAL -1)
    message(FATAL_ERROR "accessibility root integration contract: missing ${description}: ${needle}")
  endif()
endfunction()

# Extract one function definition so the label/registration contract is checked
# against the helper body rather than an arbitrary substring of the file.
function(require_function_body haystack marker description out_var)
  string(FIND "${haystack}" "${marker}" _start)
  if(_start EQUAL -1)
    message(FATAL_ERROR "accessibility root integration contract: missing ${description}: ${marker}")
  endif()
  string(SUBSTRING "${haystack}" ${_start} -1 _tail)
  string(FIND "${_tail}" "endfunction()" _end)
  if(_end EQUAL -1)
    message(FATAL_ERROR "accessibility root integration contract: unterminated ${description}")
  endif()
  math(EXPR _length "${_end} + 13")
  string(SUBSTRING "${_tail}" 0 ${_length} _body)
  set(${out_var} "${_body}" PARENT_SCOPE)
endfunction()

# Extract one top-level conditional block so a registration can be proven to
# live behind the correct opt-in gate instead of merely somewhere in the file.
function(require_conditional_body haystack marker description out_var)
  string(FIND "${haystack}" "${marker}" _start)
  if(_start EQUAL -1)
    message(FATAL_ERROR "accessibility root integration contract: missing ${description}: ${marker}")
  endif()
  string(SUBSTRING "${haystack}" ${_start} -1 _tail)
  string(FIND "${_tail}" "endif()" _end)
  if(_end EQUAL -1)
    message(FATAL_ERROR "accessibility root integration contract: unterminated ${description}")
  endif()
  math(EXPR _length "${_end} + 7")
  string(SUBSTRING "${_tail}" 0 ${_length} _body)
  set(${out_var} "${_body}" PARENT_SCOPE)
endfunction()

# Every semantic/accessibility accessibility suite must be registered in the root CTest run
# through the helpers below. The helper body owns the private include setup and
# the unit/accessibility/accessibility labels; checking it here keeps one label source of
# truth while still asserting each individual registration.
require_text("${_tests_cmake}" "nativeui_accessibility_root_integration_contract"
  "contract registration")
require_function_body("${_tests_cmake}"
  "function(nativeui_add_accessibility_test name source)"
  "portable accessibility test helper" _portable_helper)
require_function_body("${_tests_cmake}"
  "function(nativeui_add_macos_accessibility_test name source)"
  "macOS accessibility test helper" _macos_helper)

string(ASCII 36 _dollar)
require_text("${_portable_helper}"
  "nativeui_add_core_test(${_dollar}{name} ${_dollar}{source})"
  "portable helper core-test registration")
require_text("${_portable_helper}" "LABELS \"unit;accessibility;accessibility\""
  "portable accessibility test labels")
require_text("${_macos_helper}"
  "nativeui_add_accessibility_test(${_dollar}{name} ${_dollar}{source})"
  "macOS helper portable-helper registration")
require_text("${_macos_helper}" "OBJCXX_STANDARD 20" "macOS AppKit test language standard")
require_text("${_macos_helper}" "AppKit REQUIRED" "macOS AppKit framework linkage")

# name|source|registration helper
set(_accessibility_tests
  "nativeui_t045_semantics|tests/t045_semantics_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_proxy|tests/t045/accessibility_semantic_proxy_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_children_query|tests/t045/accessibility_semantic_children_query_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_proxy_cache|tests/t045/accessibility_semantic_proxy_cache_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_diff_regression|tests/t045/accessibility_semantic_diff_regression_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_action_policy|tests/t045/accessibility_semantic_action_policy_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_action_router|tests/t045/accessibility_semantic_action_router_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_action_target_binding|tests/t045/accessibility_semantic_action_target_binding_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_action_view_binding|tests/t045/accessibility_semantic_action_view_binding_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_native_view_bridge|tests/t045/accessibility_semantic_native_view_bridge_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_native_generation|tests/t045/accessibility_semantic_native_generation_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_read_only|tests/t045/accessibility_semantic_read_only_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_widget_semantic_values|tests/t045/accessibility_widget_semantic_values_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_widget_component_semantics|tests/accessibility_widget_component_semantics_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_view_state|tests/t045/accessibility_semantic_view_state_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_publication_batch|tests/t045/accessibility_semantic_publication_batch_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_native_bounds|tests/t045/accessibility_semantic_native_bounds_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_semantic_native_geometry_capture|tests/t045/accessibility_semantic_native_geometry_capture_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_retained_action_bridge_tests|tests/accessibility_retained_action_bridge_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_retained_native_checkpoint_tests|tests/accessibility_retained_native_checkpoint_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_publication_sink_tests|tests/accessibility_publication_sink_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_virtual_list_action_tests|tests/accessibility_virtual_list_action_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_concurrent_snapshot_readers|tests/accessibility_concurrent_snapshot_readers.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_uia_mapping|tests/t045/accessibility_uia_mapping_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_accessibility_uia_provider|tests/t045/accessibility_uia_provider_tests.cpp|nativeui_add_accessibility_test"
  "nativeui_semantic_macos_child_projection|tests/t045/semantic_macos_child_projection_tests.cpp|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_mapping|tests/t045/semantic_macos_mapping_tests.cpp|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_frame|tests/t045/semantic_macos_frame_tests.cpp|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_appkit|tests/t045/semantic_macos_appkit_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_appkit_frame|tests/t045/semantic_macos_appkit_frame_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_proxy_cache|tests/t045/semantic_macos_proxy_cache_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_children_callbacks|tests/t045/semantic_macos_children_callbacks_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_interaction|tests/t045/semantic_macos_interaction_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_production_bridge|tests/t045/semantic_macos_production_bridge_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_semantic_macos_notifications|tests/t045/semantic_macos_notification_tests.mm|nativeui_add_macos_accessibility_test"
  "nativeui_accessibility_objc_runtime_prefix_probe|tests/t045/semantic_macos_runtime_prefix_probe_tests.mm|nativeui_add_macos_accessibility_test"
)

foreach(_entry IN LISTS _accessibility_tests)
  string(REPLACE "|" ";" _fields "${_entry}")
  list(GET _fields 0 _name)
  list(GET _fields 1 _source)
  list(GET _fields 2 _helper)
  require_text("${_tests_cmake}" "${_helper}(${_name}" "registration for ${_name}")
  if(NOT EXISTS "${SOURCE_DIR}/${_source}")
    message(FATAL_ERROR
      "accessibility root integration contract: missing accessibility test source: ${_source}")
  endif()
endforeach()

# The standalone tests/t045 project ships only include directories: it has no
# Skia include/link wiring. The helper-level widget semantic values suite must
# therefore stay free of the umbrella paint/widget surface, while the
# component-level widget assertions live in the root-only suite that links the
# real Core through nativeui_add_accessibility_test.
file(READ "${SOURCE_DIR}/tests/t045/accessibility_widget_semantic_values_tests.cpp" _standalone_widget_values)
foreach(_forbidden IN ITEMS "nativeui/nativeui.hpp" "nativeui/paint.hpp" "nativeui/widgets.hpp")
  string(FIND "${_standalone_widget_values}" "${_forbidden}" _forbidden_index)
  if(NOT _forbidden_index EQUAL -1)
    message(FATAL_ERROR
      "accessibility root integration contract: standalone widget values suite must stay Skia-free: ${_forbidden}")
  endif()
endforeach()
require_text("${_standalone_widget_values}" "nativeui/detail/semantic_widget_info.hpp"
  "standalone widget values helper include")
file(READ "${SOURCE_DIR}/tests/accessibility_widget_component_semantics_tests.cpp" _root_widget_component)
require_text("${_root_widget_component}" "nativeui/nativeui.hpp"
  "root-only widget component suite umbrella include")
require_text("${_root_widget_component}" "test_support.hpp"
  "root-only widget component suite test support")
file(READ "${SOURCE_DIR}/tests/t045/CMakeLists.txt" _standalone_cmake)
require_text("${_standalone_cmake}" "nativeui_add_semantics_test(accessibility_widget_semantic_values"
  "standalone widget values registration")
# The Batch 7b concurrent reader stress is platform-free and header-only, so it
# must stay registered in the standalone semantic project as well.
require_text("${_standalone_cmake}"
  "nativeui_add_semantics_test(accessibility_concurrent_snapshot_readers"
  "standalone concurrent snapshot reader registration")

# The dispatcher-backed action suites keep the standalone dispatcher compilation.
require_text("${_tests_cmake}"
  "target_sources(nativeui_accessibility_semantic_action_router PRIVATE" "action-router dispatcher source")
require_text("${_tests_cmake}"
  "target_sources(nativeui_accessibility_semantic_native_view_bridge PRIVATE" "native-view-bridge dispatcher source")
require_text("${_tests_cmake}"
  "target_link_libraries(nativeui_accessibility_semantic_proxy_cache PRIVATE Threads::Threads" "proxy-cache thread linkage")

# The public semantics header must be exported by the umbrella and compiled as
# its own translation unit, so umbrella include order cannot mask a missing
# direct include in nativeui/semantics.hpp.
require_text("${_umbrella}" "#include <nativeui/semantics.hpp>" "semantics umbrella export")
string(REGEX MATCH "foreach\\(_header IN ITEMS[^)]*\\)" _public_header_loop "${_tests_cmake}")
if(_public_header_loop STREQUAL "")
  message(FATAL_ERROR "accessibility root integration contract: missing public-header compile loop")
endif()
require_text("${_public_header_loop}" " semantics " "isolated semantics public-header compile coverage")
require_text("${_tests_cmake}" "add_library(nativeui_header_${_dollar}{_header}_compile OBJECT"
  "isolated public-header compile target")
if(NOT EXISTS "${_header_fixture}")
  message(FATAL_ERROR
    "accessibility root integration contract: missing isolated public-header fixture: ${_header_fixture}")
endif()
file(READ "${_header_fixture}" _header_fixture_content)
require_text("${_header_fixture_content}" "#include <nativeui/semantics.hpp>"
  "isolated semantics public-header fixture include")

# The accessibility platform publication smoke must stay behind the explicit smoke gate,
# use its own consumer identity and expose its AppKit bridge for the prefix
# audit. The portable publication-sink suite above covers the domain contract;
# this guards the real-pump registration.
# Identify the shared smoke group: the macOS visibility suite has its own
# earlier opt-in block, which must not shadow the accessibility registration.
require_conditional_body("${_tests_cmake}"
  "if(NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS)\n      add_test(NAME nativeui_smoke_standalone"
  "platform smoke gate" _smoke_gate)
require_text("${_smoke_gate}" "add_test(NAME nativeui_smoke_accessibility "
  "accessibility smoke CTest registration inside the smoke gate")
require_text("${_tests_cmake}" "add_executable(nativeui_smoke_accessibility "
  "accessibility smoke executable target")
require_text("${_tests_cmake}" "CONSUMER_ID org.nativeui.test.smoke-accessibility"
  "accessibility smoke consumer identity")
require_text("${_tests_cmake}" "OUT_BRIDGE _nativeui_smoke_accessibility_bridge"
  "accessibility smoke bridge output for the prefix audit")
require_text("${_tests_cmake}" "nativeui_smoke_accessibility_objc_runtime_prefix"
  "accessibility smoke Objective-C prefix audit registration")
if(NOT EXISTS "${SOURCE_DIR}/tests/smoke_accessibility.cpp")
  message(FATAL_ERROR
    "accessibility root integration contract: missing accessibility smoke source: tests/smoke_accessibility.cpp")
endif()

# accessibility Batch 3 production macOS bridge: the per-consumer Objective-C++ source
# must remain discoverable by the consumer platform helper, and the smoke must
# include the automatable in-process AppKit query fixture.
file(READ "${SOURCE_DIR}/cmake/NativeUIConsumerPlatform.cmake" _consumer_platform)
require_text("${_consumer_platform}"
  "native_accessibility_macos.mm"
  "per-consumer macOS accessibility bridge source")
if(NOT EXISTS "${SOURCE_DIR}/src/detail/native_accessibility_bridge.h")
  message(FATAL_ERROR
    "accessibility root integration contract: missing accessibility C ABI header")
endif()
if(NOT EXISTS "${SOURCE_DIR}/src/detail/native_accessibility_macos.mm")
  message(FATAL_ERROR
    "accessibility root integration contract: missing macOS accessibility bridge source")
endif()
require_text("${_tests_cmake}" "smoke_accessibility_appkit.mm"
  "in-process AppKit accessibility query fixture")
require_text("${_tests_cmake}"
  "nativeui_semantic_macos_production_bridge"
  "production macOS bridge test registration")
require_text("${_tests_cmake}"
  "nativeui_accessibility_objc_runtime_prefix_probe"
  "Objective-C runtime prefix probe registration")

# accessibility Batch 4 Windows UIA fragment provider. The platform adapter is compiled
# into the platform target from the root build and into the package platform
# target for consumers; Windows uses it instead of the fail-closed stub, and
# the Windows-only client suite plus smoke extension stay behind if(WIN32).
file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${SOURCE_DIR}/cmake/Dependencies.cmake" _dependencies)
if(NOT EXISTS "${SOURCE_DIR}/src/detail/native_accessibility_windows.cpp")
  message(FATAL_ERROR
    "accessibility root integration contract: missing Windows accessibility bridge source")
endif()
require_text("${_root_cmake}" "src/detail/native_accessibility_windows.cpp"
  "root platform target Windows accessibility bridge source")
require_text("${_consumer_platform}" "native_accessibility_windows.cpp"
  "package platform Windows accessibility bridge source")
require_text("${_dependencies}" "if(NOT WIN32)" "Windows stub exclusion gate")
require_text("${_consumer_platform}" "if(NOT WIN32)" "package Windows stub exclusion gate")
require_conditional_body("${_tests_cmake}"
  "if(WIN32)"
  "Windows UIA provider test gate" _win32_gate)
require_text("${_win32_gate}" "nativeui_accessibility_uia_win32_provider_tests"
  "Windows UIA provider test registration inside the WIN32 gate")
require_text("${_win32_gate}"
  "src/detail/native_accessibility_windows.cpp"
  "Windows UIA provider test adapter compilation")
require_text("${_tests_cmake}" "smoke_accessibility_windows.cpp"
  "in-process Windows UIA query fixture")
require_text("${_tests_cmake}" "uiautomationcore"
  "Windows UIA client/provider linkage")
if(NOT EXISTS "${SOURCE_DIR}/tests/t045/accessibility_uia_win32_provider_tests.cpp")
  message(FATAL_ERROR
    "accessibility root integration contract: missing Windows UIA provider test source")
endif()
if(NOT EXISTS "${SOURCE_DIR}/tests/smoke_accessibility_windows.cpp" OR
   NOT EXISTS "${SOURCE_DIR}/tests/smoke_accessibility_windows.hpp")
  message(FATAL_ERROR
    "accessibility root integration contract: missing Windows accessibility smoke extension")
endif()

# The dedicated accessibility feature example ships through the same automatic
# discovery/registration path as every other feature ticket: the macOS
# application bundle, the labelled CTest --self-test and the display-less
# compile target used when platform/examples are disabled. The example itself
# must stay on the normal public NativeUI API and therefore must not include
# any nativeui/detail/ header; the virtual-collection self-test reads the
# shared metadata only through the public VirtualListState/Semantics surface.
file(READ "${SOURCE_DIR}/examples/features/CMakeLists.txt" _features_cmake)
set(_feature_example_helper "${SOURCE_DIR}/cmake/NativeUIFeatureExamples.cmake")
if(NOT EXISTS "${_feature_example_helper}")
  message(FATAL_ERROR
    "accessibility root integration contract: missing feature example discovery helper: ${_feature_example_helper}")
endif()
include("${_feature_example_helper}")
nativeui_discover_feature_examples(_feature_examples "${SOURCE_DIR}")
if(NOT "accessibility" IN_LIST _feature_examples)
  message(FATAL_ERROR
    "accessibility root integration contract: accessibility is not discovered as a feature example")
endif()
set(_example "${SOURCE_DIR}/examples/features/accessibility.cpp")
if(NOT EXISTS "${_example}")
  message(FATAL_ERROR
    "accessibility root integration contract: missing dedicated feature example: ${_example}")
endif()
file(READ "${_example}" _example_source)
string(FIND "${_example_source}" "nativeui/detail/" _detail_index)
if(NOT _detail_index EQUAL -1)
  message(FATAL_ERROR
    "accessibility feature example must use only the normal public NativeUI API")
endif()
require_text("${_features_cmake}" "nativeui_add_application(nativeui_example_"
  "feature application creation")
require_text("${_features_cmake}" "add_test(NAME nativeui_example_"
  "feature example CTest registration")
require_text("${_features_cmake}" "add_library(nativeui_feature_example_"
  "display-less feature example compile target")

message(STATUS "accessibility root accessibility integration contract passed")
