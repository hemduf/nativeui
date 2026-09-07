if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "NativeUI two-consumer Objective-C check: invalid SOURCE_DIR: ${SOURCE_DIR}")
endif()

if(NOT DEFINED OUTER_BUILD OR NOT IS_DIRECTORY "${OUTER_BUILD}")
  message(FATAL_ERROR "NativeUI two-consumer Objective-C check: invalid OUTER_BUILD: ${OUTER_BUILD}")
endif()

set(_pugl_source "${OUTER_BUILD}/_deps/pugl_src-src")
if(NOT EXISTS "${_pugl_source}/include/pugl/pugl.h")
  message(FATAL_ERROR
    "NativeUI two-consumer Objective-C check: configured Pugl source not found at ${_pugl_source}")
endif()

set(_skia_root "${OUTER_BUILD}/_deps/skia_prebuilt-src")
if(NOT EXISTS "${_skia_root}/include/include/core/SkCanvas.h"
   AND NOT EXISTS "${_skia_root}/build/include/include/core/SkCanvas.h")
  message(FATAL_ERROR
    "NativeUI two-consumer Objective-C check: configured Skia package not found at ${_skia_root}")
endif()

find_program(_nm NAMES nm REQUIRED)

function(_nativeui_build_prefixed_consumer label prefix out_archive)
  string(TOLOWER "${label}" _label_lower)
  set(_build_dir "${OUTER_BUILD}/objc-consumer-${_label_lower}")
  file(REMOVE_RECURSE "${_build_dir}")

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${SOURCE_DIR}"
      -B "${_build_dir}"
      -G Ninja
      -DCMAKE_BUILD_TYPE=Release
      -DNATIVEUI_BUILD_PLATFORM=ON
      -DNATIVEUI_BUILD_EXAMPLES=OFF
      -DNATIVEUI_BUILD_TESTS=OFF
      "-DNATIVEUI_PUGL_SOURCE=${_pugl_source}"
      "-DNATIVEUI_SKIA_ROOT=${_skia_root}"
      "-DNATIVEUI_OBJC_RUNTIME_PREFIX=${prefix}"
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
  )
  if(NOT _configure_result EQUAL 0)
    message(FATAL_ERROR
      "NativeUI ${label} consumer configure failed (${_configure_result})\n"
      "${_configure_output}\n${_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build_dir}"
      --target nativeui_pugl --config Release --parallel 2
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error
  )
  if(NOT _build_result EQUAL 0)
    message(FATAL_ERROR
      "NativeUI ${label} consumer Pugl build failed (${_build_result})\n"
      "${_build_output}\n${_build_error}")
  endif()

  set(_archive "${_build_dir}/libnativeui_pugl.a")
  if(NOT EXISTS "${_archive}")
    file(GLOB_RECURSE _archive_candidates LIST_DIRECTORIES FALSE
      "${_build_dir}/*nativeui_pugl*.a")
    list(LENGTH _archive_candidates _archive_count)
    if(NOT _archive_count EQUAL 1)
      message(FATAL_ERROR
        "NativeUI ${label} consumer: expected one nativeui_pugl archive, found ${_archive_count}")
    endif()
    list(GET _archive_candidates 0 _archive)
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DARCHIVE=${_archive}"
      "-DNM=${_nm}"
      "-DPREFIX=${prefix}"
      -P "${SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
    RESULT_VARIABLE _prefix_result
    OUTPUT_VARIABLE _prefix_output
    ERROR_VARIABLE _prefix_error
  )
  if(NOT _prefix_result EQUAL 0)
    message(FATAL_ERROR
      "NativeUI ${label} consumer prefix validation failed (${_prefix_result})\n"
      "${_prefix_output}\n${_prefix_error}")
  endif()

  set(${out_archive} "${_archive}" PARENT_SCOPE)
endfunction()

set(_prefix_a "NativeUITestConsumerA_")
set(_prefix_b "NativeUITestConsumerB_")
_nativeui_build_prefixed_consumer("A" "${_prefix_a}" _archive_a)
_nativeui_build_prefixed_consumer("B" "${_prefix_b}" _archive_b)

execute_process(
  COMMAND "${_nm}" -g "${_archive_a}"
  RESULT_VARIABLE _nm_a_result
  OUTPUT_VARIABLE _nm_a_output
  ERROR_VARIABLE _nm_a_error
)
execute_process(
  COMMAND "${_nm}" -g "${_archive_b}"
  RESULT_VARIABLE _nm_b_result
  OUTPUT_VARIABLE _nm_b_output
  ERROR_VARIABLE _nm_b_error
)
if(NOT _nm_a_result EQUAL 0 OR NOT _nm_b_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI two-consumer Objective-C check: nm failed\n${_nm_a_error}\n${_nm_b_error}")
endif()

foreach(_class IN ITEMS PuglWindow PuglWindowDelegate PuglWrapperView PuglOpenGLView)
  string(FIND "${_nm_a_output}" "${_prefix_b}${_class}" _b_in_a)
  string(FIND "${_nm_b_output}" "${_prefix_a}${_class}" _a_in_b)
  if(NOT _b_in_a EQUAL -1 OR NOT _a_in_b EQUAL -1)
    message(FATAL_ERROR
      "NativeUI two-consumer Objective-C check: consumer runtime namespaces overlap for ${_class}")
  endif()
endforeach()

message(STATUS
  "NativeUI two-consumer Objective-C runtime isolation passed: ${_prefix_a} and ${_prefix_b}")
