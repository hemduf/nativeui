include_guard(GLOBAL)

# Windowing dependencies are optional so the core test suite can be configured
# and run without Pugl, OpenGL, X11/AppKit/Win32, or a display.
if(NATIVEUI_BUILD_PLATFORM)
  # -----------------------------------------------------------------------------
  # Pugl: source dependency managed by CPM, compiled statically by NativeUI.
  # -----------------------------------------------------------------------------
  # 9498280 retains the Emscripten input/context-menu and multi-view focus
  # fixes while adding the reviewed iOS/iPadOS raw multi-pointer API.
  set(NATIVEUI_PUGL_COMMIT
      "94982803985eefcbaeb0a1c8d0136ec862d7cb59"
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

  add_library(nativeui_opengl INTERFACE)
  add_library(NativeUI::OpenGL ALIAS nativeui_opengl)
  if(EMSCRIPTEN)
    # The browser provides OpenGL ES through WebGL2; no link library exists.
    # Pugl's Emscripten GL backend maps the requested 3.0 context to WebGL2.
    target_link_options(nativeui_opengl INTERFACE "-sMAX_WEBGL_VERSION=2")
  else()
    find_package(OpenGL REQUIRED)
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
  endif()

  if(APPLE)
    # T053 deliberately does not create one generic Objective-C Pugl archive.
    # Only the generic common/internal C sources are shared; mac.m, mac_gl.m
    # and NativeUI's Cocoa IME bridge are compiled by the final-consumer target
    # factory with the derived consumer-specific runtime prefix. T064 adds a
    # small Objective-C++ desktop-services backend to the same final bridge.
    enable_language(OBJC)
    enable_language(OBJCXX)
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
    elseif(EMSCRIPTEN)
      # Pugl's browser backend owns the DOM canvas, WebGL context, DOM input
      # listeners and browser timers; NativeUI's IME bridge is a no-op stub
      # because committed text arrives as PUGL_TEXT events.
      list(APPEND _pugl_sources
        "${pugl_src_SOURCE_DIR}/src/emscripten.c"
        "${pugl_src_SOURCE_DIR}/src/emscripten_events.c"
        "${pugl_src_SOURCE_DIR}/src/emscripten_gl.c"
        "${CMAKE_CURRENT_LIST_DIR}/../src/detail/native_ime_emscripten.c"
      )
    elseif(UNIX)
      list(APPEND _pugl_sources
        "${pugl_src_SOURCE_DIR}/src/x11.c"
        "${pugl_src_SOURCE_DIR}/src/x11_gl.c"
        "${CMAKE_CURRENT_LIST_DIR}/../src/detail/native_ime_x11.c"
      )
    else()
      message(FATAL_ERROR "NativeUI/Pugl supports macOS, Windows, Linux/X11 and WebAssembly")
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
    elseif(EMSCRIPTEN)
      # Emscripten's GL/HTML5 runtime is provided by the toolchain itself.
      target_compile_definitions(nativeui_pugl PRIVATE _POSIX_C_SOURCE=200809L)
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
# Skia: prebuilt static release artifacts from hemduf/skia-builder.
# The binary archive itself is managed/downloaded by CPM.
# -----------------------------------------------------------------------------
# chrome/m153 is the rebuilt M153 package from skia-builder commit f21749b18c14976415ec30d068c3a13e8456c3a1.
# The release archives remain integrity-pinned below by exact SHA256.
set(NATIVEUI_SKIA_TAG "chrome/m153" CACHE STRING "skia-builder release tag")
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

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(_nativeui_linux_skia_arch "x64")
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(_nativeui_linux_skia_arch "arm64")
  else()
    message(FATAL_ERROR "The pinned skia-builder release supports Linux x64 and arm64 only")
  endif()
endif()

if(NOT NATIVEUI_SKIA_ROOT)
  string(TOLOWER "${NATIVEUI_SKIA_CONFIG}" _skia_cfg)

  if(EMSCRIPTEN)
    if(NOT NATIVEUI_SKIA_CONFIG STREQUAL "Release")
      message(FATAL_ERROR "skia-builder ${NATIVEUI_SKIA_TAG} publishes the wasm32 artifact as Release")
    endif()
    set(_skia_asset "skia-build-wasm-wasm32-gpu-release.zip")
    set(_skia_hash "SHA256=e40f67d7fe7ecaf9ee860b5d2782f573a7209e15c923ba8cc9598ffe8b630b9c")
  elseif(APPLE)
    if(NOT NATIVEUI_SKIA_CONFIG STREQUAL "Release")
      message(FATAL_ERROR "skia-builder ${NATIVEUI_SKIA_TAG} publishes the macOS universal artifact as Release")
    endif()
    set(_skia_asset "skia-build-mac-universal-gpu-release.zip")
    set(_skia_hash "SHA256=bbf23943d044a70b07bab3f935dc8bf725fac8f77593f2bd3f9c77cdb16bb640")
  elseif(WIN32)
    string(TOUPPER "${NATIVEUI_SKIA_WINDOWS_CRT}" _crt)
    if(_crt STREQUAL "MD")
      set(_suffix "gpu-md")
      if(NATIVEUI_SKIA_CONFIG STREQUAL "Debug")
        set(_skia_hash "SHA256=6c8332b6a4aafd61207d7f535f084011bc2d977a0193291b3b98de41f81e9a02")
      else()
        set(_skia_hash "SHA256=3e19bd7fb8fc81f8ac554c8a4add24ac21815d618227184d5c69aea885a20ce7")
      endif()
    elseif(_crt STREQUAL "MT")
      set(_suffix "gpu")
      if(NATIVEUI_SKIA_CONFIG STREQUAL "Debug")
        set(_skia_hash "SHA256=706a35c93eea00dda635e4c3a2eb24323371a67a21171188b32374c7d531817c")
      else()
        set(_skia_hash "SHA256=c18b120ff4f77a6440d187176877e56e80da36e7790eb85c134fc743ef15e6f9")
      endif()
    else()
      message(FATAL_ERROR "NATIVEUI_SKIA_WINDOWS_CRT must be MD or MT")
    endif()
    set(_skia_asset "skia-build-win-x64-${_suffix}-${_skia_cfg}.zip")
  elseif(UNIX)
    if(_nativeui_linux_skia_arch STREQUAL "x64")
      set(_skia_hash "SHA256=8da94ec6532d2586fdd719859b8437da20b1b5fad30951703dd19fa4a679eca9")
    else()
      set(_skia_hash "SHA256=b45e8e40f3d8e29176ea5fc6e3ecafdbc87ac0a58469f649058fbc8ac0831687")
    endif()
    if(NOT NATIVEUI_SKIA_CONFIG STREQUAL "Release")
      message(FATAL_ERROR "skia-builder ${NATIVEUI_SKIA_TAG} publishes Linux artifacts as Release")
    endif()
    set(_skia_asset "skia-build-linux-${_nativeui_linux_skia_arch}-gpu-release.zip")
  endif()

  set(_skia_url
    "https://github.com/hemduf/skia-builder/releases/download/${NATIVEUI_SKIA_TAG}/${_skia_asset}")

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

if(EMSCRIPTEN)
  set(_skia_lib_dir "${_skia_package_root}/wasm-gpu/lib/Release")
  set(_skia_lib_candidates
    "${_skia_lib_dir}/libSkia.a"
    "${_skia_lib_dir}/libskia.a"
  )
elseif(APPLE)
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
  set(_skia_lib_dir "${_skia_package_root}/linux-gpu/lib/Release/${_nativeui_linux_skia_arch}")
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

if(EMSCRIPTEN)
  # The wasm32 package bundles FreeType, HarfBuzz and ICU and consumes the
  # browser's WebGL/WebGPU through Skia, so no system library is added here.
  # skia-builder builds it with is_trivial_abi=true, so every consumer
  # translation unit must agree on the sk_sp/sk_refptr ABI attribute or
  # wasm-ld reports function signature mismatches.
  target_compile_definitions(SkiaBuilder::skia INTERFACE
    "SK_TRIVIAL_ABI=[[clang::trivial_abi]]")
elseif(APPLE)
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
