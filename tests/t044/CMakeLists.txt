cmake_minimum_required(VERSION 3.24)

if(APPLE)
  project(NativeUIT044PointerCapture LANGUAGES C CXX OBJC OBJCXX)
else()
  project(NativeUIT044PointerCapture LANGUAGES C CXX)
endif()

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM ON CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)
enable_testing()

# Recover the exact pinned Pugl source root from the target created by NativeUI.
# The platform attachment helper intentionally consumes this source tree instead
# of guessing a CPM build-directory layout.
if(TARGET nativeui_pugl_common)
  get_target_property(_t044_pugl_sources nativeui_pugl_common SOURCES)
elseif(TARGET nativeui_pugl)
  get_target_property(_t044_pugl_sources nativeui_pugl SOURCES)
else()
  message(FATAL_ERROR "T044 pointer capture smoke could not locate NativeUI's Pugl target")
endif()
list(GET _t044_pugl_sources 0 _t044_pugl_source)
get_filename_component(_t044_pugl_src_dir "${_t044_pugl_source}" DIRECTORY)
get_filename_component(NATIVEUI_PUGL_SOURCE_DIR "${_t044_pugl_src_dir}" DIRECTORY)
if(NOT EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
  message(FATAL_ERROR "T044 derived invalid Pugl source root: ${NATIVEUI_PUGL_SOURCE_DIR}")
endif()

set(_t044_source "${NATIVEUI_SOURCE_DIR}/tests/t044_pointer_capture_smoke.cpp")
if(APPLE)
  set_source_files_properties("${_t044_source}" PROPERTIES LANGUAGE OBJCXX)
endif()

add_executable(nativeui_t044_pointer_capture_smoke "${_t044_source}")
_nativeui_attach_consumer_platform(
  TARGET nativeui_t044_pointer_capture_smoke
  CONSUMER_ID org.nativeui.test.t044-pointer-capture
)
nativeui_enable_project_warnings(nativeui_t044_pointer_capture_smoke)
nativeui_enable_sanitizers(nativeui_t044_pointer_capture_smoke)

if(APPLE)
  target_compile_options(nativeui_t044_pointer_capture_smoke PRIVATE -fobjc-arc)
  find_library(T044_APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
  target_link_libraries(nativeui_t044_pointer_capture_smoke PRIVATE
    "${T044_APPLICATION_SERVICES_FRAMEWORK}"
  )
elseif(WIN32)
  target_link_libraries(nativeui_t044_pointer_capture_smoke PRIVATE user32)
elseif(UNIX)
  find_package(X11 REQUIRED)
  find_library(T044_XTST_LIBRARY NAMES Xtst REQUIRED)
  target_link_libraries(nativeui_t044_pointer_capture_smoke PRIVATE
    X11::X11
    "${T044_XTST_LIBRARY}"
  )
endif()

add_test(
  NAME nativeui_t044_pointer_capture_smoke
  COMMAND nativeui_t044_pointer_capture_smoke
)
set_tests_properties(nativeui_t044_pointer_capture_smoke PROPERTIES
  LABELS "integration;platform;input;t044"
  TIMEOUT 45
)
