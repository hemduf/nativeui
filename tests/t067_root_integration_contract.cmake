if(NOT DEFINED ROOT AND DEFINED SOURCE_DIR)
  set(ROOT "${SOURCE_DIR}")
endif()
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()

set(root_cmake "${ROOT}/CMakeLists.txt")
set(feature_examples_cmake "${ROOT}/cmake/NativeUIFeatureExamples.cmake")
set(nativeui_header "${ROOT}/include/nativeui/nativeui.hpp")
set(virtual_list_header "${ROOT}/include/nativeui/virtual_list.hpp")
set(virtual_list_model "${ROOT}/include/nativeui/detail/virtual_list_model.hpp")
set(virtual_list_retained "${ROOT}/include/nativeui/detail/virtual_list_retained.hpp")
set(virtual_list_example "${ROOT}/examples/features/t067_virtual_list.cpp")
set(virtual_list_test "${ROOT}/tests/t067_virtual_list_contract.cpp")
set(virtual_list_public_test "${ROOT}/tests/t067_public_api_tests.cpp")
set(virtual_list_semantic_test "${ROOT}/tests/t067_semantic_api_tests.cpp")
set(virtual_list_window_test "${ROOT}/tests/t067_virtual_list_window_contract.cpp")
set(virtual_list_retained_test "${ROOT}/tests/t067_retained_tests.cpp")
set(virtual_list_retained_contract "${ROOT}/tests/t067_retained_contract.inc")
set(virtual_list_visual_test "${ROOT}/tests/t067_visual_tests.cpp")
set(virtual_list_example_readme "${ROOT}/examples/features/README.md")
set(virtual_list_benchmark "${ROOT}/tests/t051/t067_virtual_list_benchmarks.cpp")

foreach(path IN LISTS
        root_cmake
        feature_examples_cmake
        nativeui_header
        virtual_list_header
        virtual_list_model
        virtual_list_retained
        virtual_list_example
        virtual_list_test
        virtual_list_public_test
        virtual_list_semantic_test
        virtual_list_window_test
        virtual_list_retained_test
        virtual_list_retained_contract
        virtual_list_visual_test
        virtual_list_example_readme
        virtual_list_benchmark)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "T067 required file missing: ${path}")
  endif()
endforeach()

file(READ "${root_cmake}" root_content)
file(READ "${nativeui_header}" nativeui_content)
file(READ "${virtual_list_header}" virtual_list_header_content)
file(READ "${virtual_list_model}" virtual_list_model_content)
file(READ "${virtual_list_retained}" virtual_list_retained_content)
file(READ "${virtual_list_example_readme}" virtual_list_example_readme_content)
file(READ "${virtual_list_benchmark}" virtual_list_benchmark_content)

function(require_text haystack needle message_text)
  string(FIND "${haystack}" "${needle}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${message_text}: missing '${needle}'")
  endif()
endfunction()

# The feature examples are registered generically. Prove the discovery helper
# resolves the real T067 source instead of depending on a brittle literal
# target name in the root CMakeLists.
include("${feature_examples_cmake}")
nativeui_discover_feature_examples(discovered_feature_examples "${ROOT}")
list(FIND discovered_feature_examples "t067_virtual_list" t067_example_index)
if(t067_example_index EQUAL -1)
  message(FATAL_ERROR
    "feature-example discovery must register examples/features/t067_virtual_list.cpp")
endif()

require_text("${root_content}" "include(cmake/NativeUIFeatureExamples.cmake)"
  "root CMake must load generic feature-example discovery")
require_text("${root_content}" "nativeui_discover_feature_examples(NATIVEUI_FEATURE_EXAMPLES"
  "root CMake must discover feature examples through the shared helper")
require_text("${root_content}" "foreach(_example IN LISTS NATIVEUI_FEATURE_EXAMPLES)"
  "root CMake must register every discovered feature example")
require_text("${root_content}" [=[nativeui_add_feature_example(${_example})]=]
  "root CMake must route discovered feature examples through the common target helper")
require_text("${root_content}" "nativeui_t067_tests"
  "root CMake must register the T067 model contract test")
require_text("${root_content}" "nativeui_t067_public_api_tests"
  "root CMake must register the T067 public API contract test")
require_text("${root_content}" "nativeui_t067_semantic_api_tests"
  "root CMake must register the T067 semantic API contract test")
require_text("${root_content}" "nativeui_t067_window_contract"
  "root CMake must register the T067 visible-window contract test")
require_text("${root_content}" "nativeui_t067_retained_tests"
  "root CMake must register the T067 retained-row contract test")
require_text("${root_content}" "nativeui_t067_visual_tests"
  "root CMake must register the T067 visual contract test")
require_text("${root_content}" "nativeui_t067_root_contract"
  "root CMake must register the T067 root source contract")

require_text("${root_content}" "file(GLOB _nativeui_public_headers CONFIGURE_DEPENDS"
  "root CMake must discover every normal public header for standalone compilation")
require_text("${root_content}" "include/nativeui/*.hpp"
  "public-header compile coverage must include every root nativeui header")
require_text("${root_content}" "foreach(_header_file IN LISTS _nativeui_public_headers)"
  "public-header compile coverage must iterate the discovered root headers")
require_text("${root_content}" "#include <nativeui/${_header_file}>"
  "public-header compile coverage must generate one direct include translation unit per header")
require_text("${nativeui_content}" "#include <nativeui/virtual_list.hpp>"
  "nativeui.hpp must expose the T067 public wrapper")
require_text("${virtual_list_header_content}" "class VirtualListModel"
  "virtual_list.hpp must expose the stable non-templated model")
require_text("${virtual_list_header_content}" "class VirtualList"
  "virtual_list.hpp must expose the retained VirtualList component")
require_text("${virtual_list_header_content}" "class VariableHeightVirtualList"
  "virtual_list.hpp must expose the variable-height retained VirtualList component")
require_text("${virtual_list_model_content}" "class FenwickTree"
  "T067 internal model must retain logarithmic prefix sums")
require_text("${virtual_list_model_content}" "std::vector<float> values_"
  "T067 model must retain compatibility with variable-height updates")
require_text("${virtual_list_retained_content}" "class RetainedRowPool"
  "T067 retained path must retain keyed row ownership")
require_text("${virtual_list_retained_content}" "scroll_state.changed_since"
  "T067 retained path must observe scroll state directly")
require_text("${virtual_list_retained_content}" "row.paint_if_dirty"
  "T067 retained path must paint only retained rows that changed")
require_text("${virtual_list_example_readme_content}" "nativeui_example_t049_gallery"
  "feature-example docs must identify the public aggregate gallery")
require_text("${virtual_list_benchmark_content}" "RetainedVirtualListFixture"
  "T067 retained path must remain covered by the benchmark harness")
