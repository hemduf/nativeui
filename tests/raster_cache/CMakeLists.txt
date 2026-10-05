cmake_minimum_required(VERSION 3.24)

# The epoch contract can run without acquiring Skia/Pugl. The real retained-tree
# integration test is additionally registered by the normal NativeUI build.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(NativeUIRasterCacheEpochTests LANGUAGES CXX)
  enable_testing()
  option(NATIVEUI_ENABLE_SANITIZERS "Enable ASan and UBSan for epoch tests" OFF)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

function(nativeui_configure_raster_cache_test target)
  target_compile_features(${target} PRIVATE cxx_std_20)
  if(COMMAND nativeui_enable_project_warnings)
    nativeui_enable_project_warnings(${target})
    nativeui_enable_sanitizers(${target})
  else()
    if(MSVC)
      target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(NATIVEUI_ENABLE_SANITIZERS)
      if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE
          -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
      else()
        message(FATAL_ERROR "Standalone epoch sanitizer tests require Clang or GCC")
      endif()
    endif()
  endif()
  add_test(NAME ${target} COMMAND ${target})
  set_tests_properties(${target} PROPERTIES LABELS "unit;raster-cache" TIMEOUT 60)
endfunction()

add_executable(nativeui_raster_cache_epoch_tests epoch_tests.cpp)
target_include_directories(nativeui_raster_cache_epoch_tests PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
nativeui_configure_raster_cache_test(nativeui_raster_cache_epoch_tests)

if(TARGET NativeUI::Core)
  add_executable(nativeui_raster_cache_tree_tests tree_tests.cpp)
  target_link_libraries(nativeui_raster_cache_tree_tests PRIVATE NativeUI::Core)
  nativeui_configure_raster_cache_test(nativeui_raster_cache_tree_tests)
endif()
