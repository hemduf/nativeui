if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_shader "${SOURCE_DIR}/include/nativeui/shader.hpp")
set(_style "${SOURCE_DIR}/include/nativeui/paint_style.hpp")
set(_backend "${SOURCE_DIR}/src/skia_shader.cpp")
set(_instance_access "${SOURCE_DIR}/src/detail/shader_instance_access.hpp")
set(_brush_access "${SOURCE_DIR}/src/detail/shader_brush_access.hpp")
set(_tests "${SOURCE_DIR}/tests/t082_shader_children_tests.cpp")
set(_faults "${SOURCE_DIR}/tests/shader_fault_tests.cpp")
set(_example "${SOURCE_DIR}/examples/features/t082_shader_children.cpp")

foreach(_path IN ITEMS
    "${_shader}" "${_style}" "${_backend}" "${_instance_access}"
    "${_brush_access}" "${_tests}" "${_faults}" "${_example}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "T082: missing required file ${_path}")
  endif()
endforeach()

file(READ "${_shader}" _shader_source)
file(READ "${_style}" _style_source)
file(READ "${_backend}" _backend_source)
file(READ "${_instance_access}" _instance_access_source)
file(READ "${_brush_access}" _brush_access_source)
file(READ "${_tests}" _tests_source)
file(READ "${_faults}" _faults_source)
file(READ "${_example}" _example_source)

foreach(_required IN ITEMS
    "class Brush;"
    "struct ShaderChildInfo"
    "std::span<const ShaderChildInfo> children() const noexcept"
    "ShaderSetResult set_child(std::string_view name, const Brush& brush)"
    "kMaxChildDepth = 16"
    "ShaderInstanceChildState")
  string(FIND "${_shader_source}" "${_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: public shader contract misses '${_required}'")
  endif()
endforeach()

string(FIND "${_shader_source}" "#include <nativeui/paint_style.hpp>" _shader_style_include)
if(NOT _shader_style_include EQUAL -1)
  message(FATAL_ERROR "T082: shader.hpp must not include paint_style.hpp")
endif()
string(FIND "${_style_source}" "#include <nativeui/shader.hpp>" _style_shader_include)
if(NOT _style_shader_include EQUAL -1)
  message(FATAL_ERROR "T082: paint_style.hpp must not include shader.hpp")
endif()

foreach(_forbidden IN ITEMS "SkRuntimeEffect" "SkShader" "SkData" "SkCanvas")
  string(FIND "${_shader_source}" "${_forbidden}" _pos)
  if(NOT _pos EQUAL -1)
    message(FATAL_ERROR "T082: backend token leaks into shader.hpp: '${_forbidden}'")
  endif()
endforeach()

foreach(_backend_required IN ITEMS
    "SkRuntimeEffect::ChildType::kShader"
    "reflected.index"
    "std::vector<ShaderChildInfo> children"
    "std::vector<std::shared_ptr<const Brush>> children"
    "ShaderBrushMaterializer"
    "SkRuntimeEffect::ChildPtr"
    "backend_children.emplace_back()"
    "ShaderInstance::kMaxChildDepth"
    "NativeUI shader Brush child slot count does not match program")
  string(FIND "${_backend_source}" "${_backend_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: backend child contract misses '${_backend_required}'")
  endif()
endforeach()

foreach(_access_required IN ITEMS "child_count" "child" "depth")
  string(FIND "${_instance_access_source}" "${_access_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: ShaderInstance private access misses '${_access_required}'")
  endif()
  string(FIND "${_brush_access_source}" "${_access_required}" _brush_pos)
  if(_brush_pos EQUAL -1)
    message(FATAL_ERROR "T082: ShaderBrush private access misses '${_access_required}'")
  endif()
endforeach()

foreach(_test_required IN ITEMS
    "bounded_depth_and_same_program_snapshot_reuse"
    "direct_vs_child_parity"
    "missing_child_is_transparent_black"
    "transformed_child_coordinates"
    "nested_layout_color_semantics"
    "two_renderer_nested_isolation")
  string(FIND "${_tests_source}" "${_test_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: targeted test misses '${_test_required}'")
  endif()
endforeach()

foreach(_fault_required IN ITEMS
    "__NATIVEUI_T082_FAIL_CHILD_REFLECTION__"
    "__NATIVEUI_T082_FAIL_DURING_CHILD_REFLECTION__"
    "__NATIVEUI_T082_FAIL_AFTER_CHILD_REFLECTION__"
    "child_binding_failure_is_atomic_and_moves_allocate_nothing"
    "nested_child_materialization_failure_recovers"
    "set_shader_materialization_failure_after_calls_for_test")
  string(FIND "${_faults_source}" "${_fault_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: fault coverage misses '${_fault_required}'")
  endif()
endforeach()

foreach(_example_required IN ITEMS
    "self_test_requested"
    "set_child"
    "ShaderInstance"
    "HeadlessRenderer")
  string(FIND "${_example_source}" "${_example_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: feature example misses '${_example_required}'")
  endif()
endforeach()

foreach(_golden IN ITEMS
    "${SOURCE_DIR}/tests/golden/baselines/shader_child_two_level.ppm"
    "${SOURCE_DIR}/tests/golden/baselines/shader_child_deep.ppm")
  if(NOT EXISTS "${_golden}")
    message(FATAL_ERROR "T082: missing required shader-child golden ${_golden}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/tests/gradient_golden_tests.cpp" _golden_source)
foreach(_golden_test IN ITEMS
    "verify_shader_child_two_level"
    "verify_shader_child_deep")
  string(FIND "${_golden_source}" "${_golden_test}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: golden suite misses '${_golden_test}'")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/tests/CMakeLists.txt" _cmake)
foreach(_cmake_required IN ITEMS
    "nativeui_t082_shader_contract"
    "nativeui_t082_shader_children_tests")
  string(FIND "${_cmake}" "${_cmake_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T082: tests/CMakeLists.txt misses '${_cmake_required}'")
  endif()
endforeach()
