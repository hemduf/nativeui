cmake_minimum_required(VERSION 3.24)
project(NativeUIT053PrefixConfigure NONE)

if(NOT DEFINED NATIVEUI_SOURCE OR
   NOT EXISTS "${NATIVEUI_SOURCE}/cmake/NativeUIConsumerPlatform.cmake")
  message(FATAL_ERROR "T053 prefix configure fixture requires NATIVEUI_SOURCE")
endif()
if(NOT DEFINED IDENTITY OR IDENTITY STREQUAL "")
  message(FATAL_ERROR "T053 prefix configure fixture requires IDENTITY")
endif()

include("${NATIVEUI_SOURCE}/cmake/NativeUIConsumerPlatform.cmake")
nativeui_compute_objc_runtime_prefix(_prefix "${IDENTITY}")
file(WRITE "${CMAKE_BINARY_DIR}/t053_prefix.txt" "${_prefix}")
