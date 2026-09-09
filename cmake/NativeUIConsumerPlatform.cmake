include_guard(GLOBAL)
include(CMakeParseArguments)

# Derive the only supported NativeUI consumer-specific Objective-C runtime
# prefix from the exact original UTF-8 identity bytes. Keep this algorithm in
# one CMake function so source-tree and installed-package helpers cannot drift.
function(nativeui_compute_objc_runtime_prefix out_var consumer_id)
  if("${out_var}" STREQUAL "")
    message(FATAL_ERROR "nativeui_compute_objc_runtime_prefix requires an output variable")
  endif()
  if("${consumer_id}" STREQUAL "")
    message(FATAL_ERROR "NativeUI CONSUMER_ID must be non-empty")
  endif()

  string(SHA256 _nativeui_consumer_digest "${consumer_id}")
  string(SUBSTRING "${_nativeui_consumer_digest}" 0 12 _nativeui_digest12)

  # Only ASCII alphanumerics survive in the human-readable fragment. Every
  # maximal run of punctuation, whitespace or non-ASCII UTF-8 bytes collapses
  # to one underscore; the SHA-256 above still covers the original bytes.
  string(REGEX REPLACE "[^A-Za-z0-9]+" "_" _nativeui_fragment "${consumer_id}")
  string(REGEX REPLACE "^_+" "" _nativeui_fragment "${_nativeui_fragment}")
  string(REGEX REPLACE "_+$" "" _nativeui_fragment "${_nativeui_fragment}")
  if(_nativeui_fragment STREQUAL "")
    set(_nativeui_fragment "consumer")
  endif()

  string(LENGTH "${_nativeui_fragment}" _nativeui_fragment_length)
  if(_nativeui_fragment_length GREATER 24)
    string(SUBSTRING "${_nativeui_fragment}" 0 24 _nativeui_fragment)
    string(REGEX REPLACE "_+$" "" _nativeui_fragment "${_nativeui_fragment}")
    if(_nativeui_fragment STREQUAL "")
      set(_nativeui_fragment "consumer")
    endif()
  endif()

  set(${out_var}
      "NUI_${_nativeui_fragment}_${_nativeui_digest12}_"
      PARENT_SCOPE)
endfunction()

# Configure-time bookkeeping only. These GLOBAL CMake properties disappear
# after generation; no process/runtime registry is emitted into NativeUI.
function(_nativeui_register_consumer_identity target consumer_id)
  if("${target}" STREQUAL "")
    message(FATAL_ERROR "NativeUI consumer registration requires a target name")
  endif()
  if("${consumer_id}" STREQUAL "")
    message(FATAL_ERROR "NativeUI CONSUMER_ID must be non-empty")
  endif()

  string(SHA256 _target_key "${target}")
  string(SHA256 _identity_key "${consumer_id}")
  set(_target_property "NATIVEUI_T053_TARGET_${_target_key}")
  set(_identity_property "NATIVEUI_T053_ID_${_identity_key}")

  get_property(_target_is_set GLOBAL PROPERTY "${_target_property}" SET)
  if(_target_is_set)
    get_property(_existing_identity GLOBAL PROPERTY "${_target_property}")
    message(FATAL_ERROR
      "NativeUI target '${target}' already has consumer identity '${_existing_identity}'; duplicate attachment is not allowed")
  endif()

  get_property(_identity_is_set GLOBAL PROPERTY "${_identity_property}" SET)
  if(_identity_is_set)
    get_property(_existing_target GLOBAL PROPERTY "${_identity_property}")
    message(FATAL_ERROR
      "NativeUI CONSUMER_ID '${consumer_id}' is already registered to target '${_existing_target}'; target '${target}' cannot reuse it")
  endif()

  set_property(GLOBAL PROPERTY "${_target_property}" "${consumer_id}")
  set_property(GLOBAL PROPERTY "${_identity_property}" "${target}")
endfunction()

# Internal T053 source-tree/package primitive. T047 supplies the public
# nativeui_attach_platform() validation wrapper; all high-level helpers must
# delegate here instead of reproducing the prefix or bridge logic.
function(_nativeui_attach_consumer_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID;OUT_BRIDGE" "")
  if(NUI_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "NativeUI consumer platform attachment received unknown arguments: ${NUI_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT NUI_TARGET)
    message(FATAL_ERROR "NativeUI consumer platform attachment requires TARGET")
  endif()
  if(NOT TARGET "${NUI_TARGET}")
    message(FATAL_ERROR "NativeUI consumer platform target does not exist: '${NUI_TARGET}'")
  endif()
  if(NOT NUI_CONSUMER_ID)
    message(FATAL_ERROR "NativeUI consumer platform attachment requires CONSUMER_ID")
  endif()
  if(NOT TARGET NativeUI::NativeUI)
    message(FATAL_ERROR "NativeUI::NativeUI must exist before attaching the platform bridge")
  endif()

  _nativeui_register_consumer_identity("${NUI_TARGET}" "${NUI_CONSUMER_ID}")
  nativeui_compute_objc_runtime_prefix(_nativeui_objc_prefix "${NUI_CONSUMER_ID}")

  set(_nativeui_bridge "")
  if(APPLE)
    if(NOT DEFINED NATIVEUI_PUGL_MACOS_SOURCES OR
       "${NATIVEUI_PUGL_MACOS_SOURCES}" STREQUAL "")
      message(FATAL_ERROR
        "NativeUI macOS consumer bridge sources are unavailable; T053 platform dependencies were not initialized")
    endif()
    if(NOT TARGET NativeUI::Pugl)
      message(FATAL_ERROR "NativeUI::Pugl common target is unavailable")
    endif()

    string(SHA256 _nativeui_bridge_digest
      "${NUI_TARGET}\n${NUI_CONSUMER_ID}")
    string(SUBSTRING "${_nativeui_bridge_digest}" 0 16 _nativeui_bridge_key)
    set(_nativeui_bridge "nativeui_macos_bridge_${_nativeui_bridge_key}")

    add_library("${_nativeui_bridge}" STATIC ${NATIVEUI_PUGL_MACOS_SOURCES})
    set_target_properties("${_nativeui_bridge}" PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      C_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
    )
    target_compile_features("${_nativeui_bridge}" PUBLIC c_std_99)
    target_include_directories("${_nativeui_bridge}"
      PUBLIC "${NATIVEUI_PUGL_SOURCE_DIR}/include"
      PRIVATE "${NATIVEUI_PUGL_SOURCE_DIR}/src"
    )
    target_compile_definitions("${_nativeui_bridge}"
      PUBLIC PUGL_STATIC
      PRIVATE
        PUGL_INTERNAL
        GL_SILENCE_DEPRECATION
        "PuglWindow=${_nativeui_objc_prefix}PuglWindow"
        "PuglWindowDelegate=${_nativeui_objc_prefix}PuglWindowDelegate"
        "PuglWrapperView=${_nativeui_objc_prefix}PuglWrapperView"
        "PuglOpenGLView=${_nativeui_objc_prefix}PuglOpenGLView"
    )
    target_compile_options("${_nativeui_bridge}" PRIVATE -Wno-deprecated-declarations)
    target_link_libraries("${_nativeui_bridge}" PUBLIC NativeUI::Pugl)

    if(COMMAND nativeui_enable_project_warnings)
      nativeui_enable_project_warnings("${_nativeui_bridge}")
    endif()
  endif()

  target_link_libraries("${NUI_TARGET}" PRIVATE NativeUI::NativeUI)
  if(_nativeui_bridge)
    target_link_libraries("${NUI_TARGET}" PRIVATE "${_nativeui_bridge}")
  endif()

  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_CONSUMER_ID "${NUI_CONSUMER_ID}")
  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_OBJC_RUNTIME_PREFIX "${_nativeui_objc_prefix}")
  if(_nativeui_bridge)
    set_property(TARGET "${NUI_TARGET}" PROPERTY
      NATIVEUI_OBJC_BRIDGE_TARGET "${_nativeui_bridge}")
  endif()

  if(NUI_OUT_BRIDGE)
    set(${NUI_OUT_BRIDGE} "${_nativeui_bridge}" PARENT_SCOPE)
  endif()
endfunction()
