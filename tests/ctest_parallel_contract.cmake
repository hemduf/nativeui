cmake_minimum_required(VERSION 3.24)

# Validate generated CTest metadata instead of source-code text patterns.
if(NOT DEFINED BUILD_DIR OR BUILD_DIR STREQUAL "")
  message(FATAL_ERROR "BUILD_DIR is required")
endif()
find_program(_nativeui_ctest ctest REQUIRED)
execute_process(
  COMMAND "${_nativeui_ctest}" --test-dir "${BUILD_DIR}" --show-only=json-v1
  RESULT_VARIABLE _status
  OUTPUT_VARIABLE _json
  ERROR_VARIABLE _diagnostic
)
if(NOT _status EQUAL 0)
  message(FATAL_ERROR "CTest metadata discovery failed: ${_diagnostic}")
endif()
string(JSON _count ERROR_VARIABLE _json_error LENGTH "${_json}" tests)
if(_json_error OR _count LESS 1)
  message(FATAL_ERROR "CTest test inventory is missing or invalid: ${_json_error}")
endif()

set(_exclusive_native_tests
  nativeui_smoke_standalone
  nativeui_smoke_embedded
  nativeui_image_texture_gpu_reference_tests
  nativeui_t088_noise_gpu_reference_tests
  nativeui_t089_perlin_noise_gpu_reference_tests
  nativeui_t090_simplex_noise_gpu_reference_tests
  nativeui_t091_worley_noise_gpu_reference_tests
  nativeui_fractal_noise_gpu_reference_tests
  nativeui_scalar_source_gpu_reference_tests
  nativeui_t095_scene_gpu_tests
  nativeui_t096_scene_gpu_tests
  nativeui_render_resource_gpu_tests
)
set(_required_serial nativeui_skia_icu_runtime_tests)
if(APPLE)
  list(APPEND _exclusive_native_tests
    nativeui_embedded_visibility_tests
    nativeui_smoke_macos_drop
  )
  list(APPEND _required_serial
    nativeui_embedded_keyboard_tests
    nativeui_embedded_keyboard_live_tests
  )
endif()

set(_seen_exclusive "")
set(_seen_serial "")
math(EXPR _last "${_count} - 1")
foreach(_index RANGE 0 ${_last})
  string(JSON _test_name GET "${_json}" tests ${_index} name)
  list(FIND _exclusive_native_tests "${_test_name}" _exclusive_index)
  list(FIND _required_serial "${_test_name}" _serial_index)
  if(_exclusive_index EQUAL -1 AND _serial_index EQUAL -1)
    continue()
  endif()
  set(_has_lock FALSE)
  set(_has_both_processors FALSE)
  set(_has_serial FALSE)
  string(JSON _properties_count LENGTH "${_json}" tests ${_index} properties)
  if(_properties_count GREATER 0)
    math(EXPR _properties_last "${_properties_count} - 1")
    foreach(_property_index RANGE 0 ${_properties_last})
      string(JSON _name GET "${_json}" tests ${_index} properties ${_property_index} name)
      string(JSON _value GET "${_json}" tests ${_index} properties ${_property_index} value)
      if(_name STREQUAL "RESOURCE_LOCK" AND _value MATCHES "native_display")
        set(_has_lock TRUE)
      elseif(_name STREQUAL "PROCESSORS" AND _value STREQUAL "2")
        set(_has_both_processors TRUE)
      elseif(_name STREQUAL "RUN_SERIAL" AND _value)
        set(_has_serial TRUE)
      endif()
    endforeach()
  endif()
  if(NOT _exclusive_index EQUAL -1)
    if(NOT _has_lock OR NOT _has_both_processors)
      message(FATAL_ERROR "${_test_name} must hold native_display and reserve both test slots")
    endif()
    list(APPEND _seen_exclusive "${_test_name}")
  endif()
  if(NOT _serial_index EQUAL -1)
    if(NOT _has_serial)
      message(FATAL_ERROR "${_test_name} must retain RUN_SERIAL")
    endif()
    list(APPEND _seen_serial "${_test_name}")
  endif()
endforeach()

foreach(_test IN LISTS _exclusive_native_tests)
  if(NOT _test IN_LIST _seen_exclusive)
    message(FATAL_ERROR "Required native test is missing: ${_test}")
  endif()
endforeach()
foreach(_test IN LISTS _required_serial)
  if(NOT _test IN_LIST _seen_serial)
    message(FATAL_ERROR "Required serial test is missing: ${_test}")
  endif()
endforeach()
message(STATUS "CTest scheduling contract passed for ${_count} registered tests")
