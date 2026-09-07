if(NOT DEFINED ARCHIVE OR NOT EXISTS "${ARCHIVE}")
  message(FATAL_ERROR "NativeUI Objective-C prefix check: archive not found: ${ARCHIVE}")
endif()

if(NOT DEFINED NM OR NM STREQUAL "")
  message(FATAL_ERROR "NativeUI Objective-C prefix check: CMAKE_NM was not provided")
endif()

if(NOT DEFINED PREFIX OR PREFIX STREQUAL "")
  message(FATAL_ERROR "NativeUI Objective-C prefix check: PREFIX is empty")
endif()

execute_process(
  COMMAND "${NM}" -g "${ARCHIVE}"
  RESULT_VARIABLE _nm_result
  OUTPUT_VARIABLE _nm_output
  ERROR_VARIABLE _nm_error
)

if(NOT _nm_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI Objective-C prefix check: nm failed (${_nm_result})\n${_nm_error}")
endif()

foreach(_class IN ITEMS PuglWindow PuglWindowDelegate PuglWrapperView PuglOpenGLView)
  string(FIND "${_nm_output}" "${PREFIX}${_class}" _prefixed_index)
  if(_prefixed_index EQUAL -1)
    message(FATAL_ERROR
      "Expected prefixed Objective-C runtime class ${PREFIX}${_class} in ${ARCHIVE}")
  endif()

  string(FIND "${_nm_output}" "OBJC_CLASS_$_${_class}" _unprefixed_class_index)
  string(FIND "${_nm_output}" "OBJC_METACLASS_$_${_class}" _unprefixed_meta_index)
  if(NOT _unprefixed_class_index EQUAL -1 OR NOT _unprefixed_meta_index EQUAL -1)
    message(FATAL_ERROR
      "Unprefixed Objective-C runtime class ${_class} is still exported by ${ARCHIVE}")
  endif()
endforeach()

message(STATUS
  "NativeUI Objective-C runtime classes use consumer prefix '${PREFIX}'")
