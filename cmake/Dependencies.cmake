include_guard(GLOBAL)

# Windowing dependencies are optional so the core test suite can be configured
# and run without Pugl, OpenGL, X11/AppKit/Win32, or a display.
if(NATIVEUI_BUILD_PLATFORM)
  # -----------------------------------------------------------------------------
  # Pugl: source dependency managed by CPM, compiled statically by NativeUI.
  # -----------------------------------------------------------------------------
  set(NATIVEUI_PUGL_COMMIT
      "195f79b22644010c81a5e0c3231c591856787ec6"
      CACHE STRING "Pinned hemduf/pugl commit")
  set(NATIVEUI_PUGL_SOURCE "" CACHE PATH "Use an already available Pugl source tree")

  if(NATIVEUI_PUGL_SOURCE)
    set(pugl_src_SOURCE_DIR "${NATIVEUI_PUGL_SOURCE}")
  else()
    CPMAddPackage(
      NAME pugl_src
      GITHUB_REPOSITORY hemduf/pugl
      GIT_TAG ${NATIVEUI_PUGL_COMMIT}
      DOWNLOAD_ONLY YES
    )
  endif()

  if(NOT EXISTS "${pugl_src_SOURCE_DIR}/include/pugl/pugl.h")
    message(FATAL_ERROR "Invalid Pugl source tree: ${pugl_src_SOURCE_DIR}")
  endif()
  # T053's consumer platform module and T047's installed-package helper use the
  # same resolved pinned source root. This is build/configure data only.
  set(NATIVEUI_PUGL_SOURCE_DIR "${pugl_src_SOURCE_DIR}")

  find_package(OpenGL REQUIRED)
  add_library(nativeui_opengl INTERFACE)
  add_library(NativeUI::OpenGL ALIAS nativeui_opengl)
  if(TARGET OpenGL::GL)
    target_link_libraries(nativeui_opengl INTERFACE OpenGL::GL)
  elseif(TARGET OpenGL::OpenGL)
    target_link_libraries(nativeui_opengl INTERFACE OpenGL::OpenGL)
    if(TARGET OpenGL::GLX)
      target_link_libraries(nativeui_opengl INTERFACE OpenGL::GLX)
    endif()
  else()
    message(FATAL_ERROR "CMake FindOpenGL did not provide a usable OpenGL target")
  endif()

  if(APPLE)
    # T053 deliberately does not create one generic Objective-C Pugl archive.
    # Only the generic common/internal C sources are shared; mac.m, mac_gl.m
    # and NativeUI's Cocoa IME bridge are compiled by the final-consumer target
    # factory with the derived consumer-specific runtime prefix.
    enable_language(OBJC)
    find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COREVIDEO_FRAMEWORK CoreVideo REQUIRED)
  else()
    set(_pugl_sources
      "${pugl_src_SOURCE_DIR}/src/common.c"
      "${pugl_src_SOURCE_DIR}/src/internal.c"
    )

    if(WIN32)
      list(APPEND _pugl_sources
        "${pugl_src_SOURCE_DIR}/src/win.c"
        "${pugl_src_SOURCE_DIR}/src/win_gl.c"
        "${CMAKE_CURRENT_LIST_DIR}/../src/detail/native_ime_windows.c"
      )
    elseif(UNIX)
      list(APPEND _pugl_sources
        "${pugl_src_SOURCE_DIR}/src/x11.c"
        "${pugl_src_SOURCE_DIR}/src/x11_gl.c"
        "${CMAKE_CURRENT_LIST_DIR}/../src/detail/native_ime_x11.c"
      )
    else()
      message(FATAL_ERROR "NativeUI/Pugl supports macOS, Windows and Linux/X11")
    endif()

    add_library(nativeui_pugl STATIC ${_pugl_sources})
    add_library(NativeUI::Pugl ALIAS nativeui_pugl)
    set_target_properties(nativeui_pugl PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_features(nativeui_pugl PUBLIC c_std_99)
    target_include_directories(nativeui_pugl
      PUBLIC "${pugl_src_SOURCE_DIR}/include"
      PRIVATE "${pugl_src_SOURCE_DIR}/src"
    )
    target_compile_definitions(nativeui_pugl
      PUBLIC PUGL_STATIC
      PRIVATE PUGL_INTERNAL
    )
    target_link_libraries(nativeui_pugl PUBLIC NativeUI::OpenGL)

    if(WIN32)
      target_compile_definitions(nativeui_pugl PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
        WINVER=0x0601 _WIN32_WINNT=0x0601
      )
      target_link_libraries(nativeui_pugl PUBLIC
        dwmapi gdi32 imm32 shell32 shlwapi user32
      )
    else()
      find_package(X11 REQUIRED)
      target_compile_definitions(nativeui_pugl PRIVATE
        _POSIX_C_SOURCE=200809L
        USE_XCURSOR=0
        USE_XRANDR=0
        USE_XSYNC=0
      )
      target_link_libraries(nativeui_pugl PUBLIC X11::X11 ${CMAKE_DL_LIBS})
    endif()
  endif()
endif()

# -----------------------------------------------------------------------------
# Skia: prebuilt static release artifacts from olilarkin/skia-builder.
# The binary archive itself is managed/downloaded by CPM.
# -----------------------------------------------------------------------------
set(NATIVEUI_SKIA_TAG "chrome/m149" CACHE STRING "skia-builder release tag")
set(NATIVEUI_SKIA_ROOT "" CACHE PATH "Use an already extracted skia-builder archive")
set(NATIVEUI_SKIA_WINDOWS_CRT "MD" CACHE STRING "Windows Skia CRT: MD or MT")
set_property(CACHE NATIVEUI_SKIA_WINDOWS_CRT PROPERTY STRINGS MD MT)
set(_nativeui_default_skia_config "Release")
if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(_nativeui_default_skia_config "Debug")
endif()
set(NATIVEUI_SKIA_CONFIG "${_nativeui_default_skia_config}" CACHE STRING "Skia binary configuration")
set_property(CACHE NATIVEUI_SKIA_CONFIG PROPERTY STRINGS Release Debug)

if(WIN32 AND CMAKE_CONFIGURATION_TYPES)
  message(STATUS
    "NativeUI uses one pinned skia-builder configuration per build tree: ${NATIVEUI_SKIA_CONFIG}. "
    "Use a separate build tree with -DNATIVEUI_SKIA_CONFIG=Debug when a Debug Skia CRT is required.")
endif()

if(NOT NATIVEUI_SKIA_ROOT)
  string(TOLOWER "${NATIVEUI_SKIA_CONFIG}" _skia_cfg)

  if(APPLE)
    if(NOT NATIVEUI_SKIA_CONFIG STREQUAL "Release")
      message(FATAL_ERROR "skia-builder chrome/m149 publishes the macOS universal artifact as Release")
    endif()
    set(_skia_asset "skia-build-mac-universal-gpu-release.zip")
    set(_skia_hash "SHA256=1230ef54e0b656e91747299942ddab4b3e69bfdaa6a1feb047d806090965485a")
  elseif(WIN32)
    string(TOUPPER "${NATIVEUI_SKIA_WINDOWS_CRT}" _crt)
    if(_crt STREQUAL "MD")
      set(_suffix "gpu-md")
      if(NATIVEUI_SKIA_CONFIG STREQUAL "Debug")
        set(_skia_hash "SHA256=449a8a7337f46d9a3ead149b4702884e907dcc225e7e09d57df310d447d999de")
      else()
        set(_skia_hash "SHA256=b6ab07feeac73d1ed9063a89bd12d81873bbb0896dd73750d1500e3e239ff020")
      endif()
    elseif(_crt STREQUAL "MT")
      set(_suffix "gpu")
      if(NATIVEUI_SKIA_CONFIG STREQUAL "Debug")
        set(_skia_hash "SHA256=3417c63f6c1ea7014f1369c8961b4e58658f7cf57793c5dd7c6c7a3c1e9a440c")
      else()
        set(_skia_hash "SHA256=f0935746976f19e2ad3b9a75a8bdbbd40bab745b90d66e014332b233a3121cfb")
      endif()
    else()
      message(FATAL_ERROR "NATIVEUI_SKIA_WINDOWS_CRT must be MD or MT")
    endif()
    set(_skia_asset "skia-build-win-x64-${_suffix}-${_skia_cfg}.zip")
  elseif(UNIX)
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
      message(FATAL_ERROR "The pinned skia-builder release currently provides Linux x64 only")
    endif()
    if(NOT NATIVEUI_SKIA_CONFIG STREQUAL "Release")
      message(FATAL_ERROR "skia-builder chrome/m149 publishes the Linux x64 artifact as Release")
    endif()
    set(_skia_asset "skia-build-linux-x64-gpu-release.zip")
    set(_skia_hash "SHA256=43a22804b525829d829b1b32da32a8c635c3a7fd7c257725bac48ff32258a6f0")
  endif()

  set(_skia_url
    "https://github.com/olilarkin/skia-builder/releases/download/${NATIVEUI_SKIA_TAG}/${_skia_asset}")

  CPMAddPackage(
    NAME skia_prebuilt
    URL "${_skia_url}"
    URL_HASH "${_skia_hash}"
    DOWNLOAD_ONLY YES
  )
  set(NATIVEUI_SKIA_ROOT "${skia_prebuilt_SOURCE_DIR}")
endif()

# CMake/FetchContent strips a single common top-level directory when extracting
# archives. skia-builder release zips contain a top-level `build/` directory,
# so a CPM download normally lands as:
#
#   <source>/include/include/core/SkCanvas.h
#   <source>/mac-gpu/...
#
# while a manually extracted archive may still be:
#
#   <root>/build/include/include/core/SkCanvas.h
#   <root>/build/mac-gpu/...
#
# Normalize both layouts to the directory that corresponds to skia-builder's
# original `build/` directory.
if(EXISTS "${NATIVEUI_SKIA_ROOT}/include/include/core/SkCanvas.h")
  set(_skia_package_root "${NATIVEUI_SKIA_ROOT}")
elseif(EXISTS "${NATIVEUI_SKIA_ROOT}/build/include/include/core/SkCanvas.h")
  set(_skia_package_root "${NATIVEUI_SKIA_ROOT}/build")
else()
  message(FATAL_ERROR
    "Invalid skia-builder package at ${NATIVEUI_SKIA_ROOT}. Expected either "
    "include/include/core/SkCanvas.h (CPM-extracted layout) or "
    "build/include/include/core/SkCanvas.h (manual archive layout).")
endif()

# skia-builder preserves Skia's source-relative include paths inside its
# packaged header directory. The consumer therefore adds the outer
# `<package>/include` directory so `#include "include/core/SkCanvas.h"` and
# `#include "src/..."` resolve correctly.
set(_skia_include "${_skia_package_root}/include")

if(APPLE)
  set(_skia_lib_dir "${_skia_package_root}/mac-gpu/lib/Release")
  # Some skia-builder paths contain the original libskia.a while combined
  # Apple packages may additionally contain libSkia.a. Prefer the combined
  # archive when it is present, otherwise use the canonical Skia archive.
  set(_skia_lib_candidates
    "${_skia_lib_dir}/libSkia.a"
    "${_skia_lib_dir}/libskia.a"
  )
elseif(WIN32)
  string(TOUPPER "${NATIVEUI_SKIA_WINDOWS_CRT}" _crt)
  if(_crt STREQUAL "MD")
    set(_variant "win-gpu-md")
  else()
    set(_variant "win-gpu")
  endif()
  set(_skia_lib_dir "${_skia_package_root}/${_variant}/lib/${NATIVEUI_SKIA_CONFIG}/x64")
  set(_skia_lib_candidates
    "${_skia_lib_dir}/Skia.lib"
    "${_skia_lib_dir}/skia.lib"
  )
else()
  set(_skia_lib_dir "${_skia_package_root}/linux-gpu/lib/Release/x64")
  set(_skia_lib_candidates
    "${_skia_lib_dir}/libSkia.a"
    "${_skia_lib_dir}/libskia.a"
  )
endif()

unset(_skia_lib)
foreach(_candidate IN LISTS _skia_lib_candidates)
  if(EXISTS "${_candidate}")
    set(_skia_lib "${_candidate}")
    break()
  endif()
endforeach()

message(STATUS "NativeUI Skia package root: ${_skia_package_root}")
message(STATUS "NativeUI Skia include root: ${_skia_include}")
message(STATUS "NativeUI Skia library directory: ${_skia_lib_dir}")

if(NOT _skia_lib)
  message(FATAL_ERROR
    "Could not find the Skia static library in ${_skia_lib_dir}. "
    "Candidates: ${_skia_lib_candidates}")
endif()
message(STATUS "NativeUI Skia library: ${_skia_lib}")

add_library(SkiaBuilder::skia STATIC IMPORTED GLOBAL)
set_target_properties(SkiaBuilder::skia PROPERTIES
  IMPORTED_LOCATION "${_skia_lib}"
  INTERFACE_INCLUDE_DIRECTORIES "${_skia_include}"
)

if(APPLE)
  find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
  find_library(CORETEXT_FRAMEWORK CoreText REQUIRED)
  find_library(COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
  set_property(TARGET SkiaBuilder::skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "${COREFOUNDATION_FRAMEWORK};${CORETEXT_FRAMEWORK};${COREGRAPHICS_FRAMEWORK}")
elseif(WIN32)
  set_property(TARGET SkiaBuilder::skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "dwrite")
elseif(UNIX AND NOT APPLE)
  find_package(Fontconfig REQUIRED)
  set_property(TARGET SkiaBuilder::skia APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    "Fontconfig::Fontconfig;pthread;${CMAKE_DL_LIBS}")
endif()

# NativeUI-owned targets are warning-free by default. Dependencies above are
# created before this point, so their third-party diagnostics stay outside this
# policy unless a NativeUI consumer/platform target compiles their sources.
set(NATIVEUI_ALLOWED_WARNINGS "" CACHE STRING
    "Semicolon-separated compiler warnings explicitly allowed for NativeUI-owned targets (Clang/GCC names without -W; MSVC numeric codes)")

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
if(MSVC)
  add_compile_options(/W4 /permissive-)
  foreach(_warning IN LISTS NATIVEUI_ALLOWED_WARNINGS)
    if(NOT _warning MATCHES "^[0-9][0-9][0-9][0-9]$")
      message(FATAL_ERROR
        "Invalid NATIVEUI_ALLOWED_WARNINGS entry '${_warning}' for MSVC; use a four-digit warning code such as 4996")
    endif()
    add_compile_options("/wd${_warning}")
    if(_warning STREQUAL "4996")
      add_compile_definitions(NATIVEUI_ALLOW_DEPRECATED_DECLARATIONS=1)
    endif()
  endforeach()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
  foreach(_warning IN LISTS NATIVEUI_ALLOWED_WARNINGS)
    if(NOT _warning MATCHES "^[A-Za-z0-9][A-Za-z0-9_-]*$")
      message(FATAL_ERROR
        "Invalid NATIVEUI_ALLOWED_WARNINGS entry '${_warning}'; use the diagnostic name without the -W prefix")
    endif()
    add_compile_options("-Wno-error=${_warning}")
    if(_warning STREQUAL "deprecated-declarations")
      add_compile_definitions(NATIVEUI_ALLOW_DEPRECATED_DECLARATIONS=1)
    endif()
  endforeach()
else()
  message(FATAL_ERROR
    "NativeUI warning policy is not defined for compiler '${CMAKE_CXX_COMPILER_ID}'")
endif()
