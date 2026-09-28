cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T043 registration contract requires SOURCE_DIR")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _nativeui_root_cmake)
file(READ "${SOURCE_DIR}/tests/CMakeLists.txt" _nativeui_tests_cmake)
file(READ "${SOURCE_DIR}/examples/features/CMakeLists.txt" _nativeui_features_cmake)
set(_feature_example_helper "${SOURCE_DIR}/cmake/NativeUIFeatureExamples.cmake")

if(NOT EXISTS "${_feature_example_helper}")
  message(FATAL_ERROR
    "T043 completion artifact is not wired into the root build: missing feature example discovery helper")
endif()
include("${_feature_example_helper}")
nativeui_discover_feature_examples(_feature_examples "${SOURCE_DIR}")
if(NOT "t043_resize_scale" IN_LIST _feature_examples)
  message(FATAL_ERROR
    "T043 completion artifact is not wired into the root build: t043_resize_scale")
endif()

string(FIND "${_nativeui_root_cmake}" "add_subdirectory(examples)" _examples_directory)
string(FIND "${_nativeui_root_cmake}" "add_subdirectory(tests)" _tests_directory)
string(FIND "${_nativeui_features_cmake}" "nativeui_discover_feature_examples(" _discovery)
string(FIND "${_nativeui_tests_cmake}" "nativeui_add_core_test(nativeui_t043_view_geometry_tests" _test)
if(_examples_directory EQUAL -1 OR _tests_directory EQUAL -1 OR
   _discovery EQUAL -1 OR _test EQUAL -1)
  message(FATAL_ERROR "T043 completion artifacts are not registered through examples/ and tests/")
endif()

message(STATUS "T043 completion artifacts are registered")
