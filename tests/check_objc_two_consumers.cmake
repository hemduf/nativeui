if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "NativeUI two-consumer Objective-C check: invalid SOURCE_DIR: ${SOURCE_DIR}")
endif()

if(NOT DEFINED OUTER_BUILD OR NOT IS_DIRECTORY "${OUTER_BUILD}")
  message(FATAL_ERROR "NativeUI two-consumer Objective-C check: invalid OUTER_BUILD: ${OUTER_BUILD}")
endif()

# CPM normally materializes dependencies below the build tree. When
# CPM_SOURCE_CACHE is enabled, however, DOWNLOAD_ONLY packages live under the
# shared cache instead. Resolve either layout so this runtime-isolation check
# validates the configured dependency instead of assuming one CPM storage mode.
function(nativeui_find_dependency_root out_var build_candidate cache_package)
  set(_candidates "${build_candidate}")
  if(DEFINED ENV{CPM_SOURCE_CACHE} AND NOT "$ENV{CPM_SOURCE_CACHE}" STREQUAL "")
    file(GLOB _cache_candidates LIST_DIRECTORIES true
      "$ENV{CPM_SOURCE_CACHE}/${cache_package}/*")
    list(APPEND _candidates ${_cache_candidates})
  endif()

  foreach(_candidate IN LISTS _candidates)
    foreach(_marker ${ARGN})
      if(EXISTS "${_candidate}/${_marker}")
        set(${out_var} "${_candidate}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
  endforeach()

  set(${out_var} "" PARENT_SCOPE)
endfunction()

nativeui_find_dependency_root(
  _pugl_source
  "${OUTER_BUILD}/_deps/pugl_src-src"
  pugl_src
  "include/pugl/pugl.h")
if(NOT _pugl_source)
  message(FATAL_ERROR
    "NativeUI two-consumer Objective-C check: configured Pugl source not found in build tree or CPM_SOURCE_CACHE")
endif()

nativeui_find_dependency_root(
  _skia_root
  "${OUTER_BUILD}/_deps/skia_prebuilt-src"
  skia_prebuilt
  "include/include/core/SkCanvas.h"
  "build/include/include/core/SkCanvas.h")
if(NOT _skia_root)
  message(FATAL_ERROR
    "NativeUI two-consumer Objective-C check: configured Skia package not found in build tree or CPM_SOURCE_CACHE")
endif()

find_program(_nm NAMES nm REQUIRED)
set(_fixture_source "${SOURCE_DIR}/tests/t053_macos_consumers")
set(_fixture_build "${OUTER_BUILD}/t053-macos-consumers")
file(REMOVE_RECURSE "${_fixture_build}")

execute_process(
  COMMAND "${CMAKE_COMMAND}"
    -S "${_fixture_source}"
    -B "${_fixture_build}"
    -G Ninja
    -DCMAKE_BUILD_TYPE=Release
    "-DNATIVEUI_SOURCE=${SOURCE_DIR}"
    "-DPUGL_SOURCE=${_pugl_source}"
    "-DSKIA_ROOT=${_skia_root}"
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_output
  ERROR_VARIABLE _configure_error
)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI T053 two-consumer configure failed (${_configure_result})\n"
    "${_configure_output}\n${_configure_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_fixture_build}"
    --target t053_consumer_a t053_consumer_b t053_consumer_loader
    --config Release --parallel 2
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_output
  ERROR_VARIABLE _build_error
)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI T053 two-consumer build failed (${_build_result})\n"
    "${_build_output}\n${_build_error}")
endif()

set(_artifacts "${_fixture_build}/t053_artifacts.cmake")
if(NOT EXISTS "${_artifacts}")
  message(FATAL_ERROR "NativeUI T053 fixture did not generate ${_artifacts}")
endif()
include("${_artifacts}")

foreach(_required IN ITEMS
    T053_BRIDGE_A T053_BRIDGE_B T053_MODULE_A T053_MODULE_B T053_LOADER T053_CORE)
  if(NOT DEFINED ${_required} OR NOT EXISTS "${${_required}}")
    message(FATAL_ERROR
      "NativeUI T053 fixture artifact ${_required} is missing: ${${_required}}")
  endif()
endforeach()
if(NOT DEFINED T053_PREFIX_A OR T053_PREFIX_A STREQUAL "" OR
   NOT DEFINED T053_PREFIX_B OR T053_PREFIX_B STREQUAL "" OR
   T053_PREFIX_A STREQUAL T053_PREFIX_B)
  message(FATAL_ERROR "NativeUI T053 fixture produced invalid/duplicate consumer prefixes")
endif()
if(T053_BRIDGE_A STREQUAL T053_BRIDGE_B)
  message(FATAL_ERROR "NativeUI T053 consumers unexpectedly share one Objective-C bridge archive")
endif()

foreach(_label IN ITEMS A B)
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      "-DARCHIVE=${T053_BRIDGE_${_label}}"
      "-DNM=${_nm}"
      "-DPREFIX=${T053_PREFIX_${_label}}"
      -P "${SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
    RESULT_VARIABLE _prefix_result
    OUTPUT_VARIABLE _prefix_output
    ERROR_VARIABLE _prefix_error
  )
  if(NOT _prefix_result EQUAL 0)
    message(FATAL_ERROR
      "NativeUI T053 consumer ${_label} symbol audit failed (${_prefix_result})\n"
      "${_prefix_output}\n${_prefix_error}")
  endif()
endforeach()

execute_process(
  COMMAND "${_nm}" -g "${T053_BRIDGE_A}"
  RESULT_VARIABLE _nm_a_result
  OUTPUT_VARIABLE _nm_a_output
  ERROR_VARIABLE _nm_a_error
)
execute_process(
  COMMAND "${_nm}" -g "${T053_BRIDGE_B}"
  RESULT_VARIABLE _nm_b_result
  OUTPUT_VARIABLE _nm_b_output
  ERROR_VARIABLE _nm_b_error
)
if(NOT _nm_a_result EQUAL 0 OR NOT _nm_b_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI T053 two-consumer nm failed\n${_nm_a_error}\n${_nm_b_error}")
endif()
string(FIND "${_nm_a_output}" "${T053_PREFIX_B}Pugl" _b_in_a)
string(FIND "${_nm_b_output}" "${T053_PREFIX_A}Pugl" _a_in_b)
if(NOT _b_in_a EQUAL -1 OR NOT _a_in_b EQUAL -1)
  message(FATAL_ERROR "NativeUI T053 consumer Objective-C namespaces overlap")
endif()

# Loading both final MODULE images in one process proves that the Objective-C
# runtime accepts both consumer namespaces simultaneously. The loader also
# rejects any generic Pugl class that escaped compile-time prefixing.
execute_process(
  COMMAND "${T053_LOADER}"
    "${T053_MODULE_A}" "${T053_PREFIX_A}"
    "${T053_MODULE_B}" "${T053_PREFIX_B}"
  RESULT_VARIABLE _load_result
  OUTPUT_VARIABLE _load_output
  ERROR_VARIABLE _load_error
)
if(NOT _load_result EQUAL 0)
  message(FATAL_ERROR
    "NativeUI T053 two-consumer runtime load failed (${_load_result})\n"
    "${_load_output}\n${_load_error}")
endif()

message(STATUS
  "NativeUI T053 two-consumer isolation passed: distinct bridge archives, one shared Core, symbol audit clean, both final modules coexist in one Objective-C runtime")
