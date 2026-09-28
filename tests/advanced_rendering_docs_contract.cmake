if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_guide "${SOURCE_DIR}/docs/advanced-rendering.md")
set(_snippets "${SOURCE_DIR}/tests/advanced_rendering_docs_snippets.cpp")

foreach(_path IN ITEMS
    "${_guide}"
    "${_snippets}"
    "${SOURCE_DIR}/include/nativeui/paint_style.hpp"
    "${SOURCE_DIR}/include/nativeui/paint.hpp"
    "${SOURCE_DIR}/include/nativeui/shader.hpp")
  if(NOT EXISTS "${_path}")
    message(FATAL_ERROR "advanced-rendering documentation contract: missing ${_path}")
  endif()
endforeach()

file(READ "${_guide}" _guide_source)
file(READ "${_snippets}" _snippet_source)

foreach(_required IN ITEMS
    "## 1. Rendering model and scope"
    "## 2. Brush, colors and gradients"
    "## 3. Clips, layers and effects"
    "## 4. SkSL runtime shaders"
    "ui::LinearGradient"
    "ui::Effect::gaussian_blur"
    "ui::ShaderProgram::compile"
    "ui::ShaderInstance"
    "Skia C++ API and ABI remain private"
    "does **not** claim that arbitrary user-supplied SkSL will meet a hard 60 FPS target")
  string(FIND "${_guide_source}" "${_required}" _required_pos)
  if(_required_pos EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: missing '${_required}'")
  endif()
endforeach()

foreach(_forbidden IN ITEMS
    "#include <include/core/"
    "#include <include/effects/"
    "#include <pugl/"
    "#include <nativeui/detail/"
    "ui::detail::"
    "SkRuntimeEffect"
    "SkCanvas")
  string(FIND "${_guide_source}" "${_forbidden}" _guide_forbidden)
  string(FIND "${_snippet_source}" "${_forbidden}" _snippet_forbidden)
  if(NOT _guide_forbidden EQUAL -1 OR NOT _snippet_forbidden EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: backend/private API leaked: '${_forbidden}'")
  endif()
endforeach()

foreach(_relative IN ITEMS
    "../include/nativeui/paint_style.hpp"
    "../include/nativeui/paint.hpp"
    "../include/nativeui/shader.hpp")
  string(FIND "${_guide_source}" "(${_relative})" _link_pos)
  if(_link_pos EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: missing link ${_relative}")
  endif()
  get_filename_component(_resolved "${SOURCE_DIR}/docs/${_relative}" ABSOLUTE)
  if(NOT EXISTS "${_resolved}")
    message(FATAL_ERROR
      "advanced-rendering documentation contract: broken link ${_relative}")
  endif()
endforeach()
