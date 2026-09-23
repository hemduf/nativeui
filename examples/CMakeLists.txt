set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

if(NATIVEUI_BUILD_EXAMPLES)
  add_executable(nativeui_demo standalone.cpp)
  _nativeui_attach_consumer_platform(
    TARGET nativeui_demo
    CONSUMER_ID org.nativeui.demo
  )
  nativeui_enable_sanitizers(nativeui_demo)
elseif(NATIVEUI_BUILD_TESTS)
  # Keep the demo's public Core API checked in display-less builds.
  add_library(nativeui_demo_consumer_compile OBJECT standalone.cpp)
  target_link_libraries(nativeui_demo_consumer_compile PRIVATE NativeUI::Core)
  nativeui_enable_project_warnings(nativeui_demo_consumer_compile)
  nativeui_enable_sanitizers(nativeui_demo_consumer_compile)
endif()

if(NATIVEUI_BUILD_TESTS OR
   (NATIVEUI_BUILD_EXAMPLES AND NATIVEUI_BUILD_FEATURE_EXAMPLES))
  add_subdirectory(features)
endif()
