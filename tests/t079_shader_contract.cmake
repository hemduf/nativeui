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
    message(FATAL_ERROR
      "T079: implicit ShaderProgram::compile call found outside skia_shader.cpp: ${_path}")
  endif()
endforeach()
