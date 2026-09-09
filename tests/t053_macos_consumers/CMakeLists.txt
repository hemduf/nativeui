cmake_minimum_required(VERSION 3.24)
# The bridge targets are created in this directory by the T053 helper. CMake
# versions supported by NativeUI require a language to be enabled in the
# highest common directory that owns targets compiling that language, so this
# fixture must enable Objective-C at its own project boundary rather than rely
# on NativeUI's nested source-tree project to propagate it upward.
project(NativeUIT053MacConsumers LANGUAGES C CXX OBJC)

if(NOT DEFINED NATIVEUI_SOURCE OR NOT EXISTS "${NATIVEUI_SOURCE}/CMakeLists.txt")
  message(FATAL_ERROR "T053 fixture requires NATIVEUI_SOURCE")
endif()
if(NOT DEFINED PUGL_SOURCE OR NOT EXISTS "${PUGL_SOURCE}/include/pugl/pugl.h")
  message(FATAL_ERROR "T053 fixture requires PUGL_SOURCE")
endif()
if(NOT DEFINED SKIA_ROOT OR NOT EXISTS "${SKIA_ROOT}/include/include/core/SkCanvas.h")
  if(NOT EXISTS "${SKIA_ROOT}/build/include/include/core/SkCanvas.h")
    message(FATAL_ERROR "T053 fixture requires SKIA_ROOT")
  endif()
endif()

# Provide source locations before NativeUI configures so the same T053 module
# can create final-consumer bridges both from this source fixture and, later,
# from T047's installed package layout.
set(NATIVEUI_PUGL_SOURCE "${PUGL_SOURCE}" CACHE PATH "" FORCE)
set(NATIVEUI_SKIA_ROOT "${SKIA_ROOT}" CACHE PATH "" FORCE)
set(NATIVEUI_PUGL_SOURCE_DIR "${PUGL_SOURCE}")
set(NATIVEUI_PLATFORM_SOURCE_ROOT "${NATIVEUI_SOURCE}")
set(NATIVEUI_BUILD_PLATFORM ON CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)

include("${NATIVEUI_SOURCE}/cmake/NativeUIConsumerPlatform.cmake")
add_subdirectory("${NATIVEUI_SOURCE}" nativeui)

add_library(t053_consumer_a MODULE consumer.cpp)
add_library(t053_consumer_b MODULE consumer.cpp)
set_target_properties(t053_consumer_a t053_consumer_b PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

set(_id_a "com.nativeui.t053.consumer-a")
set(_id_b "com.nativeui.t053.consumer-b")
_nativeui_attach_consumer_platform(
  TARGET t053_consumer_a
  CONSUMER_ID "${_id_a}"
  OUT_BRIDGE _bridge_a
)
_nativeui_attach_consumer_platform(
  TARGET t053_consumer_b
  CONSUMER_ID "${_id_b}"
  OUT_BRIDGE _bridge_b
)

if(_bridge_a STREQUAL "" OR _bridge_b STREQUAL "" OR _bridge_a STREQUAL _bridge_b)
  message(FATAL_ERROR "T053 fixture did not create two distinct macOS consumer bridges")
endif()

nativeui_compute_objc_runtime_prefix(_prefix_a "${_id_a}")
nativeui_compute_objc_runtime_prefix(_prefix_b "${_id_b}")
if(_prefix_a STREQUAL _prefix_b)
  message(FATAL_ERROR "T053 fixture generated duplicate Objective-C prefixes")
endif()

find_library(OBJC_LIBRARY objc REQUIRED)
add_executable(t053_consumer_loader loader.cpp)
target_link_libraries(t053_consumer_loader PRIVATE "${OBJC_LIBRARY}" ${CMAKE_DL_LIBS})

# Generator expressions capture exact built artifact paths without assuming
# archive/module suffixes or multi-config layout.
file(GENERATE
  OUTPUT "${CMAKE_BINARY_DIR}/t053_artifacts.cmake"
  CONTENT
"set(T053_BRIDGE_A \"$<TARGET_FILE:${_bridge_a}>\")\nset(T053_BRIDGE_B \"$<TARGET_FILE:${_bridge_b}>\")\nset(T053_MODULE_A \"$<TARGET_FILE:t053_consumer_a>\")\nset(T053_MODULE_B \"$<TARGET_FILE:t053_consumer_b>\")\nset(T053_LOADER \"$<TARGET_FILE:t053_consumer_loader>\")\nset(T053_CORE \"$<TARGET_FILE:nativeui_core>\")\nset(T053_PREFIX_A \"${_prefix_a}\")\nset(T053_PREFIX_B \"${_prefix_b}\")\n")
