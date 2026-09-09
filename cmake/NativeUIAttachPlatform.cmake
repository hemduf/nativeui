include_guard(GLOBAL)
include(CMakeParseArguments)

if(NOT COMMAND _nativeui_attach_consumer_platform)
  include("${CMAKE_CURRENT_LIST_DIR}/NativeUIConsumerPlatform.cmake")
endif()

function(_nativeui_validate_consumer_id consumer_id)
  if("${consumer_id}" STREQUAL "")
    message(FATAL_ERROR "NativeUI nativeui_attach_platform CONSUMER_ID is required")
  endif()

  string(REPLACE "." ";" _nativeui_consumer_segments "${consumer_id}")
  list(LENGTH _nativeui_consumer_segments _nativeui_consumer_segment_count)
  if(_nativeui_consumer_segment_count LESS 2)
    message(FATAL_ERROR
      "NativeUI CONSUMER_ID '${consumer_id}' must be a reverse-DNS identity with at least two dot-separated ASCII segments")
  endif()

  foreach(_nativeui_segment IN LISTS _nativeui_consumer_segments)
    if(_nativeui_segment STREQUAL "" OR
       NOT (_nativeui_segment MATCHES "^[A-Za-z0-9]$" OR
            _nativeui_segment MATCHES "^[A-Za-z0-9][A-Za-z0-9-]*[A-Za-z0-9]$"))
      message(FATAL_ERROR
        "NativeUI CONSUMER_ID '${consumer_id}' must be a reverse-DNS identity; "
        "each segment must begin/end with [A-Za-z0-9] and contain only [A-Za-z0-9-]")
    endif()
  endforeach()
endfunction()

function(nativeui_attach_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID" "")
  if(NUI_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "NativeUI nativeui_attach_platform received unknown arguments: ${NUI_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT NUI_TARGET)
    message(FATAL_ERROR "NativeUI nativeui_attach_platform TARGET is required")
  endif()
  if(NOT TARGET "${NUI_TARGET}")
    message(FATAL_ERROR
      "NativeUI nativeui_attach_platform target does not exist: '${NUI_TARGET}'")
  endif()
  if(NOT NUI_CONSUMER_ID)
    message(FATAL_ERROR "NativeUI nativeui_attach_platform CONSUMER_ID is required")
  endif()

  get_target_property(_nativeui_aliased_target "${NUI_TARGET}" ALIASED_TARGET)
  if(_nativeui_aliased_target)
    message(FATAL_ERROR
      "NativeUI nativeui_attach_platform target '${NUI_TARGET}' is an alias; attach the concrete final target instead")
  endif()

  get_target_property(_nativeui_imported "${NUI_TARGET}" IMPORTED)
  if(_nativeui_imported)
    message(FATAL_ERROR
      "NativeUI nativeui_attach_platform target '${NUI_TARGET}' is imported; attach an existing local final target instead")
  endif()

  get_target_property(_nativeui_target_type "${NUI_TARGET}" TYPE)
  if(NOT (_nativeui_target_type STREQUAL "EXECUTABLE" OR
          _nativeui_target_type STREQUAL "MODULE_LIBRARY" OR
          _nativeui_target_type STREQUAL "SHARED_LIBRARY"))
    message(FATAL_ERROR
      "NativeUI nativeui_attach_platform target '${NUI_TARGET}' has type '${_nativeui_target_type}'. "
      "The platform bridge may only attach to a final target (EXECUTABLE, MODULE_LIBRARY, or SHARED_LIBRARY)")
  endif()

  _nativeui_validate_consumer_id("${NUI_CONSUMER_ID}")

  get_property(_nativeui_attached TARGET "${NUI_TARGET}"
    PROPERTY NATIVEUI_CONSUMER_ID SET)
  if(_nativeui_attached)
    get_property(_nativeui_existing_identity TARGET "${NUI_TARGET}"
      PROPERTY NATIVEUI_CONSUMER_ID)
    message(FATAL_ERROR
      "NativeUI target '${NUI_TARGET}' is already attached with CONSUMER_ID '${_nativeui_existing_identity}'; "
      "a second platform attachment is not allowed")
  endif()

  if(NOT COMMAND _nativeui_attach_consumer_platform)
    message(FATAL_ERROR
      "NativeUI package is missing its consumer platform implementation")
  endif()

  _nativeui_attach_consumer_platform(
    TARGET "${NUI_TARGET}"
    CONSUMER_ID "${NUI_CONSUMER_ID}"
  )
endfunction()
