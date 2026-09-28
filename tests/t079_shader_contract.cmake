if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
set(_shader_header "${SOURCE_DIR}/include/nativeui/shader.hpp")
if(NOT EXISTS "${_shader_header}")
  message(FATAL_ERROR "T079: missing public shader.hpp")
endif()

file(READ "${_shader_header}" _shader_public)
foreach(_forbidden IN ITEMS
    "SkRuntimeEffect"
    "SkShader"
    "SkData"
    "SkCanvas"
    "SkString"
    "NATIVEUI_ENABLE_TEST_SEAMS"
    "compile_shader_program_for_test")
  string(FIND "${_shader_public}" "${_forbidden}" _forbidden_pos)
  if(NOT _forbidden_pos EQUAL -1)
    message(FATAL_ERROR
      "T079: public shader.hpp leaks backend type/token '${_forbidden}'")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/src/skia_shader.cpp" _shader_backend)
string(FIND
  "${_shader_backend}"
  "std::numeric_limits<uint32_t>::max()"
  _backend_size_limit)
string(FIND
  "${_shader_backend}"
  "sksl.size() > detail::kMaxSkSLSourceBytes"
  _backend_size_guard)
string(FIND
  "${_shader_backend}"
  "SkRuntimeEffect::MakeForShader"
  _backend_compile_call)
if(_backend_size_limit EQUAL -1 OR
   _backend_size_guard EQUAL -1 OR
   _backend_compile_call EQUAL -1 OR
   _backend_size_guard GREATER _backend_compile_call)
  message(FATAL_ERROR
    "T079: pinned SkString 32-bit source-size guard must run before MakeForShader")
endif()

file(READ "${SOURCE_DIR}/include/nativeui/nativeui.hpp" _umbrella)
string(FIND "${_umbrella}" "#include <nativeui/shader.hpp>" _umbrella_pos)
if(_umbrella_pos EQUAL -1)
  message(FATAL_ERROR "T079: nativeui.hpp does not expose shader.hpp")
endif()

string(REGEX MATCH
  "target_compile_definitions[ \t\r\n]*\\([ \t\r\n]*nativeui_core[^)]*NATIVEUI_ENABLE_TEST_SEAMS"
  _core_test_seam
  "${_root_cmake}")
if(NOT _core_test_seam STREQUAL "")
  message(FATAL_ERROR
    "T079: NativeUI::Core must not compile the shader fault-test seam")
endif()

file(GLOB_RECURSE _production_files
  "${SOURCE_DIR}/src/*.c"
  "${SOURCE_DIR}/src/*.cc"
  "${SOURCE_DIR}/src/*.cpp"
  "${SOURCE_DIR}/src/*.cxx"
  "${SOURCE_DIR}/src/*.m"
  "${SOURCE_DIR}/src/*.mm"
  "${SOURCE_DIR}/src/*.h"
  "${SOURCE_DIR}/src/*.hpp"
  "${SOURCE_DIR}/src/*.inc"
  "${SOURCE_DIR}/include/nativeui/*.h"
  "${SOURCE_DIR}/include/nativeui/*.hpp"
  "${SOURCE_DIR}/include/nativeui/*.inc"
)
foreach(_path IN LISTS _production_files)
  get_filename_component(_name "${_path}" NAME)
  if(_name STREQUAL "skia_shader.cpp")
    continue()
  endif()
  file(READ "${_path}" _content)
  string(REGEX MATCH
    "ShaderProgram[ \t\r\n]*::[ \t\r\n]*compile[ \t\r\n]*\\("
    _implicit_compile
    "${_content}")
  if(NOT _implicit_compile STREQUAL "")
    if(_name STREQUAL "skia_noise.cpp")
      # Built-in noise programs may compile only at explicit NoiseSource
      # creation boundaries. Neither path may defer source compilation to
      # as_brush()/paint.
      string(REGEX MATCHALL
        "ShaderProgram[ \t\r\n]*::[ \t\r\n]*compile[ \t\r\n]*\\("
        _noise_compile_calls "${_content}")
      list(LENGTH _noise_compile_calls _noise_compile_count)
      string(FIND "${_content}" "void append_fractal_kernel" _fractal_kernel)
      string(FIND "${_content}" "NoiseCreateResult NoiseSource::create(" _noise_create)
      string(FIND "${_content}" "NoiseCreateResult NoiseSource::create_fractal(" _fractal_create)
      string(FIND "${_content}" "Brush NoiseSource::as_brush" _noise_brush)

      if(_fractal_kernel EQUAL -1 OR _noise_create EQUAL -1 OR
         _fractal_create EQUAL -1 OR
         _noise_brush EQUAL -1 OR
         _fractal_kernel GREATER _noise_create OR
         _fractal_create LESS _noise_create OR
         _noise_brush LESS _fractal_create)
        message(FATAL_ERROR
          "Noise shader contract: creation/materialization boundaries are missing or reordered")
      endif()

      math(EXPR _fractal_kernel_region_length "${_noise_create} - ${_fractal_kernel}")
      string(SUBSTRING "${_content}" ${_fractal_kernel}
        ${_fractal_kernel_region_length} _fractal_kernel_region)
      string(REGEX MATCH
        "(pow|exp|log)[ \\t\\r\\n]*\\\\("
        _fractal_exponentiation "${_fractal_kernel_region}")
      string(FIND "${_fractal_kernel_region}"
        "fractal_base(p * frequency)" _fractal_scaled_coordinates)
      if(NOT _fractal_exponentiation STREQUAL "" OR
         _fractal_scaled_coordinates EQUAL -1)
        message(FATAL_ERROR
          "Fractal noise must use iterative frequency scaling without exponentiation")
      endif()

      math(EXPR _base_region_length "${_fractal_create} - ${_noise_create}")
      string(SUBSTRING "${_content}" ${_noise_create} ${_base_region_length}
        _base_create_region)
      math(EXPR _fractal_region_length "${_noise_brush} - ${_fractal_create}")
      string(SUBSTRING "${_content}" ${_fractal_create} ${_fractal_region_length}
        _fractal_create_region)

      string(REGEX MATCHALL
        "ShaderProgram[ \t\r\n]*::[ \t\r\n]*compile[ \t\r\n]*\\("
        _base_compile_calls "${_base_create_region}")
      string(REGEX MATCHALL
        "ShaderProgram[ \t\r\n]*::[ \t\r\n]*compile[ \t\r\n]*\\("
        _fractal_compile_calls "${_fractal_create_region}")
      list(LENGTH _base_compile_calls _base_compile_count)
      list(LENGTH _fractal_compile_calls _fractal_compile_count)

      if(NOT _noise_compile_count EQUAL 2 OR
         NOT _base_compile_count EQUAL 1 OR
         NOT _fractal_compile_count EQUAL 1)
        message(FATAL_ERROR
          "Noise shader contract: built-in programs must compile exactly once in each explicit creation path")
      endif()
    else()
      message(FATAL_ERROR
        "T079: implicit ShaderProgram::compile call found outside skia_shader.cpp: ${_path}")
    endif()
  endif()
endforeach()
