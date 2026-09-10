cmake_minimum_required(VERSION 3.24)
project(NativeUI VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

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

if(NATIVEUI_BUILD_EXAMPLES AND NOT NATIVEUI_BUILD_PLATFORM)
  message(FATAL_ERROR "NATIVEUI_BUILD_EXAMPLES requires NATIVEUI_BUILD_PLATFORM=ON")
endif()

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)
include(cmake/NativeUIConsumerPlatform.cmake)
include(cmake/NativeUIAttachPlatform.cmake)

# T023 consumes the SVG module shipped beside the pinned skia-builder archive.
# Skia's SVG DOM references text shaping even for SVGs without text nodes, and
# skshaper in turn references the packaged SkUnicode ICU implementation. Keep
# the Unix static-link dependency order from users toward providers.
find_library(NATIVEUI_SKIA_SVG_LIBRARY
  NAMES svg
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_SHAPER_LIBRARY
  NAMES skshaper
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_UNICODE_ICU_LIBRARY
  NAMES skunicode_icu
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_UNICODE_CORE_LIBRARY
  NAMES skunicode_core
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  REQUIRED
)
add_library(SkiaBuilder::svg STATIC IMPORTED GLOBAL)
set_target_properties(SkiaBuilder::svg PROPERTIES
  IMPORTED_LOCATION "${NATIVEUI_SKIA_SVG_LIBRARY}"
)
set_property(TARGET SkiaBuilder::svg APPEND PROPERTY INTERFACE_LINK_LIBRARIES
  "${NATIVEUI_SKIA_SHAPER_LIBRARY};${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY};${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY};SkiaBuilder::skia")

# Stable file names are baked into the generated package configs while every
# path remains relative to the package prefix in the installed configuration.
get_filename_component(NATIVEUI_SKIA_MAIN_FILENAME "${_skia_lib}" NAME)
get_filename_component(NATIVEUI_SKIA_SVG_FILENAME "${NATIVEUI_SKIA_SVG_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_SHAPER_FILENAME "${NATIVEUI_SKIA_SHAPER_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_UNICODE_ICU_FILENAME "${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_UNICODE_CORE_FILENAME "${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY}" NAME)

set(NATIVEUI_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/NativeUI")
set(NATIVEUI_INSTALL_SKIA_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/skia")
set(NATIVEUI_INSTALL_PUGL_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/pugl")
set(NATIVEUI_INSTALL_SOURCE_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/source")

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

# Core runtime + Skia-only services. This target deliberately has no Pugl or
# OpenGL dependency and is compiled once regardless of final consumer identity.
add_library(nativeui_core STATIC
  src/skia_core.cpp
  src/skia_image.cpp
  src/skia_svg.cpp
  src/headless.cpp
  src/dispatcher.cpp
)
add_library(NativeUI::Core ALIAS nativeui_core)
set_target_properties(nativeui_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  EXPORT_NAME Core
)
target_include_directories(nativeui_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(nativeui_core PUBLIC cxx_std_20)
target_link_libraries(nativeui_core PUBLIC SkiaBuilder::svg)
nativeui_enable_project_warnings(nativeui_core)
nativeui_enable_sanitizers(nativeui_core)

if(NATIVEUI_BUILD_PLATFORM)
  # On macOS the Pugl C core is generic while Objective-C sources are compiled
  # later by _nativeui_attach_consumer_platform() for each final target. On
  # Windows/Linux the existing generic Pugl target remains unchanged.
  if(APPLE)
    _nativeui_prepare_macos_platform_common()
  endif()

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
    target_link_libraries(nativeui
      PUBLIC NativeUI::Core
      PRIVATE nativeui_pugl_common NativeUI::OpenGL
    )
  else()
    target_link_libraries(nativeui
      PUBLIC NativeUI::Core
      PRIVATE NativeUI::Pugl NativeUI::OpenGL
    )
  endif()
  nativeui_enable_project_warnings(nativeui)
  nativeui_enable_sanitizers(nativeui)
endif()

# T047 package/export contract. Only the generic Core library is exported.
# Platform implementation sources remain private package machinery compiled by
# nativeui_attach_platform() into each final consumer as required by T053.
install(TARGETS nativeui_core
  EXPORT NativeUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(DIRECTORY include/nativeui
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(EXPORT NativeUITargets
  FILE NativeUITargets.cmake
  NAMESPACE NativeUI::
  DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
)

set(_nativeui_install_package_dir "${CMAKE_CURRENT_BINARY_DIR}/nativeui-install-package")
file(MAKE_DIRECTORY "${_nativeui_install_package_dir}")
configure_package_config_file(
  cmake/NativeUIConfig.cmake.in
  "${_nativeui_install_package_dir}/NativeUIConfig.cmake"
  INSTALL_DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
  PATH_VARS
    NATIVEUI_INSTALL_SKIA_DIR
    NATIVEUI_INSTALL_PUGL_DIR
    NATIVEUI_INSTALL_SOURCE_DIR
)
write_basic_package_version_file(
  "${_nativeui_install_package_dir}/NativeUIConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${_nativeui_install_package_dir}/NativeUIConfig.cmake"
  "${_nativeui_install_package_dir}/NativeUIConfigVersion.cmake"
  cmake/NativeUIConsumerPlatform.cmake
  cmake/NativeUIAttachPlatform.cmake
  cmake/NativeUIApplication.cmake
  cmake/NativeUIBinaryData.cmake
  cmake/NativeUIEmbedResource.cmake
  DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
)

# Preserve the exact pinned Skia binaries/headers in the package. This avoids a
# consumer silently resolving an ABI-incompatible system Skia.
install(DIRECTORY "${_skia_include}/"
  DESTINATION "${NATIVEUI_INSTALL_SKIA_DIR}/include"
)
install(FILES
  "${_skia_lib}"
  "${NATIVEUI_SKIA_SVG_LIBRARY}"
  "${NATIVEUI_SKIA_SHAPER_LIBRARY}"
  "${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY}"
  "${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY}"
  DESTINATION "${NATIVEUI_INSTALL_SKIA_DIR}/lib"
)

if(NATIVEUI_BUILD_PLATFORM)
  # Pugl is a source dependency by design: the final consumer owns its platform
  # backend, and macOS needs separately compiled Objective-C runtime names.
  install(DIRECTORY "${NATIVEUI_PUGL_SOURCE_DIR}/include/"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}/include"
  )
  install(DIRECTORY "${NATIVEUI_PUGL_SOURCE_DIR}/src/"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}/src"
  )
  install(FILES "${NATIVEUI_PUGL_SOURCE_DIR}/COPYING"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}"
    OPTIONAL
  )
  install(FILES src/pugl_skia.cpp
    DESTINATION "${NATIVEUI_INSTALL_SOURCE_DIR}/src"
  )
  install(DIRECTORY src/detail/
    DESTINATION "${NATIVEUI_INSTALL_SOURCE_DIR}/src/detail"
  )
endif()

# Build-tree package parity: expose the same NativeUI::Core target and public
# nativeui_attach_platform() helper to an external CMake project without install.
set(_nativeui_build_package_dir "${CMAKE_CURRENT_BINARY_DIR}/nativeui-package")
file(MAKE_DIRECTORY "${_nativeui_build_package_dir}")
export(TARGETS nativeui_core
  FILE "${_nativeui_build_package_dir}/NativeUITargets.cmake"
  NAMESPACE NativeUI::
)
configure_file(
  cmake/NativeUIBuildTreeConfig.cmake.in
  "${_nativeui_build_package_dir}/NativeUIConfig.cmake"
  @ONLY
)
write_basic_package_version_file(
  "${_nativeui_build_package_dir}/NativeUIConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_file(cmake/NativeUIConsumerPlatform.cmake
  "${_nativeui_build_package_dir}/NativeUIConsumerPlatform.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIAttachPlatform.cmake
  "${_nativeui_build_package_dir}/NativeUIAttachPlatform.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIApplication.cmake
  "${_nativeui_build_package_dir}/NativeUIApplication.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIBinaryData.cmake
  "${_nativeui_build_package_dir}/NativeUIBinaryData.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIEmbedResource.cmake
  "${_nativeui_build_package_dir}/NativeUIEmbedResource.cmake"
  COPYONLY
)

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
  t023_svg_icons
  t024_goldens
  t025_text_edit_model
  t026_label
  t027_fonts
  t028_text_area
  t029_ime_composition
  t030_button
  t031_checkbox_radio
  t032_slider
  t033_progress_meter
  t034_scroll_view
  t036_list_tabs
  t037_theme
  t041_smoke_harness
  t057_embedded_resources
  t058_dynamic_composition
  t059_component_state
  t067_virtual_list
)

if(NATIVEUI_BUILD_EXAMPLES)
  add_executable(nativeui_demo examples/standalone.cpp)
  _nativeui_attach_consumer_platform(
    TARGET nativeui_demo
    CONSUMER_ID org.nativeui.demo
  )
  nativeui_enable_sanitizers(nativeui_demo)

  function(nativeui_add_feature_example name)
    add_executable(nativeui_example_${name} examples/features/${name}.cpp)
    target_include_directories(nativeui_example_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/features)
    string(REPLACE "_" "-" _consumer_name "${name}")
    _nativeui_attach_consumer_platform(
      TARGET nativeui_example_${name}
      CONSUMER_ID "org.nativeui.example.${_consumer_name}"
    )
    nativeui_enable_project_warnings(nativeui_example_${name})
    nativeui_enable_sanitizers(nativeui_example_${name})
  endfunction()

  foreach(_example IN LISTS NATIVEUI_FEATURE_EXAMPLES)
    nativeui_add_feature_example(${_example})
  endforeach()
endif()

if(NATIVEUI_BUILD_TESTS)
  enable_testing()

  add_test(
    NAME nativeui_t053_prefix_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t053_prefix_tests.cmake"
  )
  add_test(
    NAME nativeui_t053_registration_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t053_registration_tests.cmake"
  )
  add_test(
    NAME nativeui_t047_attach_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t047_attach_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_application_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t054_application_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_argument_boundary_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t054_argument_boundary_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_package_source_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t054_package_source_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t065_root_integration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t065_root_integration_contract.cmake"
  )
  add_test(
    NAME nativeui_t067_root_integration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/t067_root_integration_contract.cmake"
  )
  set_tests_properties(
    nativeui_t053_prefix_tests
    nativeui_t053_registration_tests
    PROPERTIES LABELS "build;cmake;t053" TIMEOUT 20
  )
  set_tests_properties(
    nativeui_t047_attach_contract_tests
    PROPERTIES LABELS "build;cmake;t047" TIMEOUT 60
  )
  set_tests_properties(
    nativeui_t054_application_contract_tests
    nativeui_t054_argument_boundary_tests
    nativeui_t054_package_source_contract_tests
    PROPERTIES LABELS "build;cmake;t054" TIMEOUT 60
  )
  set_tests_properties(
    nativeui_t065_root_integration_contract
    PROPERTIES LABELS "build;cmake;t065" TIMEOUT 20
  )
  set_tests_properties(
    nativeui_t067_root_integration_contract
    PROPERTIES LABELS "build;cmake;t067" TIMEOUT 20
  )

  function(nativeui_add_core_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(${name})
    nativeui_enable_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
  endfunction()

  nativeui_add_core_test(nativeui_dispatcher_tests tests/dispatcher_tests.cpp)
  nativeui_add_core_test(nativeui_dispatcher_edge_tests tests/dispatcher_edge_tests.cpp)
  nativeui_add_core_test(nativeui_state_tests tests/state_tests.cpp)
  nativeui_add_core_test(nativeui_component_state_tests tests/component_state_tests.cpp)
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
  nativeui_add_core_test(nativeui_svg_tests tests/svg_tests.cpp)
  nativeui_add_core_test(nativeui_resource_manager_tests tests/resource_manager_tests.cpp)
  nativeui_add_core_test(nativeui_layout_constraints_tests tests/layout_constraints_tests.cpp)
  nativeui_add_core_test(nativeui_layout_alignment_tests tests/layout_alignment_tests.cpp)
  nativeui_add_core_test(nativeui_layout_flex_tests tests/layout_flex_tests.cpp)
  nativeui_add_core_test(nativeui_grid_layout_tests tests/grid_layout_tests.cpp)
  nativeui_add_core_test(nativeui_clipping_tests tests/clipping_tests.cpp)
  nativeui_add_core_test(nativeui_scroll_layout_tests tests/scroll_layout_tests.cpp)
  nativeui_add_core_test(nativeui_t034_scroll_view_tests tests/t034_scroll_view_tests.cpp)
  nativeui_add_core_test(nativeui_t036_list_tabs_tests tests/t036_list_tabs_tests.cpp)
  nativeui_add_core_test(nativeui_t067_virtual_list_contract tests/t067_virtual_list_contract.cpp)
  nativeui_add_core_test(nativeui_t067_virtual_list_window_contract tests/t067_virtual_list_window_contract.cpp)
  nativeui_add_core_test(nativeui_t067_retained_tests tests/t067_retained_tests.cpp)
  nativeui_add_core_test(nativeui_t067_public_api_tests tests/t067_public_api_tests.cpp)
  nativeui_add_core_test(nativeui_t067_semantic_api_tests tests/t067_semantic_api_tests.cpp)
  nativeui_add_core_test(nativeui_t067_visual_tests tests/t067_visual_tests.cpp)
  nativeui_add_core_test(nativeui_canvas_tests tests/canvas_tests.cpp)
  nativeui_add_core_test(nativeui_lifecycle_tests tests/lifecycle_tests.cpp)
  nativeui_add_core_test(nativeui_dynamic_composition_tests tests/dynamic_composition_tests.cpp)
  nativeui_add_core_test(nativeui_widget_tests tests/widget_tests.cpp)
  nativeui_add_core_test(nativeui_button_tests tests/button_tests.cpp)
  nativeui_add_core_test(nativeui_checkbox_radio_tests tests/checkbox_radio_tests.cpp)
  nativeui_add_core_test(nativeui_t032_slider_value_contract tests/t032_slider_value_contract.cpp)
  nativeui_add_core_test(nativeui_t032_slider_completion_tests tests/t032_slider_completion_tests.cpp)
  nativeui_add_core_test(nativeui_t032_slider_visual_tests tests/t032_slider_visual_tests.cpp)
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
    _nativeui_attach_consumer_platform(
      TARGET nativeui_smoke_standalone
      CONSUMER_ID org.nativeui.test.smoke-standalone
      OUT_BRIDGE _nativeui_smoke_standalone_bridge
    )
    nativeui_enable_project_warnings(nativeui_smoke_standalone)
    nativeui_enable_sanitizers(nativeui_smoke_standalone)

    add_executable(nativeui_smoke_embedded tests/smoke_embedded.cpp)
    _nativeui_attach_consumer_platform(
      TARGET nativeui_smoke_embedded
      CONSUMER_ID org.nativeui.test.smoke-embedded
      OUT_BRIDGE _nativeui_smoke_embedded_bridge
    )
    nativeui_enable_project_warnings(nativeui_smoke_embedded)
    nativeui_enable_sanitizers(nativeui_smoke_embedded)

    if(APPLE)
      enable_language(OBJCXX)
      add_executable(nativeui_smoke_macos_drop tests/smoke_macos_drop.mm)
      set_target_properties(nativeui_smoke_macos_drop PROPERTIES OBJCXX_STANDARD 20)
      target_compile_options(nativeui_smoke_macos_drop PRIVATE -fobjc-arc)
      _nativeui_attach_consumer_platform(
        TARGET nativeui_smoke_macos_drop
        CONSUMER_ID org.nativeui.test.macos-drop
      )
      get_target_property(_nativeui_drop_prefix nativeui_smoke_macos_drop NATIVEUI_OBJC_RUNTIME_PREFIX)
      target_compile_definitions(nativeui_smoke_macos_drop PRIVATE
        "NativeUIDropTestDraggingInfo=${_nativeui_drop_prefix}DropTestDraggingInfo")
      nativeui_enable_project_warnings(nativeui_smoke_macos_drop)
      nativeui_enable_sanitizers(nativeui_smoke_macos_drop)
      add_test(NAME nativeui_smoke_macos_drop COMMAND nativeui_smoke_macos_drop)
      set_tests_properties(nativeui_smoke_macos_drop PROPERTIES
        LABELS "integration;platform;macos;drop"
        TIMEOUT 20
      )

      nativeui_compute_objc_runtime_prefix(
        _nativeui_smoke_embedded_prefix
        org.nativeui.test.smoke-embedded
      )
      add_test(
        NAME nativeui_objc_runtime_prefix
        COMMAND ${CMAKE_COMMAND}
          "-DARCHIVE=$<TARGET_FILE:${_nativeui_smoke_embedded_bridge}>"
          "-DNM=${CMAKE_NM}"
          "-DPREFIX=${_nativeui_smoke_embedded_prefix}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
      )
      set_tests_properties(nativeui_objc_runtime_prefix PROPERTIES
        LABELS "integration;platform;macos;objc-runtime;t053"
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
  foreach(_header IN ITEMS geometry constraints invalidation input gesture state dispatcher text_edit text theme path resource embedded_resource resource_manager image svg paint_style paint component component_state command focus layout virtual_list widgets ui headless window nativeui)
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
