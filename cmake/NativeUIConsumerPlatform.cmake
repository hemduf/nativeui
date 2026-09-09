include_guard(GLOBAL)

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
