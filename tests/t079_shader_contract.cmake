if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

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
    "SkString")
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

file(GLOB _production_sources "${SOURCE_DIR}/src/*.cpp")
file(GLOB _production_headers "${SOURCE_DIR}/include/nativeui/*.hpp")
foreach(_path IN LISTS _production_sources _production_headers)
  get_filename_component(_name "${_path}" NAME)
  if(_name STREQUAL "skia_shader.cpp")
    continue()
  endif()
  file(READ "${_path}" _content)
  string(FIND "${_content}" "ShaderProgram::compile(" _implicit_compile)
  if(NOT _implicit_compile EQUAL -1)
    message(FATAL_ERROR
      "T079: implicit ShaderProgram::compile call found outside skia_shader.cpp: ${_path}")
  endif()
endforeach()
