if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T053 registration tests require SOURCE_DIR")
endif()

function(_nativeui_expect_registration_failure case expected_text)
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
  string(FIND "${_combined}" "${expected_text}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR
      "T053 ${case} failed for the wrong reason\nexpected diagnostic: ${expected_text}\noutput:\n${_combined}")
  endif()
endfunction()

_nativeui_expect_registration_failure(
  duplicate_identity
  "CONSUMER_ID 'com.example.shared' is already registered to target 'consumer_a'; target 'consumer_b' cannot reuse it")
_nativeui_expect_registration_failure(
  duplicate_target
  "target 'consumer_a' already has consumer identity 'com.example.first'; duplicate attachment is not allowed")

message(STATUS "T053 configure-time registration isolation passed")
