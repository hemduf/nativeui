cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_helper "${SOURCE_DIR}/cmake/NativeUIFeatureExamples.cmake")
set(_features_dir "${SOURCE_DIR}/examples/features")

if(NOT EXISTS "${_helper}")
  message(FATAL_ERROR "Feature example discovery contract: missing ${_helper}")
endif()

include("${_helper}")
nativeui_discover_feature_examples(_discovered_examples "${SOURCE_DIR}")

file(GLOB _canonical_sources
  RELATIVE "${_features_dir}"
  "${_features_dir}/t[0-9][0-9][0-9]_*.cpp"
)
list(SORT _canonical_sources)

set(_expected_examples)
foreach(_source IN LISTS _canonical_sources)
  if(NOT _source MATCHES "^t[0-9][0-9][0-9]_[A-Za-z0-9_]+[.]cpp$")
    message(FATAL_ERROR
      "Feature example discovery contract: canonical glob matched invalid filename ${_source}")
  endif()
  get_filename_component(_name "${_source}" NAME_WE)
  list(APPEND _expected_examples "${_name}")
endforeach()

if(NOT "${_discovered_examples}" STREQUAL "${_expected_examples}")
  message(FATAL_ERROR
    "Feature example discovery contract: discovered examples differ from canonical sources\n"
    "expected: ${_expected_examples}\n"
    "actual:   ${_discovered_examples}")
endif()

foreach(_required IN ITEMS
    t060_multi_window_application
    t065_ui_dispatcher)
  if(NOT "${_required}" IN_LIST _discovered_examples)
    message(FATAL_ERROR
      "Feature example discovery contract: missing previously omitted example ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${_helper}" _helper_source)

function(require_text haystack needle description)
  string(FIND "${haystack}" "${needle}" _index)
  if(_index EQUAL -1)
    message(FATAL_ERROR
      "Feature example discovery contract: missing ${description}: ${needle}")
  endif()
endfunction()

require_text("${_root_cmake}"
  "include(cmake/NativeUIFeatureExamples.cmake)"
  "root discovery helper include")
require_text("${_root_cmake}"
  "nativeui_discover_feature_examples(NATIVEUI_FEATURE_EXAMPLES"
  "root automatic discovery call")
require_text("${_helper_source}"
  "CONFIGURE_DEPENDS"
  "automatic CMake reconfigure when feature sources change")

string(FIND "${_root_cmake}" "set(NATIVEUI_FEATURE_EXAMPLES\n  t" _manual_list_index)
if(NOT _manual_list_index EQUAL -1)
  message(FATAL_ERROR
    "Feature example discovery contract: root CMake still contains a manually maintained feature example list")
endif()

message(STATUS
  "Feature example discovery contract passed (${_expected_examples})")
