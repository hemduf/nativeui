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
  "${_features_dir}/*.cpp"
)
list(SORT _canonical_sources)

set(_expected_examples)
foreach(_source IN LISTS _canonical_sources)
  if(NOT _source MATCHES "^((t[0-9][0-9][0-9]_)?[a-z][a-z0-9_]*)[.]cpp$")
    message(FATAL_ERROR
      "Feature example discovery contract: discovery matched invalid filename ${_source}")
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
    t049_gallery
    t060_multi_window_application
    t065_ui_dispatcher)
  if(NOT "${_required}" IN_LIST _discovered_examples)
    message(FATAL_ERROR
      "Feature example discovery contract: missing required example ${_required}")
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
  "nativeui_discover_feature_examples("
  "root automatic discovery call")
require_text("${_root_cmake}"
  "NATIVEUI_FEATURE_EXAMPLES"
  "root discovered example output")
require_text("${_helper_source}"
  "CONFIGURE_DEPENDS"
  "automatic CMake reconfigure when feature sources change")

string(FIND "${_root_cmake}" "set(NATIVEUI_FEATURE_EXAMPLES\n  t" _manual_list_index)
if(NOT _manual_list_index EQUAL -1)
  message(FATAL_ERROR
    "Feature example discovery contract: root CMake still contains a manually maintained feature example list")
endif()

set(_gallery_source "${_features_dir}/t049_gallery.cpp")
if(NOT EXISTS "${_gallery_source}")
  message(FATAL_ERROR "Feature example discovery contract: missing T049 gallery source")
endif()
file(READ "${_gallery_source}" _gallery)

foreach(_forbidden IN ITEMS
    "nativeui/detail/"
    "#include <pugl/"
    "#include <Sk"
    "#include <windows.h>"
    "#include <AppKit/"
    "#include <X11/")
  string(FIND "${_gallery}" "${_forbidden}" _forbidden_index)
  if(NOT _forbidden_index EQUAL -1)
    message(FATAL_ERROR
      "Feature example discovery contract: T049 gallery uses private/platform surface ${_forbidden}")
  endif()
endforeach()

foreach(_token IN ITEMS
    "ui::Row{"
    "ui::Column{"
    "ui::Stack{"
    "ui::Grid{"
    "ui::Padding{"
    "ui::Flex{"
    "ui::Clip{"
    "ui::TextInput{"
    "ui::TextArea{"
    "ui::Button{"
    "ui::Toggle{"
    "ui::Checkbox{"
    "ui::RadioButton{"
    "ui::Knob{"
    "ui::Slider{"
    "ui::RangeSlider{"
    "ui::ProgressBar{"
    "ui::Meter{"
    "ui::ScrollView{"
    "ui::ComboBox<int>{"
    "ui::PopupMenu{"
    "ui::ListView<int>{"
    "ui::Tabs<int>{"
    "draw_image"
    "draw_svg"
    "ui::ButtonStyle"
    "example::self_test_requested")
  string(FIND "${_gallery}" "${_token}" _token_index)
  if(_token_index EQUAL -1)
    message(FATAL_ERROR
      "Feature example discovery contract: T049 gallery missing required public surface ${_token}")
  endif()
endforeach()

message(STATUS
  "Feature example discovery contract passed (${_expected_examples})")
