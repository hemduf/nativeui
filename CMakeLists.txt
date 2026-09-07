cmake_minimum_required(VERSION 3.24)
project(NativeUI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NATIVEUI_BUILD_PLATFORM "Build Pugl/OpenGL windowing integration" ON)
option(NATIVEUI_BUILD_EXAMPLES "Build NativeUI examples" ON)
option(NATIVEUI_BUILD_TESTS "Build NativeUI core tests" ON)
option(NATIVEUI_ENABLE_SANITIZERS "Enable ASan+UBSan on NativeUI-owned code/tests" OFF)
option(NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS "Register native standalone/embedded smoke tests with CTest" OFF)
set(NATIVEUI_OBJC_RUNTIME_PREFIX "" CACHE STRING
    "Consumer/plugin-specific Objective-C runtime prefix required for macOS platform builds")

if(NATIVEUI_BUILD_EXAMPLES AND NOT NATIVEUI_BUILD_PLATFORM)
  message(FATAL_ERROR "NATIVEUI_BUILD_EXAMPLES requires NATIVEUI_BUILD_PLATFORM=ON")
endif()

if(APPLE AND NATIVEUI_BUILD_PLATFORM)
  if(NATIVEUI_OBJC_RUNTIME_PREFIX STREQUAL "")
    message(FATAL_ERROR
      "macOS platform builds require -DNATIVEUI_OBJC_RUNTIME_PREFIX=<unique_prefix>. "
      "The prefix must be unique to the final application/plugin binary (normally derived from its bundle identifier). "
      "A generic NativeUI/Pugl prefix is not safe when the static library is embedded in multiple plug-ins loaded by one host.")
  endif()
  if(NOT NATIVEUI_OBJC_RUNTIME_PREFIX MATCHES "^[A-Za-z_][A-Za-z0-9_]*$")
    message(FATAL_ERROR
      "NATIVEUI_OBJC_RUNTIME_PREFIX must be a valid Objective-C identifier prefix: ${NATIVEUI_OBJC_RUNTIME_PREFIX}")
  endif()
endif()

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)

if(APPLE AND NATIVEUI_BUILD_PLATFORM)
  # Pugl's pinned macOS implementation defines fixed Objective-C classes
  # PuglWindow, PuglWrapperView and PuglOpenGLView. Objective-C runtime class
  # names are process-global, while NativeUI/Pugl is linked statically and can
  # therefore be duplicated across several plug-in bundles in one host. Rename
  # the private Pugl runtime classes at preprocessing time with a prefix that is
  # unique to the final consumer binary.
  target_compile_definitions(nativeui_pugl PRIVATE
    "PuglWindow=${NATIVEUI_OBJC_RUNTIME_PREFIX}PuglWindow"
    "PuglWrapperView=${NATIVEUI_OBJC_RUNTIME_PREFIX}PuglWrapperView"
    "PuglOpenGLView=${NATIVEUI_OBJC_RUNTIME_PREFIX}PuglOpenGLView"
  )
endif()

function(nativeui_enable_project_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(nativeui_enable_sanitizers target)
  if(NOT NATIVEUI_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # The pinned skia-builder binaries are consumed as a third-party ABI
    # boundary. UBSan's vptr instrumentation emits RTTI references for Skia
    # polymorphic types which these prebuilt archives do not provide. Disable
    # only that check while retaining ASan and the remaining UBSan coverage.
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-sanitize=vptr
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fsanitize=address)
    message(WARNING "MSVC sanitizer mode enables ASan; UBSan is unavailable with MSVC")
  else()
    message(FATAL_ERROR "NATIVEUI_ENABLE_SANITIZERS is unsupported by ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()

# Core runtime + Skia-only services.  This target deliberately has no Pugl or
# OpenGL dependency so retained-mode behavior can be tested without a window.
add_library(nativeui_core STATIC
  src/skia_core.cpp
  src/skia_image.cpp
  src/headless.cpp
)
add_library(NativeUI::Core ALIAS nativeui_core)
set_target_properties(nativeui_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(nativeui_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(nativeui_core PUBLIC cxx_std_20)
target_link_libraries(nativeui_core PUBLIC SkiaBuilder::skia)
nativeui_enable_project_warnings(nativeui_core)
nativeui_enable_sanitizers(nativeui_core)

if(NATIVEUI_BUILD_PLATFORM)
  add_library(nativeui STATIC
    src/pugl_skia.cpp
  )
  add_library(NativeUI::NativeUI ALIAS nativeui)
  set_target_properties(nativeui PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_features(nativeui PUBLIC cxx_std_20)
  target_compile_definitions(nativeui PRIVATE SK_GL)
  if(WIN32)
    # windows.h defines function-like min/max macros unless NOMINMAX is set.
    # The C++ platform bridge intentionally uses std::min/std::max throughout;
    # keep the Win32 macros out of this target instead of rewriting portable
    # code around macro expansion. Pugl's C target is intentionally unchanged.
    target_compile_definitions(nativeui PRIVATE NOMINMAX)
  endif()
  if(APPLE)
    target_compile_definitions(nativeui PRIVATE GL_SILENCE_DEPRECATION)
  endif()
  target_link_libraries(nativeui
    PUBLIC
      NativeUI::Core
    PRIVATE
      NativeUI::Pugl
      NativeUI::OpenGL
  )
  nativeui_enable_project_warnings(nativeui)
  nativeui_enable_sanitizers(nativeui)
endif()

set(NATIVEUI_FEATURE_EXAMPLES
  t007_constraints
  t008_alignment
  t009_flex
  t010_grid
  t011_clipping
  t012_scroll
  t013_bubbling
  t014_focus_scopes
  t015_pointer_capture
  t016_gestures
  t017_commands
  t018_drop
  t019_transforms
  t020_paths
  t021_gradients
  t022_images
  t024_goldens
  t025_text_edit_model
  t026_label
  t027_fonts
  t028_text_area
  t041_smoke_harness
)

if(NATIVEUI_BUILD_EXAMPLES)
  add_executable(nativeui_demo examples/standalone.cpp)
  target_link_libraries(nativeui_demo PRIVATE NativeUI::NativeUI)
  nativeui_enable_sanitizers(nativeui_demo)

  function(nativeui_add_feature_example name)
    add_executable(nativeui_example_${name} examples/features/${name}.cpp)
    target_include_directories(nativeui_example_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/features)
    target_link_libraries(nativeui_example_${name} PRIVATE NativeUI::NativeUI)
    nativeui_enable_project_warnings(nativeui_example_${name})
    nativeui_enable_sanitizers(nativeui_example_${name})
  endfunction()

  foreach(_example IN LISTS NATIVEUI_FEATURE_EXAMPLES)
    nativeui_add_feature_example(${_example})
  endforeach()
endif()

if(NATIVEUI_BUILD_TESTS)
  enable_testing()

  function(nativeui_add_core_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(${name})
    nativeui_enable_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
  endfunction()

  nativeui_add_core_test(nativeui_state_tests tests/state_tests.cpp)
  nativeui_add_core_test(nativeui_focus_tests tests/focus_tests.cpp)
  nativeui_add_core_test(nativeui_routing_tests tests/routing_tests.cpp)
  nativeui_add_core_test(nativeui_pointer_capture_tests tests/pointer_capture_tests.cpp)
  nativeui_add_core_test(nativeui_gesture_tests tests/gesture_tests.cpp)
  nativeui_add_core_test(nativeui_command_tests tests/command_tests.cpp)
  nativeui_add_core_test(nativeui_drop_tests tests/drop_tests.cpp)
  nativeui_add_core_test(nativeui_transform_tests tests/transform_tests.cpp)
  nativeui_add_core_test(nativeui_path_tests tests/path_tests.cpp)
  nativeui_add_core_test(nativeui_paint_style_tests tests/paint_style_tests.cpp)
  nativeui_add_core_test(nativeui_golden_tests tests/golden_tests.cpp)
  nativeui_add_core_test(nativeui_gradient_golden_tests tests/gradient_golden_tests.cpp)
  target_compile_definitions(nativeui_golden_tests PRIVATE
    NATIVEUI_GOLDEN_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/baselines"
    NATIVEUI_GOLDEN_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/golden-artifacts"
  )
  target_compile_definitions(nativeui_gradient_golden_tests PRIVATE
    NATIVEUI_GOLDEN_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/baselines"
    NATIVEUI_GOLDEN_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/golden-artifacts"
  )
  add_custom_target(nativeui_update_goldens
    COMMAND $<TARGET_FILE:nativeui_golden_tests> --update-goldens
    COMMAND $<TARGET_FILE:nativeui_gradient_golden_tests> --update-goldens
    DEPENDS nativeui_golden_tests nativeui_gradient_golden_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Explicitly update NativeUI golden image baselines"
  )
  nativeui_add_core_test(nativeui_invalidation_tests tests/invalidation_tests.cpp)
  nativeui_add_core_test(nativeui_headless_tests tests/headless_tests.cpp)
  nativeui_add_core_test(nativeui_image_tests tests/image_tests.cpp)
  nativeui_add_core_test(nativeui_layout_constraints_tests tests/layout_constraints_tests.cpp)
  nativeui_add_core_test(nativeui_layout_alignment_tests tests/layout_alignment_tests.cpp)
  nativeui_add_core_test(nativeui_layout_flex_tests tests/layout_flex_tests.cpp)
  nativeui_add_core_test(nativeui_grid_layout_tests tests/grid_layout_tests.cpp)
  nativeui_add_core_test(nativeui_clipping_tests tests/clipping_tests.cpp)
  nativeui_add_core_test(nativeui_scroll_layout_tests tests/scroll_layout_tests.cpp)
  nativeui_add_core_test(nativeui_canvas_tests tests/canvas_tests.cpp)
  nativeui_add_core_test(nativeui_lifecycle_tests tests/lifecycle_tests.cpp)
  nativeui_add_core_test(nativeui_widget_tests tests/widget_tests.cpp)
  nativeui_add_core_test(nativeui_text_input_tests tests/text_input_tests.cpp)
  nativeui_add_core_test(nativeui_text_area_tests tests/text_area_tests.cpp)
  nativeui_add_core_test(nativeui_text_edit_model_tests tests/text_edit_model_tests.cpp)
  nativeui_add_core_test(nativeui_label_tests tests/label_tests.cpp)
  nativeui_add_core_test(nativeui_font_tests tests/font_tests.cpp)
  target_compile_definitions(nativeui_font_tests PRIVATE
    NATIVEUI_TEST_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/resources"
  )

  if(NATIVEUI_BUILD_PLATFORM)
    add_executable(nativeui_smoke_standalone tests/smoke_standalone.cpp)
    target_link_libraries(nativeui_smoke_standalone PRIVATE NativeUI::NativeUI)
    nativeui_enable_project_warnings(nativeui_smoke_standalone)
    nativeui_enable_sanitizers(nativeui_smoke_standalone)

    add_executable(nativeui_smoke_embedded tests/smoke_embedded.cpp)
    target_link_libraries(nativeui_smoke_embedded PRIVATE NativeUI::NativeUI)
    nativeui_enable_project_warnings(nativeui_smoke_embedded)
    nativeui_enable_sanitizers(nativeui_smoke_embedded)

    if(APPLE)
      add_test(
        NAME nativeui_objc_runtime_prefix
        COMMAND ${CMAKE_COMMAND}
          "-DARCHIVE=$<TARGET_FILE:nativeui_pugl>"
          "-DNM=${CMAKE_NM}"
          "-DPREFIX=${NATIVEUI_OBJC_RUNTIME_PREFIX}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
      )
      set_tests_properties(nativeui_objc_runtime_prefix PROPERTIES
        LABELS "integration;platform;macos;objc-runtime"
        TIMEOUT 20
      )
    endif()

    if(NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS)
      add_test(NAME nativeui_smoke_standalone COMMAND nativeui_smoke_standalone)
      add_test(NAME nativeui_smoke_embedded COMMAND nativeui_smoke_embedded)
      set_tests_properties(nativeui_smoke_standalone nativeui_smoke_embedded PROPERTIES
        LABELS "integration;platform;smoke"
        TIMEOUT 20
      )
    endif()
  endif()

  # Each public header gets its own translation unit so missing direct includes
  # cannot be accidentally masked by umbrella include order.
  foreach(_header IN ITEMS geometry constraints invalidation input gesture state text_edit text path image paint_style paint component command focus layout widgets ui headless window nativeui)
    add_library(nativeui_header_${_header}_compile OBJECT tests/headers/${_header}.cpp)
    target_link_libraries(nativeui_header_${_header}_compile PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(nativeui_header_${_header}_compile)
    nativeui_enable_sanitizers(nativeui_header_${_header}_compile)
  endforeach()

  # Every completed feature ships an executable example. Compile every
  # example source against the core even in platform-less CI so public API
  # regressions are caught without requiring a display server.
  foreach(_example IN LISTS NATIVEUI_FEATURE_EXAMPLES)
    add_library(nativeui_feature_example_${_example}_compile OBJECT examples/features/${_example}.cpp)
    target_include_directories(nativeui_feature_example_${_example}_compile PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/features)
    target_link_libraries(nativeui_feature_example_${_example}_compile PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(nativeui_feature_example_${_example}_compile)
    nativeui_enable_sanitizers(nativeui_feature_example_${_example}_compile)

    if(NATIVEUI_BUILD_EXAMPLES AND NATIVEUI_BUILD_PLATFORM)
      add_test(NAME nativeui_example_${_example}_self_test
               COMMAND nativeui_example_${_example} --self-test)
    endif()
  endforeach()

  # Compile the existing demo source against the umbrella header without
  # linking a platform backend. This guards the public source-level API.
  add_library(nativeui_demo_consumer_compile OBJECT examples/standalone.cpp)
  target_link_libraries(nativeui_demo_consumer_compile PRIVATE NativeUI::Core)
  nativeui_enable_project_warnings(nativeui_demo_consumer_compile)
  nativeui_enable_sanitizers(nativeui_demo_consumer_compile)
endif()
