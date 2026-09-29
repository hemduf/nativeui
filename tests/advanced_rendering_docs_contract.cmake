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
    "mutable preparation state, not a synchronized shared object"
    "Numeric uniform setters are allocation-free/noexcept"
    "`set_child()` snapshots a Brush and may allocate"
    "Skia C++ API/ABI is not a supported portable NativeUI integration contract"
    "does **not** claim that arbitrary user-supplied SkSL will meet a hard 60 FPS target")
  string(FIND "${_guide_source}" "${_required}" _required_pos)
  if(_required_pos EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: missing '${_required}'")
  endif()
endforeach()


function(require_compiled_doc_snippet _name _anchor)
  string(FIND "${_guide_source}" "${_anchor}" _anchor_pos)
  if(_anchor_pos EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: missing snippet anchor '${_anchor}'")
  endif()

  string(SUBSTRING "${_guide_source}" ${_anchor_pos} -1 _guide_tail)
  string(REGEX MATCH "~~~cpp\n([^~]*)~~~" _doc_block "${_guide_tail}")
  if(_doc_block STREQUAL "")
    message(FATAL_ERROR
      "advanced-rendering documentation contract: missing C++ block for ${_name}")
  endif()

  set(_compiled_snippet "${CMAKE_MATCH_1}")
  if(_name STREQUAL "gradient")
    string(REPLACE
      "void paint_header("
      "void advanced_rendering_gradient_snippet("
      _compiled_snippet
      "${_compiled_snippet}")
  elseif(_name STREQUAL "runtime shader")
    string(REPLACE
      "ui::Brush make_meter_brush()"
      "ui::Brush advanced_rendering_shader_snippet()"
      _compiled_snippet
      "${_compiled_snippet}")
  endif()

  string(FIND "${_snippet_source}" "${_compiled_snippet}" _compiled_fixture_pos)
  if(_compiled_fixture_pos EQUAL -1)
    message(FATAL_ERROR
      "advanced-rendering documentation contract: ${_name} snippet is not the compiled fixture")
  endif()
endfunction()

require_compiled_doc_snippet("gradient" "Canonical gradient fill:")
require_compiled_doc_snippet(
  "runtime shader"
  "Canonical compile, uniform binding and Brush snapshot:")

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
