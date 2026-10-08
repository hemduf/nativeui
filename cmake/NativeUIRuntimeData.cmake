include_guard(GLOBAL)
include(CMakeParseArguments)

# Core-only consumers opt in explicitly. Platform attachment invokes this helper
# automatically. Runtime files belong beside the final executable/module, never
# in a host-owned executable directory.
function(nativeui_attach_runtime_data)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET" "")
  if(NUI_UNPARSED_ARGUMENTS OR NUI_KEYWORDS_MISSING_VALUES OR
     NOT DEFINED NUI_TARGET OR NOT TARGET "${NUI_TARGET}")
    message(FATAL_ERROR "NativeUI runtime data requires an existing TARGET")
  endif()
  get_target_property(_nativeui_type "${NUI_TARGET}" TYPE)
  if(NOT _nativeui_type MATCHES "^(EXECUTABLE|SHARED_LIBRARY|MODULE_LIBRARY)$")
    message(FATAL_ERROR "NativeUI runtime data requires an executable or module target")
  endif()
  if(NOT WIN32)
    return()
  endif()
  if(NOT TARGET NativeUI::Core)
    message(FATAL_ERROR "NativeUI runtime data requires NativeUI::Core")
  endif()
  get_target_property(_nativeui_data NativeUI::Core NATIVEUI_SKIA_ICU_DATA)
  if(NOT _nativeui_data OR NOT EXISTS "${_nativeui_data}" OR IS_DIRECTORY "${_nativeui_data}")
    message(FATAL_ERROR "NativeUI matching Skia ICU runtime data is missing: ${_nativeui_data}")
  endif()
  get_target_property(_nativeui_attached "${NUI_TARGET}" NATIVEUI_RUNTIME_DATA_TARGET)
  if(_nativeui_attached)
    return()
  endif()

  # A dependency target works even when the consumer was created by its parent
  # directory. Always run it so deleting data does not require relinking the EXE.
  string(SHA256 _nativeui_digest "${CMAKE_CURRENT_BINARY_DIR}/${NUI_TARGET}")
  set(_nativeui_stage "nativeui_icu_data_${_nativeui_digest}")
  cmake_policy(PUSH)
  cmake_policy(SET CMP0112 NEW)
  add_custom_target("${_nativeui_stage}"
    COMMAND "${CMAKE_COMMAND}"
      "-DNATIVEUI_DATA_SOURCE=${_nativeui_data}"
      "-DNATIVEUI_DATA_DIRECTORY=$<TARGET_FILE_DIR:${NUI_TARGET}>"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/NativeUIRuntimeDataCopy.cmake"
    DEPENDS "${_nativeui_data}"
    VERBATIM)
  cmake_policy(POP)
  add_dependencies("${NUI_TARGET}" "${_nativeui_stage}")
  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_RUNTIME_DATA_TARGET "${_nativeui_stage}")
endfunction()
