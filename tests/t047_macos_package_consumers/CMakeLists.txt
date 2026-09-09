cmake_minimum_required(VERSION 3.24)
project(NativeUIT047MacPackageConsumers LANGUAGES C CXX OBJC)

find_package(NativeUI CONFIG REQUIRED)
if(NOT TARGET NativeUI::Core)
  message(FATAL_ERROR "T047 installed package did not export NativeUI::Core")
endif()
if(TARGET NativeUI::NativeUI)
  message(FATAL_ERROR "T047 installed package must not export NativeUI::NativeUI")
endif()
if(NOT COMMAND nativeui_attach_platform)
  message(FATAL_ERROR "T047 installed package did not expose nativeui_attach_platform")
endif()

add_library(t047_consumer_a MODULE consumer.cpp)
add_library(t047_consumer_b MODULE consumer.cpp)
set_target_properties(t047_consumer_a t047_consumer_b PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

set(_id_a "com.nativeui.t047.consumer-a")
set(_id_b "com.nativeui.t047.consumer-b")
nativeui_attach_platform(TARGET t047_consumer_a CONSUMER_ID "${_id_a}")
nativeui_attach_platform(TARGET t047_consumer_b CONSUMER_ID "${_id_b}")

get_target_property(_bridge_a t047_consumer_a NATIVEUI_OBJC_BRIDGE_TARGET)
get_target_property(_bridge_b t047_consumer_b NATIVEUI_OBJC_BRIDGE_TARGET)
get_target_property(_prefix_a t047_consumer_a NATIVEUI_OBJC_RUNTIME_PREFIX)
get_target_property(_prefix_b t047_consumer_b NATIVEUI_OBJC_RUNTIME_PREFIX)
if(NOT _bridge_a OR NOT _bridge_b OR _bridge_a STREQUAL _bridge_b)
  message(FATAL_ERROR "T047 installed package did not create distinct consumer bridges")
endif()
if(NOT _prefix_a OR NOT _prefix_b OR _prefix_a STREQUAL _prefix_b)
  message(FATAL_ERROR "T047 installed package did not create distinct consumer runtime prefixes")
endif()

find_library(OBJC_LIBRARY objc REQUIRED)
add_executable(t047_consumer_loader loader.cpp)
target_link_libraries(t047_consumer_loader PRIVATE "${OBJC_LIBRARY}" ${CMAKE_DL_LIBS})

file(GENERATE
  OUTPUT "${CMAKE_BINARY_DIR}/t047_artifacts.cmake"
  CONTENT
"set(T047_BRIDGE_A \"$<TARGET_FILE:${_bridge_a}>\")\nset(T047_BRIDGE_B \"$<TARGET_FILE:${_bridge_b}>\")\nset(T047_MODULE_A \"$<TARGET_FILE:t047_consumer_a>\")\nset(T047_MODULE_B \"$<TARGET_FILE:t047_consumer_b>\")\nset(T047_LOADER \"$<TARGET_FILE:t047_consumer_loader>\")\nset(T047_CORE \"$<TARGET_FILE:NativeUI::Core>\")\nset(T047_PREFIX_A \"${_prefix_a}\")\nset(T047_PREFIX_B \"${_prefix_b}\")\n")
