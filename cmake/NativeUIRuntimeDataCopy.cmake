cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED NATIVEUI_DATA_SOURCE OR
   NOT EXISTS "${NATIVEUI_DATA_SOURCE}" OR IS_DIRECTORY "${NATIVEUI_DATA_SOURCE}")
  message(FATAL_ERROR "NativeUI matching Skia ICU runtime data is missing: ${NATIVEUI_DATA_SOURCE}")
endif()
if(NOT DEFINED NATIVEUI_DATA_DIRECTORY OR NATIVEUI_DATA_DIRECTORY STREQUAL "")
  message(FATAL_ERROR "NativeUI runtime data destination is missing")
endif()
file(MAKE_DIRECTORY "${NATIVEUI_DATA_DIRECTORY}")
# Several consumers may share one output directory. Serialize their build-time
# copies; no lock or file write is added to a NativeUI/audio runtime callback.
file(LOCK "${NATIVEUI_DATA_DIRECTORY}/.nativeui-icu-data.lock"
  GUARD PROCESS TIMEOUT 60 RESULT_VARIABLE _nativeui_lock_result)
if(NOT _nativeui_lock_result STREQUAL "0")
  message(FATAL_ERROR "NativeUI ICU data staging lock failed: ${_nativeui_lock_result}")
endif()
file(COPY_FILE "${NATIVEUI_DATA_SOURCE}"
  "${NATIVEUI_DATA_DIRECTORY}/icudtl.dat"
  ONLY_IF_DIFFERENT RESULT _nativeui_copy_result)
if(NOT _nativeui_copy_result STREQUAL "0")
  message(FATAL_ERROR "NativeUI ICU data staging failed: ${_nativeui_copy_result}")
endif()
