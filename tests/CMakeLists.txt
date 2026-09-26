set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

if(NATIVEUI_BUILD_TESTS)
  add_test(
    NAME nativeui_feature_example_discovery_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/feature_example_discovery_contract.cmake"
  )
  add_test(
    NAME nativeui_t053_prefix_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t053_prefix_tests.cmake"
  )
  add_test(
    NAME nativeui_t053_registration_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t053_registration_tests.cmake"
  )
  add_test(
    NAME nativeui_t043_registration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t043_registration_contract.cmake"
  )
  add_test(
    NAME nativeui_t047_attach_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t047_attach_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_application_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t054_application_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_argument_boundary_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t054_argument_boundary_tests.cmake"
  )
  add_test(
    NAME nativeui_t054_package_source_contract_tests
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t054_package_source_contract_tests.cmake"
  )
  add_test(
    NAME nativeui_t064_root_integration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t064_root_integration_contract.cmake"
  )
  add_test(
    NAME nativeui_t065_root_integration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t065_root_integration_contract.cmake"
  )
  add_test(
    NAME nativeui_t067_root_integration_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t067_root_integration_contract.cmake"
  )
  add_test(
    NAME nativeui_t079_shader_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t079_shader_contract.cmake"
  )
  add_test(
    NAME nativeui_t080_shader_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t080_shader_contract.cmake"
  )
  add_test(
    NAME nativeui_t081_shader_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t081_shader_contract.cmake"
  )
  add_test(
    NAME nativeui_t082_shader_contract
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      -P "${PROJECT_SOURCE_DIR}/tests/t082_shader_contract.cmake"
  )
  # CMake-script contract tests shell out to nested configures/builds (or read
  # and hash source trees), so they get a timeout with enough headroom for slow
  # or degraded CI runners instead of being killed mid-configure.
  set_tests_properties(
    nativeui_feature_example_discovery_contract
    PROPERTIES LABELS "build;cmake;examples" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t053_prefix_tests
    nativeui_t053_registration_tests
    PROPERTIES LABELS "build;cmake;t053" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t043_registration_contract
    PROPERTIES LABELS "build;cmake;t043" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t047_attach_contract_tests
    PROPERTIES LABELS "build;cmake;package;t047" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t054_application_contract_tests
    nativeui_t054_argument_boundary_tests
    nativeui_t054_package_source_contract_tests
    PROPERTIES LABELS "build;cmake;package;t054" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t064_root_integration_contract
    PROPERTIES LABELS "build;cmake;t064" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t065_root_integration_contract
    PROPERTIES LABELS "build;cmake;t065" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t067_root_integration_contract
    PROPERTIES LABELS "build;cmake;t067" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t079_shader_contract
    PROPERTIES LABELS "build;cmake;t079;shader" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t080_shader_contract
    PROPERTIES LABELS "build;cmake;t080;shader" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t081_shader_contract
    PROPERTIES LABELS "build;cmake;t081;shader" TIMEOUT 300
  )
  set_tests_properties(
    nativeui_t082_shader_contract
    PROPERTIES LABELS "build;cmake;t082;shader" TIMEOUT 300
  )

  function(nativeui_add_core_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(${name})
    nativeui_enable_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "unit")
  endfunction()

  nativeui_add_core_test(nativeui_dispatcher_tests ${PROJECT_SOURCE_DIR}/tests/dispatcher_tests.cpp)
  nativeui_add_core_test(nativeui_dispatcher_edge_tests ${PROJECT_SOURCE_DIR}/tests/dispatcher_edge_tests.cpp)
  nativeui_add_core_test(nativeui_state_tests ${PROJECT_SOURCE_DIR}/tests/state_tests.cpp)
  nativeui_add_core_test(nativeui_component_state_tests ${PROJECT_SOURCE_DIR}/tests/component_state_tests.cpp)
  nativeui_add_core_test(nativeui_focus_tests ${PROJECT_SOURCE_DIR}/tests/focus_tests.cpp)
  nativeui_add_core_test(nativeui_routing_tests ${PROJECT_SOURCE_DIR}/tests/routing_tests.cpp)
  nativeui_add_core_test(nativeui_context_menu_input_tests ${PROJECT_SOURCE_DIR}/tests/context_menu_input_tests.cpp)
  nativeui_add_core_test(nativeui_t175_pugl_button_tests ${PROJECT_SOURCE_DIR}/tests/t175_pugl_button_tests.cpp)
  target_include_directories(nativeui_t175_pugl_button_tests PRIVATE ${PROJECT_SOURCE_DIR}/src)
  nativeui_add_core_test(nativeui_pointer_capture_tests ${PROJECT_SOURCE_DIR}/tests/pointer_capture_tests.cpp)
  nativeui_add_core_test(nativeui_multitouch_pointer_tests ${PROJECT_SOURCE_DIR}/tests/multitouch_pointer_tests.cpp)
  nativeui_add_core_test(nativeui_gesture_tests ${PROJECT_SOURCE_DIR}/tests/gesture_tests.cpp)
  nativeui_add_core_test(nativeui_command_tests ${PROJECT_SOURCE_DIR}/tests/command_tests.cpp)
  nativeui_add_core_test(nativeui_drop_tests ${PROJECT_SOURCE_DIR}/tests/drop_tests.cpp)
  nativeui_add_core_test(nativeui_transform_tests ${PROJECT_SOURCE_DIR}/tests/transform_tests.cpp)
  nativeui_add_core_test(nativeui_t098_transform_tracking_tests ${PROJECT_SOURCE_DIR}/tests/t098_transform_tracking_tests.cpp)
  nativeui_add_core_test(nativeui_path_tests ${PROJECT_SOURCE_DIR}/tests/path_tests.cpp)
  nativeui_add_core_test(nativeui_paint_style_tests ${PROJECT_SOURCE_DIR}/tests/paint_style_tests.cpp)
  nativeui_add_core_test(nativeui_t077_blur_tests ${PROJECT_SOURCE_DIR}/tests/t077_blur_tests.cpp)
  nativeui_add_core_test(nativeui_t078_drop_shadow_tests ${PROJECT_SOURCE_DIR}/tests/t078_drop_shadow_tests.cpp)
  nativeui_add_core_test(nativeui_t050_inspector_model_tests ${PROJECT_SOURCE_DIR}/tests/t050_inspector_model_tests.cpp)
  if(NATIVEUI_ENABLE_INSPECTOR)
    nativeui_add_core_test(nativeui_t050_inspector_runtime_tests ${PROJECT_SOURCE_DIR}/tests/t050_inspector_runtime_tests.cpp)
  endif()
  nativeui_add_core_test(nativeui_golden_tests ${PROJECT_SOURCE_DIR}/tests/golden_tests.cpp)
  nativeui_add_core_test(nativeui_gradient_golden_tests ${PROJECT_SOURCE_DIR}/tests/gradient_golden_tests.cpp)
  target_compile_definitions(nativeui_golden_tests PRIVATE
    NATIVEUI_GOLDEN_BASELINE_DIR="${PROJECT_SOURCE_DIR}/tests/golden/baselines"
    NATIVEUI_GOLDEN_ARTIFACT_DIR="${PROJECT_BINARY_DIR}/golden-artifacts"
  )
  target_compile_definitions(nativeui_gradient_golden_tests PRIVATE
    NATIVEUI_GOLDEN_BASELINE_DIR="${PROJECT_SOURCE_DIR}/tests/golden/baselines"
    NATIVEUI_GOLDEN_ARTIFACT_DIR="${PROJECT_BINARY_DIR}/golden-artifacts"
  )
  add_custom_target(nativeui_update_goldens
    COMMAND $<TARGET_FILE:nativeui_golden_tests> --update-goldens
    COMMAND $<TARGET_FILE:nativeui_gradient_golden_tests> --update-goldens
    DEPENDS nativeui_golden_tests nativeui_gradient_golden_tests
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Explicitly update NativeUI golden image baselines"
  )
  nativeui_add_core_test(nativeui_invalidation_tests ${PROJECT_SOURCE_DIR}/tests/invalidation_tests.cpp)
  nativeui_add_core_test(nativeui_paint_culling_tests ${PROJECT_SOURCE_DIR}/tests/paint_culling_tests.cpp)
  nativeui_add_core_test(nativeui_t096_damage_mapping_tests
    ${PROJECT_SOURCE_DIR}/tests/t096_damage_mapping_tests.cpp)
  target_include_directories(nativeui_t096_damage_mapping_tests PRIVATE
    "${PROJECT_SOURCE_DIR}"
  )
  nativeui_add_core_test(nativeui_headless_tests ${PROJECT_SOURCE_DIR}/tests/headless_tests.cpp)
  nativeui_add_core_test(nativeui_t061_overlay_acceptance_tests ${PROJECT_SOURCE_DIR}/tests/t061_overlay_acceptance_tests.cpp)
  nativeui_add_core_test(nativeui_t062_tooltip_tests ${PROJECT_SOURCE_DIR}/tests/t062_tooltip_tests.cpp)
  nativeui_add_core_test(nativeui_image_tests ${PROJECT_SOURCE_DIR}/tests/image_tests.cpp)
  nativeui_add_core_test(nativeui_t083_image_texture_tests ${PROJECT_SOURCE_DIR}/tests/t083_image_texture_tests.cpp)
  target_include_directories(nativeui_t083_image_texture_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
  )
  nativeui_add_core_test(nativeui_t084_image_texture_tiling_tests
    ${PROJECT_SOURCE_DIR}/tests/t084_image_texture_tiling_tests.cpp)
  nativeui_add_core_test(nativeui_t086_image_texture_transform_tests
    ${PROJECT_SOURCE_DIR}/tests/t086_image_texture_transform_tests.cpp)
  nativeui_add_core_test(nativeui_image_texture_interpretation_tests
    ${PROJECT_SOURCE_DIR}/tests/image_texture_interpretation_tests.cpp)

  # T083 value/materialization fault seams are compiled only into this isolated
  # executable. NativeUI::Core contains no mutable test instrumentation.
  add_executable(nativeui_t083_image_texture_fault_tests
    ${PROJECT_SOURCE_DIR}/tests/t083_image_texture_fault_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_image.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_shader.cpp
  )
  target_include_directories(nativeui_t083_image_texture_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(nativeui_t083_image_texture_fault_tests PRIVATE cxx_std_20)
  target_compile_definitions(nativeui_t083_image_texture_fault_tests PRIVATE
    NATIVEUI_ENABLE_TEST_SEAMS=1
  )
  target_link_libraries(nativeui_t083_image_texture_fault_tests PRIVATE SkiaBuilder::svg)
  nativeui_enable_project_warnings(nativeui_t083_image_texture_fault_tests)
  nativeui_enable_sanitizers(nativeui_t083_image_texture_fault_tests)
  add_test(
    NAME nativeui_t083_image_texture_fault_tests
    COMMAND nativeui_t083_image_texture_fault_tests
  )
  set_tests_properties(nativeui_t083_image_texture_fault_tests PROPERTIES LABELS "unit")

  nativeui_add_core_test(nativeui_shader_tests ${PROJECT_SOURCE_DIR}/tests/shader_tests.cpp)
  nativeui_add_core_test(nativeui_t088_noise_tests ${PROJECT_SOURCE_DIR}/tests/t088_noise_tests.cpp)
  target_include_directories(nativeui_t088_noise_tests PRIVATE ${PROJECT_SOURCE_DIR})
  nativeui_add_core_test(nativeui_t089_perlin_noise_tests ${PROJECT_SOURCE_DIR}/tests/t089_perlin_noise_tests.cpp)
  target_include_directories(nativeui_t089_perlin_noise_tests PRIVATE ${PROJECT_SOURCE_DIR})
  nativeui_add_core_test(nativeui_t090_simplex_noise_tests ${PROJECT_SOURCE_DIR}/tests/t090_simplex_noise_tests.cpp)
  target_include_directories(nativeui_t090_simplex_noise_tests PRIVATE ${PROJECT_SOURCE_DIR})

  # Fault controls remain in this isolated executable, never NativeUI::Core.
  add_executable(nativeui_t088_noise_fault_tests
    ${PROJECT_SOURCE_DIR}/tests/t088_noise_fault_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_noise.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_shader.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_image.cpp
  )
  target_include_directories(nativeui_t088_noise_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(nativeui_t088_noise_fault_tests PRIVATE cxx_std_20)
  target_compile_definitions(nativeui_t088_noise_fault_tests PRIVATE NATIVEUI_ENABLE_TEST_SEAMS=1)
  target_link_libraries(nativeui_t088_noise_fault_tests PRIVATE SkiaBuilder::svg)
  nativeui_enable_project_warnings(nativeui_t088_noise_fault_tests)
  nativeui_enable_sanitizers(nativeui_t088_noise_fault_tests)
  add_test(NAME nativeui_t088_noise_fault_tests COMMAND nativeui_t088_noise_fault_tests)
  set_tests_properties(nativeui_t088_noise_fault_tests PROPERTIES LABELS "unit")

  # Fault controls remain in this isolated executable, never NativeUI::Core.
  add_executable(nativeui_t089_perlin_noise_fault_tests
    ${PROJECT_SOURCE_DIR}/tests/t089_perlin_noise_fault_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_noise.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_shader.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_image.cpp
  )
  target_include_directories(nativeui_t089_perlin_noise_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(nativeui_t089_perlin_noise_fault_tests PRIVATE cxx_std_20)
  target_compile_definitions(nativeui_t089_perlin_noise_fault_tests PRIVATE NATIVEUI_ENABLE_TEST_SEAMS=1)
  target_link_libraries(nativeui_t089_perlin_noise_fault_tests PRIVATE SkiaBuilder::svg)
  nativeui_enable_project_warnings(nativeui_t089_perlin_noise_fault_tests)
  nativeui_enable_sanitizers(nativeui_t089_perlin_noise_fault_tests)
  add_test(NAME nativeui_t089_perlin_noise_fault_tests COMMAND nativeui_t089_perlin_noise_fault_tests)
  set_tests_properties(nativeui_t089_perlin_noise_fault_tests PROPERTIES LABELS "unit")

  # Fault controls remain in this isolated executable, never NativeUI::Core.
  add_executable(nativeui_t090_simplex_noise_fault_tests
    ${PROJECT_SOURCE_DIR}/tests/t090_simplex_noise_fault_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_noise.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_shader.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_image.cpp
  )
  target_include_directories(nativeui_t090_simplex_noise_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(nativeui_t090_simplex_noise_fault_tests PRIVATE cxx_std_20)
  target_compile_definitions(nativeui_t090_simplex_noise_fault_tests PRIVATE NATIVEUI_ENABLE_TEST_SEAMS=1)
  target_link_libraries(nativeui_t090_simplex_noise_fault_tests PRIVATE SkiaBuilder::svg)
  nativeui_enable_project_warnings(nativeui_t090_simplex_noise_fault_tests)
  nativeui_enable_sanitizers(nativeui_t090_simplex_noise_fault_tests)
  add_test(NAME nativeui_t090_simplex_noise_fault_tests COMMAND nativeui_t090_simplex_noise_fault_tests)
  set_tests_properties(nativeui_t090_simplex_noise_fault_tests PROPERTIES LABELS "unit")
  nativeui_add_core_test(nativeui_t081_shader_brush_tests ${PROJECT_SOURCE_DIR}/tests/t081_shader_brush_tests.cpp)
  target_include_directories(nativeui_t081_shader_brush_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
  )
  nativeui_add_core_test(nativeui_t082_shader_children_tests ${PROJECT_SOURCE_DIR}/tests/t082_shader_children_tests.cpp)
  target_include_directories(nativeui_t082_shader_children_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
  )

  # T079/T080/T081/T082 shader allocation/fallback/materialization fault seams are
  # compiled only into
  # this isolated test executable. NativeUI::Core never contains the test-only
  # entry point,
  # even when the normal test suite is enabled.
  add_executable(nativeui_shader_fault_tests
    ${PROJECT_SOURCE_DIR}/tests/shader_fault_tests.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_shader.cpp
    ${PROJECT_SOURCE_DIR}/src/skia_image.cpp
  )
  target_include_directories(nativeui_shader_fault_tests PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
  )
  target_compile_features(nativeui_shader_fault_tests PRIVATE cxx_std_20)
  target_compile_definitions(nativeui_shader_fault_tests PRIVATE
    NATIVEUI_ENABLE_TEST_SEAMS=1
  )
  target_link_libraries(nativeui_shader_fault_tests PRIVATE SkiaBuilder::svg)
  nativeui_enable_project_warnings(nativeui_shader_fault_tests)
  nativeui_enable_sanitizers(nativeui_shader_fault_tests)
  add_test(NAME nativeui_shader_fault_tests COMMAND nativeui_shader_fault_tests)
  set_tests_properties(nativeui_shader_fault_tests PROPERTIES LABELS "unit")

  nativeui_add_core_test(nativeui_svg_tests ${PROJECT_SOURCE_DIR}/tests/svg_tests.cpp)
  nativeui_add_core_test(nativeui_resource_manager_tests ${PROJECT_SOURCE_DIR}/tests/resource_manager_tests.cpp)
  nativeui_add_core_test(nativeui_layout_constraints_tests ${PROJECT_SOURCE_DIR}/tests/layout_constraints_tests.cpp)
  nativeui_add_core_test(nativeui_layout_alignment_tests ${PROJECT_SOURCE_DIR}/tests/layout_alignment_tests.cpp)
  nativeui_add_core_test(nativeui_layout_flex_tests ${PROJECT_SOURCE_DIR}/tests/layout_flex_tests.cpp)
  nativeui_add_core_test(nativeui_grid_layout_tests ${PROJECT_SOURCE_DIR}/tests/grid_layout_tests.cpp)
  nativeui_add_core_test(nativeui_clipping_tests ${PROJECT_SOURCE_DIR}/tests/clipping_tests.cpp)
  nativeui_add_core_test(nativeui_t075_scoped_clipping_tests ${PROJECT_SOURCE_DIR}/tests/t075_scoped_clipping_tests.cpp)
  nativeui_add_core_test(nativeui_scroll_layout_tests ${PROJECT_SOURCE_DIR}/tests/scroll_layout_tests.cpp)
  nativeui_add_core_test(nativeui_t034_scroll_view_tests ${PROJECT_SOURCE_DIR}/tests/t034_scroll_view_tests.cpp)
  nativeui_add_core_test(nativeui_t035_combo_popup_tests ${PROJECT_SOURCE_DIR}/tests/t035_combo_popup_tests.cpp)
  nativeui_add_core_test(nativeui_t036_list_tabs_tests ${PROJECT_SOURCE_DIR}/tests/t036_list_tabs_tests.cpp)
  nativeui_add_core_test(nativeui_t067_virtual_list_contract ${PROJECT_SOURCE_DIR}/tests/t067_virtual_list_contract.cpp)
  nativeui_add_core_test(nativeui_t067_virtual_list_window_contract ${PROJECT_SOURCE_DIR}/tests/t067_virtual_list_window_contract.cpp)
  nativeui_add_core_test(nativeui_t067_retained_tests ${PROJECT_SOURCE_DIR}/tests/t067_retained_tests.cpp)
  nativeui_add_core_test(nativeui_t067_public_api_tests ${PROJECT_SOURCE_DIR}/tests/t067_public_api_tests.cpp)
  nativeui_add_core_test(nativeui_t067_semantic_api_tests ${PROJECT_SOURCE_DIR}/tests/t067_semantic_api_tests.cpp)
  nativeui_add_core_test(nativeui_t067_visual_tests ${PROJECT_SOURCE_DIR}/tests/t067_visual_tests.cpp)
  nativeui_add_core_test(nativeui_canvas_tests ${PROJECT_SOURCE_DIR}/tests/canvas_tests.cpp)
  nativeui_add_core_test(nativeui_lifecycle_tests ${PROJECT_SOURCE_DIR}/tests/lifecycle_tests.cpp)
  nativeui_add_core_test(nativeui_t043_view_geometry_tests ${PROJECT_SOURCE_DIR}/tests/t043_view_geometry_tests.cpp)
  nativeui_add_core_test(nativeui_dynamic_composition_tests ${PROJECT_SOURCE_DIR}/tests/dynamic_composition_tests.cpp)
  nativeui_add_core_test(nativeui_widget_tests ${PROJECT_SOURCE_DIR}/tests/widget_tests.cpp)
  nativeui_add_core_test(nativeui_button_tests ${PROJECT_SOURCE_DIR}/tests/button_tests.cpp)
  nativeui_add_core_test(nativeui_checkbox_radio_tests ${PROJECT_SOURCE_DIR}/tests/checkbox_radio_tests.cpp)
  nativeui_add_core_test(nativeui_t032_slider_value_contract ${PROJECT_SOURCE_DIR}/tests/t032_slider_value_contract.cpp)
  nativeui_add_core_test(nativeui_t032_slider_completion_tests ${PROJECT_SOURCE_DIR}/tests/t032_slider_completion_tests.cpp)
  nativeui_add_core_test(nativeui_t032_slider_visual_tests ${PROJECT_SOURCE_DIR}/tests/t032_slider_visual_tests.cpp)
  nativeui_add_core_test(nativeui_text_input_tests ${PROJECT_SOURCE_DIR}/tests/text_input_tests.cpp)
  nativeui_add_core_test(nativeui_t095_scene_extent_tests ${PROJECT_SOURCE_DIR}/tests/t095_scene_extent_tests.cpp)
  target_include_directories(nativeui_t095_scene_extent_tests PRIVATE
    "${PROJECT_SOURCE_DIR}"
  )
  nativeui_add_core_test(nativeui_text_area_tests ${PROJECT_SOURCE_DIR}/tests/text_area_tests.cpp)
  nativeui_add_core_test(nativeui_text_edit_model_tests ${PROJECT_SOURCE_DIR}/tests/text_edit_model_tests.cpp)
  nativeui_add_core_test(nativeui_label_tests ${PROJECT_SOURCE_DIR}/tests/label_tests.cpp)
  nativeui_add_core_test(nativeui_font_tests ${PROJECT_SOURCE_DIR}/tests/font_tests.cpp)
  target_compile_definitions(nativeui_font_tests PRIVATE
    NATIVEUI_TEST_FONT_DIR="${PROJECT_SOURCE_DIR}/tests/resources"
  )

  if(NATIVEUI_BUILD_PLATFORM)
    nativeui_add_core_test(nativeui_pugl_pointer_tests ${PROJECT_SOURCE_DIR}/tests/pugl_pointer_tests.cpp)
    target_include_directories(nativeui_pugl_pointer_tests PRIVATE
      ${PROJECT_SOURCE_DIR}/src
      ${NATIVEUI_PUGL_SOURCE_DIR}/include
    )

    add_executable(nativeui_smoke_standalone ${PROJECT_SOURCE_DIR}/tests/smoke_standalone.cpp)
    _nativeui_attach_consumer_platform(
      TARGET nativeui_smoke_standalone
      CONSUMER_ID org.nativeui.test.smoke-standalone
      OUT_BRIDGE _nativeui_smoke_standalone_bridge
    )
    nativeui_enable_project_warnings(nativeui_smoke_standalone)
    nativeui_enable_sanitizers(nativeui_smoke_standalone)

    add_executable(nativeui_image_texture_gpu_reference_tests
      ${PROJECT_SOURCE_DIR}/tests/image_texture_gpu_reference_tests.cpp
    )
    target_include_directories(nativeui_image_texture_gpu_reference_tests PRIVATE
      "${PROJECT_SOURCE_DIR}"
    )
    _nativeui_attach_consumer_platform(
      TARGET nativeui_image_texture_gpu_reference_tests
      CONSUMER_ID org.nativeui.test.image-texture-gpu-reference
    )
    nativeui_enable_project_warnings(nativeui_image_texture_gpu_reference_tests)
    nativeui_enable_sanitizers(nativeui_image_texture_gpu_reference_tests)

    add_executable(nativeui_t088_noise_gpu_reference_tests
      ${PROJECT_SOURCE_DIR}/tests/t088_noise_gpu_reference_tests.cpp)
    target_include_directories(nativeui_t088_noise_gpu_reference_tests PRIVATE ${PROJECT_SOURCE_DIR})
    _nativeui_attach_consumer_platform(
      TARGET nativeui_t088_noise_gpu_reference_tests
      CONSUMER_ID org.nativeui.test.t088-noise-gpu-reference
    )
    nativeui_enable_project_warnings(nativeui_t088_noise_gpu_reference_tests)
    nativeui_enable_sanitizers(nativeui_t088_noise_gpu_reference_tests)

    add_executable(nativeui_t089_perlin_noise_gpu_reference_tests
      ${PROJECT_SOURCE_DIR}/tests/t089_perlin_noise_gpu_reference_tests.cpp)
    target_include_directories(nativeui_t089_perlin_noise_gpu_reference_tests PRIVATE ${PROJECT_SOURCE_DIR})
    _nativeui_attach_consumer_platform(
      TARGET nativeui_t089_perlin_noise_gpu_reference_tests
      CONSUMER_ID org.nativeui.test.t089-perlin-noise-gpu-reference
    )
    nativeui_enable_project_warnings(nativeui_t089_perlin_noise_gpu_reference_tests)
    nativeui_enable_sanitizers(nativeui_t089_perlin_noise_gpu_reference_tests)

    add_executable(nativeui_t090_simplex_noise_gpu_reference_tests
      ${PROJECT_SOURCE_DIR}/tests/t090_simplex_noise_gpu_reference_tests.cpp)
    target_include_directories(nativeui_t090_simplex_noise_gpu_reference_tests PRIVATE ${PROJECT_SOURCE_DIR})
    _nativeui_attach_consumer_platform(
      TARGET nativeui_t090_simplex_noise_gpu_reference_tests
      CONSUMER_ID org.nativeui.test.t090-simplex-noise-gpu-reference
    )
    nativeui_enable_project_warnings(nativeui_t090_simplex_noise_gpu_reference_tests)
    nativeui_enable_sanitizers(nativeui_t090_simplex_noise_gpu_reference_tests)

    add_executable(nativeui_t095_scene_gpu_tests ${PROJECT_SOURCE_DIR}/tests/t095_scene_gpu_tests.cpp)
    target_include_directories(nativeui_t095_scene_gpu_tests PRIVATE
      "${PROJECT_SOURCE_DIR}"
    )
    _nativeui_attach_consumer_platform(
      TARGET nativeui_t095_scene_gpu_tests
      CONSUMER_ID org.nativeui.test.t095-scene-gpu
    )
    nativeui_enable_project_warnings(nativeui_t095_scene_gpu_tests)
    nativeui_enable_sanitizers(nativeui_t095_scene_gpu_tests)

    add_executable(nativeui_t096_scene_gpu_tests ${PROJECT_SOURCE_DIR}/tests/t096_scene_gpu_tests.cpp)
    target_include_directories(nativeui_t096_scene_gpu_tests PRIVATE
      "${PROJECT_SOURCE_DIR}"
    )
    _nativeui_attach_consumer_platform(
      TARGET nativeui_t096_scene_gpu_tests
      CONSUMER_ID org.nativeui.test.t096-scene-gpu
    )
    nativeui_enable_project_warnings(nativeui_t096_scene_gpu_tests)
    nativeui_enable_sanitizers(nativeui_t096_scene_gpu_tests)

    add_executable(nativeui_smoke_embedded ${PROJECT_SOURCE_DIR}/tests/smoke_embedded.cpp)
    _nativeui_attach_consumer_platform(
      TARGET nativeui_smoke_embedded
      CONSUMER_ID org.nativeui.test.smoke-embedded
      OUT_BRIDGE _nativeui_smoke_embedded_bridge
    )
    nativeui_enable_project_warnings(nativeui_smoke_embedded)
    nativeui_enable_sanitizers(nativeui_smoke_embedded)

    if(APPLE)
      enable_language(OBJCXX)
      add_executable(nativeui_smoke_macos_drop ${PROJECT_SOURCE_DIR}/tests/smoke_macos_drop.mm)
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
        TIMEOUT 60
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
          -P "${PROJECT_SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
      )
      set_tests_properties(nativeui_objc_runtime_prefix PROPERTIES
        LABELS "integration;platform;macos;objc-runtime;t053"
        TIMEOUT 300
      )
    endif()

    if(NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS)
      add_test(NAME nativeui_smoke_standalone COMMAND nativeui_smoke_standalone)
      add_test(NAME nativeui_smoke_embedded COMMAND nativeui_smoke_embedded)
      add_test(NAME nativeui_image_texture_gpu_reference_tests
        COMMAND nativeui_image_texture_gpu_reference_tests)
      add_test(NAME nativeui_t088_noise_gpu_reference_tests
        COMMAND nativeui_t088_noise_gpu_reference_tests)
      add_test(NAME nativeui_t089_perlin_noise_gpu_reference_tests
        COMMAND nativeui_t089_perlin_noise_gpu_reference_tests)
      add_test(NAME nativeui_t090_simplex_noise_gpu_reference_tests
        COMMAND nativeui_t090_simplex_noise_gpu_reference_tests)
      add_test(NAME nativeui_t095_scene_gpu_tests COMMAND nativeui_t095_scene_gpu_tests)
      add_test(NAME nativeui_t096_scene_gpu_tests COMMAND nativeui_t096_scene_gpu_tests)
      set_tests_properties(nativeui_smoke_standalone nativeui_smoke_embedded PROPERTIES
        LABELS "integration;platform;smoke"
        TIMEOUT 60
      )
      set_tests_properties(nativeui_image_texture_gpu_reference_tests
        nativeui_t088_noise_gpu_reference_tests
        nativeui_t089_perlin_noise_gpu_reference_tests
        nativeui_t090_simplex_noise_gpu_reference_tests
        nativeui_t095_scene_gpu_tests nativeui_t096_scene_gpu_tests PROPERTIES
        LABELS "integration;platform;gpu"
        TIMEOUT 60
      )
    endif()
  endif()

  # Each public header gets its own translation unit so missing direct includes
  # cannot be accidentally masked by umbrella include order.
  foreach(_header IN ITEMS geometry constraints invalidation input gesture state dispatcher desktop_services text_edit text theme path resource embedded_resource resource_manager image shader noise svg paint_style paint component component_state command focus layout virtual_list widgets overlay tooltip inspector ui headless window nativeui)
    add_library(nativeui_header_${_header}_compile OBJECT ${PROJECT_SOURCE_DIR}/tests/headers/${_header}.cpp)
    target_link_libraries(nativeui_header_${_header}_compile PRIVATE NativeUI::Core)
    nativeui_enable_project_warnings(nativeui_header_${_header}_compile)
    nativeui_enable_sanitizers(nativeui_header_${_header}_compile)
  endforeach()

endif()

if(NATIVEUI_BUILD_PACKAGE_TESTS)
  function(nativeui_add_package_contract name script)
    add_test(NAME ${name}
      COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/tests/${script}")
    set_tests_properties(${name} PROPERTIES LABELS "package" TIMEOUT 300)
  endfunction()

  nativeui_add_package_contract(nativeui_package_t047_attach_contract
    t047_attach_contract_tests.cmake)
  nativeui_add_package_contract(nativeui_package_t048_source_contract
    t048_external_consumer_contract_tests.cmake)
  nativeui_add_package_contract(nativeui_package_t054_argument_contract
    t054_argument_boundary_tests.cmake)
  nativeui_add_package_contract(nativeui_package_t054_source_contract
    t054_package_source_contract_tests.cmake)
  nativeui_add_package_contract(nativeui_package_t056_source_contract
    t056_package_source_contract_tests.cmake)
  nativeui_add_package_contract(nativeui_package_t056_binary_data_contract
    t056_binary_data_contract_tests.cmake)

  add_test(NAME nativeui_package_t047
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DBUILD_PACKAGE_DIR=${CMAKE_BINARY_DIR}/nativeui-package"
      "-DINSTALL_CMAKE_DIR=lib/cmake/NativeUI"
      "-DCONFIG=$<CONFIG>"
      -P "${PROJECT_SOURCE_DIR}/tests/t047_package_tests.cmake")
  add_test(NAME nativeui_package_t048
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DINSTALL_CMAKE_DIR=lib/cmake/NativeUI"
      "-DCONFIG=$<CONFIG>"
      -P "${PROJECT_SOURCE_DIR}/tests/t048_external_consumer_tests.cmake")
  add_test(NAME nativeui_package_t054
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DBUILD_PACKAGE_DIR=${CMAKE_BINARY_DIR}/nativeui-package"
      "-DINSTALL_CMAKE_DIR=lib/cmake/NativeUI"
      "-DCONFIG=$<CONFIG>"
      -P "${PROJECT_SOURCE_DIR}/tests/t054_package_tests.cmake")
  add_test(NAME nativeui_package_t056
    COMMAND ${CMAKE_COMMAND}
      "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
      "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
      "-DBUILD_PACKAGE_DIR=${CMAKE_BINARY_DIR}/nativeui-package"
      "-DINSTALL_CMAKE_DIR=lib/cmake/NativeUI"
      "-DCONFIG=$<CONFIG>"
      -P "${PROJECT_SOURCE_DIR}/tests/t056_package_tests.cmake")
  set_tests_properties(nativeui_package_t047 nativeui_package_t048
    nativeui_package_t054 nativeui_package_t056 PROPERTIES
    LABELS "package" RUN_SERIAL TRUE TIMEOUT 900)
  set_tests_properties(nativeui_package_t048 nativeui_package_t054
    nativeui_package_t056 PROPERTIES DEPENDS nativeui_package_t047)

  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    nativeui_add_package_contract(nativeui_package_t054_application_contract
      t054_application_contract_tests.cmake)
  endif()
endif()
