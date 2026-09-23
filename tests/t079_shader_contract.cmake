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
      # T088 explicitly compiles its built-in source at creation time. Keep the
      # T079 no-implicit-paint-compilation rule for every other production file.
      string(REGEX MATCHALL
        "ShaderProgram[ \t\r\n]*::[ \t\r\n]*compile[ \t\r\n]*\\("
        _noise_compile_calls "${_content}")
      list(LENGTH _noise_compile_calls _noise_compile_count)
      string(FIND "${_content}" "NoiseCreateResult NoiseSource::create" _noise_create)
      string(FIND "${_content}" "ShaderProgram::compile(source)" _noise_compile)
      string(FIND "${_content}" "Brush NoiseSource::as_brush" _noise_brush)
      if(NOT _noise_compile_count EQUAL 1 OR _noise_create EQUAL -1 OR
         _noise_compile EQUAL -1 OR _noise_brush EQUAL -1 OR
         _noise_compile LESS _noise_create OR
         _noise_compile GREATER _noise_brush)
        message(FATAL_ERROR
          "T079: T088 built-in shader must compile exactly once in NoiseSource::create")
      endif()
    else()
      message(FATAL_ERROR
        "T079: implicit ShaderProgram::compile call found outside skia_shader.cpp: ${_path}")
    endif()
  endif()
endforeach()
