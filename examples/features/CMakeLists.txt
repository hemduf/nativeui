include("${PROJECT_SOURCE_DIR}/cmake/NativeUIFeatureExamples.cmake")
nativeui_discover_feature_examples(NATIVEUI_FEATURE_EXAMPLES "${PROJECT_SOURCE_DIR}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

if(NATIVEUI_BUILD_EXAMPLES AND NATIVEUI_BUILD_FEATURE_EXAMPLES)
  include("${PROJECT_SOURCE_DIR}/cmake/NativeUIApplication.cmake")
endif()

foreach(_example IN LISTS NATIVEUI_FEATURE_EXAMPLES)
  if(NATIVEUI_BUILD_EXAMPLES AND NATIVEUI_BUILD_FEATURE_EXAMPLES)
    string(REPLACE "_" "-" _consumer_name "${_example}")
    nativeui_add_application(nativeui_example_${_example}
      PRODUCT_NAME nativeui_example_${_example}
      BUNDLE_ID "org.nativeui.example.${_consumer_name}"
      VERSION "${PROJECT_VERSION}"
      SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_example}.cpp")
    target_include_directories(nativeui_example_${_example} PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}")
    if(WIN32)
      # Feature self-tests use main() and need a console entry point.
      set_target_properties(nativeui_example_${_example} PROPERTIES
        WIN32_EXECUTABLE FALSE)
    endif()
    nativeui_enable_project_warnings(nativeui_example_${_example})
    nativeui_enable_sanitizers(nativeui_example_${_example})
  elseif(NATIVEUI_BUILD_TESTS)
    # Headless CI still compiles every feature source against the public API.
    add_library(nativeui_feature_example_${_example}_compile OBJECT
      "${CMAKE_CURRENT_SOURCE_DIR}/${_example}.cpp")
    target_include_directories(nativeui_feature_example_${_example}_compile PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(nativeui_feature_example_${_example}_compile PRIVATE
      NativeUI::Core)
    nativeui_enable_project_warnings(nativeui_feature_example_${_example}_compile)
    nativeui_enable_sanitizers(nativeui_feature_example_${_example}_compile)
  endif()

  if(NATIVEUI_BUILD_TESTS AND NATIVEUI_BUILD_EXAMPLES AND
     NATIVEUI_BUILD_FEATURE_EXAMPLES)
    add_test(NAME nativeui_example_${_example}_self_test
      COMMAND nativeui_example_${_example} --self-test)
    set_tests_properties(nativeui_example_${_example}_self_test PROPERTIES
      LABELS "example")
  endif()
endforeach()
