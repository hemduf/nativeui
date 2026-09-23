if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_paint_style "${SOURCE_DIR}/include/nativeui/paint_style.hpp")
set(_paint "${SOURCE_DIR}/include/nativeui/paint.hpp")
set(_shader_backend "${SOURCE_DIR}/src/skia_shader.cpp")
set(_access "${SOURCE_DIR}/src/detail/shader_brush_access.hpp")

foreach(_path IN ITEMS "${_paint_style}" "${_paint}" "${_shader_backend}" "${_access}")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "T081: missing required file ${_path}")
  endif()
endforeach()

file(READ "${_paint_style}" _style)
file(READ "${_paint}" _paint_source)
file(READ "${_shader_backend}" _backend)
file(READ "${_access}" _access_source)

foreach(_required IN ITEMS
    "class ShaderInstance;"
    "explicit Brush(const ShaderInstance& shader);"
    "ShaderBrushSnapshot"
    "std::shared_ptr<const detail::ShaderBrushSnapshot>")
  string(FIND "${_style}" "${_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T081: missing Brush snapshot contract '${_required}'")
  endif()
endforeach()

string(FIND "${_style}" "#include <nativeui/shader.hpp>" _shader_include)
if(NOT _shader_include EQUAL -1)
  message(FATAL_ERROR "T081: paint_style.hpp must not include shader.hpp")
endif()

foreach(_forbidden IN ITEMS "SkRuntimeEffect" "SkShader" "SkData" "SkCanvas")
  string(FIND "${_style}" "${_forbidden}" _forbidden_pos)
  if(NOT _forbidden_pos EQUAL -1)
    message(FATAL_ERROR "T081: backend token leaks into paint_style.hpp: '${_forbidden}'")
  endif()
endforeach()

foreach(_backend_required IN ITEMS
    "SkData::MakeEmpty"
    "SkData::MakeWithCopy"
    "data.effect->makeShader"
    "ShaderInstanceAccess::binding_bytes"
    "shader_materialization_call_count_for_test_value"
    "NativeUI runtime shader materialization returned no shader")
  string(FIND "${_backend}" "${_backend_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T081: missing backend/materialization contract '${_backend_required}'")
  endif()
endforeach()

string(FIND "${_paint_source}" "materialize_shader_brush" _materialize)
if(_materialize EQUAL -1)
  message(FATAL_ERROR "T081: Painter does not materialize shader Brush snapshots")
endif()

foreach(_access_required IN ITEMS
    "is_shader"
    "is_transparent_solid"
    "binding_bytes")
  string(FIND "${_access_source}" "${_access_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T081: private snapshot access misses '${_access_required}'")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/tests/CMakeLists.txt" _cmake)
foreach(_cmake_required IN ITEMS
    "nativeui_t081_shader_contract"
    "nativeui_t081_shader_brush_tests")
  string(FIND "${_cmake}" "${_cmake_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T081: tests/CMakeLists.txt misses '${_cmake_required}'")
  endif()
endforeach()

set(_golden "${SOURCE_DIR}/tests/golden/baselines/shader_brush_primitives.ppm")
if(NOT EXISTS "${_golden}")
  message(FATAL_ERROR "T081: missing shader Brush golden baseline")
endif()

file(READ "${SOURCE_DIR}/tests/gradient_golden_tests.cpp" _golden_source)
string(FIND "${_golden_source}" "verify_shader_brush_primitives" _golden_test)
if(_golden_test EQUAL -1)
  message(FATAL_ERROR "T081: shader Brush golden is not exercised")
endif()

file(READ "${SOURCE_DIR}/examples/features/t081_shader_brush.cpp" _example)
foreach(_example_required IN ITEMS
    "self_test_requested"
    "ShaderInstance"
    "Brush"
    "HeadlessRenderer")
  string(FIND "${_example}" "${_example_required}" _pos)
  if(_pos EQUAL -1)
    message(FATAL_ERROR "T081: feature example misses '${_example_required}'")
  endif()
endforeach()
