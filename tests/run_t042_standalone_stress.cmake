if(NOT DEFINED EXE OR EXE STREQUAL "")
  message(FATAL_ERROR "run_t042_standalone_stress.cmake requires -DEXE=<stress executable>")
endif()

set(_cycles 50)
math(EXPR _last_cycle "${_cycles} - 1")

foreach(_cycle RANGE 0 ${_last_cycle})
  execute_process(
    COMMAND "${EXE}" --standalone-once "${_cycle}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 20
  )

  if(NOT _result STREQUAL "0")
    string(STRIP "${_stdout}" _stdout)
    string(STRIP "${_stderr}" _stderr)
    message(FATAL_ERROR
      "standalone_sequential_50 cycle=${_cycle} transition=process-isolated-lifecycle failed "
      "with exit=${_result}\nstdout:\n${_stdout}\nstderr:\n${_stderr}")
  endif()
endforeach()

message(STATUS "PASS standalone_sequential_50: ${_cycles} process-isolated PROGRAM lifetimes")
