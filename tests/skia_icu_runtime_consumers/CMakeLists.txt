cmake_minimum_required(VERSION 3.24)
project(NativeUISkiaIcuRuntimeConsumers LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

function(icu_fixture_warnings target)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /permissive-)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

if(RUNTIME_DATA_MODE STREQUAL "mock")
  if(NOT DEFINED RUNTIME_DATA_MODULE OR NOT EXISTS "${RUNTIME_DATA_MODULE}")
    message(FATAL_ERROR "ICU staging fixture requires RUNTIME_DATA_MODULE")
  endif()
  add_library(runtime_core_stub INTERFACE)
  add_library(NativeUI::Core ALIAS runtime_core_stub)
  set_property(TARGET runtime_core_stub PROPERTY
    NATIVEUI_SKIA_ICU_DATA "${ICU_DATA_SOURCE}")

  add_executable(runtime_one stub_main.cpp)
  add_executable(runtime_two stub_main.cpp)
  add_executable(runtime_shared stub_main.cpp)
  add_library(runtime_library SHARED stub_module.cpp)
  add_library(runtime_module MODULE stub_module.cpp)
  set_target_properties(runtime_one runtime_shared PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/one")
  set_target_properties(runtime_two PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/two")
  set_target_properties(runtime_library runtime_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/module"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/module")
  foreach(_target IN ITEMS runtime_one runtime_two runtime_shared
      runtime_library runtime_module)
    icu_fixture_warnings("${_target}")
  endforeach()

  # Parent-created targets are deliberately attached from a child directory.
  # This reproduces nativeui_add_application's public attachment call structure.
  add_subdirectory(attachment)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/icu-artifacts-$<CONFIG>.cmake"
    CONTENT
"set(ICU_RUNTIME_ONE \"$<TARGET_FILE:runtime_one>\")\nset(ICU_RUNTIME_TWO \"$<TARGET_FILE:runtime_two>\")\nset(ICU_RUNTIME_SHARED \"$<TARGET_FILE:runtime_shared>\")\nset(ICU_RUNTIME_LIBRARY \"$<TARGET_FILE:runtime_library>\")\nset(ICU_RUNTIME_MODULE \"$<TARGET_FILE:runtime_module>\")\n")
elseif(RUNTIME_DATA_MODE STREQUAL "windows")
  if(NOT WIN32)
    message(FATAL_ERROR "Real ICU runtime consumers require Windows")
  endif()
  find_package(NativeUI CONFIG REQUIRED)
  if(NOT COMMAND nativeui_attach_runtime_data)
    message(FATAL_ERROR "NativeUI package does not expose nativeui_attach_runtime_data")
  endif()
  get_target_property(_icu_data NativeUI::Core NATIVEUI_SKIA_ICU_DATA)
  if(NOT _icu_data OR NOT EXISTS "${_icu_data}")
    message(FATAL_ERROR "NativeUI package did not export its pinned ICU data source")
  endif()

  add_executable(runtime_core core_main.cpp)
  add_library(runtime_module MODULE core_module.cpp)
  add_executable(runtime_host host.cpp)
  target_link_libraries(runtime_core PRIVATE NativeUI::Core)
  target_link_libraries(runtime_module PRIVATE NativeUI::Core)
  target_compile_definitions(runtime_host PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_target_properties(runtime_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/core")
  set_target_properties(runtime_module PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/module"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/module")
  set_target_properties(runtime_host PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/host")
  foreach(_target IN ITEMS runtime_core runtime_module runtime_host)
    icu_fixture_warnings("${_target}")
  endforeach()
  add_subdirectory(attachment)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/icu-artifacts-$<CONFIG>.cmake"
    CONTENT
"set(ICU_RUNTIME_CORE \"$<TARGET_FILE:runtime_core>\")\nset(ICU_RUNTIME_MODULE \"$<TARGET_FILE:runtime_module>\")\nset(ICU_RUNTIME_HOST \"$<TARGET_FILE:runtime_host>\")\nset(ICU_RUNTIME_DATA_SOURCE \"${_icu_data}\")\n")
else()
  message(FATAL_ERROR "ICU runtime fixture requires mock or windows RUNTIME_DATA_MODE")
endif()
