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
  foreach(_kind IN ITEMS CLASS METACLASS)
    set(_expected_symbol "OBJC_${_kind}_$_${PREFIX}${_class}")
    string(FIND "${_nm_output}" "${_expected_symbol}" _prefixed_index)
    if(_prefixed_index EQUAL -1)
      message(FATAL_ERROR
        "Expected prefixed Objective-C runtime symbol ${_expected_symbol} in ${ARCHIVE}")
    endif()
  endforeach()
endforeach()

# Pattern-based future-proof gate: every Objective-C class/metaclass emitted by
# Pugl must have the consumer prefix before the literal Pugl class stem. This
# deliberately does not enumerate known classes, so a newly added Pugl runtime
# class in a future pinned source update fails until it is covered by T053's
# compile-time rename contract.
string(REGEX MATCHALL
  "OBJC_(CLASS|METACLASS)_\\$_Pugl[A-Za-z0-9_]*"
  _unprefixed_pugl_runtime_symbols
  "${_nm_output}")
if(_unprefixed_pugl_runtime_symbols)
  list(REMOVE_DUPLICATES _unprefixed_pugl_runtime_symbols)
  list(JOIN _unprefixed_pugl_runtime_symbols ", " _unprefixed_summary)
  message(FATAL_ERROR
    "Unprefixed Pugl Objective-C runtime symbols are still exported by ${ARCHIVE}: ${_unprefixed_summary}")
endif()

message(STATUS
  "NativeUI Objective-C runtime classes/metaclasses use consumer prefix '${PREFIX}' and no unprefixed Pugl class/metaclass symbols remain")
