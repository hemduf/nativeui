if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T053 registration tests require SOURCE_DIR")
endif()

function(_nativeui_expect_registration_failure case)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DSOURCE_DIR=${SOURCE_DIR}"
      "-DCASE=${case}"
      -P "${SOURCE_DIR}/tests/t053_registration_case.cmake"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  if(_result EQUAL 0)
    message(FATAL_ERROR "T053 ${case} unexpectedly succeeded")
  endif()
  set(_combined "${_stdout}\n${_stderr}")
  string(REGEX REPLACE "[ \t\r\n]+" " " _normalized "${_combined}")
  foreach(_expected IN LISTS ARGN)
    string(FIND "${_normalized}" "${_expected}" _found)
    if(_found EQUAL -1)
      message(FATAL_ERROR
        "T053 ${case} failed for the wrong reason\nmissing diagnostic fragment: ${_expected}\noutput:\n${_combined}")
    endif()
  endforeach()
endfunction()

_nativeui_expect_registration_failure(
  duplicate_identity
  "CONSUMER_ID 'com.example.shared'"
  "target 'consumer_a'"
  "target 'consumer_b'"
  "cannot reuse it")
_nativeui_expect_registration_failure(
  duplicate_target
  "target 'consumer_a'"
  "consumer identity 'com.example.first'"
  "duplicate attachment is not allowed")

message(STATUS "T053 configure-time registration isolation passed")
