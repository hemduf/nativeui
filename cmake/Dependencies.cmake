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

  set(PUGL_INCLUDE_DIR "${pugl_src_SOURCE_DIR}/include")
  set(PUGL_SRC_DIR "${pugl_src_SOURCE_DIR}/src")
  set(PUGL_SRC_COMMON
    "${PUGL_SRC_DIR}/common.c"
    "${PUGL_SRC_DIR}/internal.c"
  )
  set(PUGL_PLATFORM_SOURCES)
  set(PUGL_PLATFORM_LIBS)

  if(APPLE)
    list(APPEND PUGL_PLATFORM_SOURCES
      "${PUGL_SRC_DIR}/mac.m"
      "${PUGL_SRC_DIR}/cairo_gl.m"
      "${PUGL_SRC_DIR}/stub.c"
    )
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    list(APPEND PUGL_PLATFORM_LIBS
      ${COCOA_FRAMEWORK}
      ${OPENGL_FRAMEWORK}
    )
  elseif(WIN32)
    list(APPEND PUGL_PLATFORM_SOURCES
      "${PUGL_SRC_DIR}/win.c"
      "${PUGL_SRC_DIR}/win_gl.c"
      "${PUGL_SRC_DIR}/stub.c"
    )
    list(APPEND PUGL_PLATFORM_LIBS opengl32)
  elseif(UNIX)
    list(APPEND PUGL_PLATFORM_SOURCES
      "${PUGL_SRC_DIR}/x11.c"
      "${PUGL_SRC_DIR}/x11_gl.c"
      "${PUGL_SRC_DIR}/stub.c"
    )
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    list(APPEND PUGL_PLATFORM_LIBS
      X11::X11
      OpenGL::GL
    )
  endif()

  # -----------------------------------------------------------------------------
  # Skia: pinned prebuilt static binaries from olilarkin/skia-builder.
  # -----------------------------------------------------------------------------
  set(NATIVEUI_SKIA_BRANCH "chrome/m149" CACHE STRING "Pinned skia-builder branch")
  set(NATIVEUI_SKIA_SOURCE "" CACHE PATH "Use an already available skia-builder checkout")
  set(NATIVEUI_SKIA_BASE_URL
      "https://github.com/olilarkin/skia-builder/releases/download/${NATIVEUI_SKIA_BRANCH}"
      CACHE STRING "Base URL for pinned Skia archives")

  set(NATIVEUI_SKIA_WINDOWS_CRT "MD" CACHE STRING "Windows Skia CRT variant: MD or MT")
  set_property(CACHE NATIVEUI_SKIA_WINDOWS_CRT PROPERTY STRINGS MD MT)

  if(NATIVEUI_SKIA_SOURCE)
    set(skia_builder_SOURCE_DIR "${NATIVEUI_SKIA_SOURCE}")
  else()
    CPMAddPackage(
      NAME skia_builder
      GITHUB_REPOSITORY olilarkin/skia-builder
      GIT_TAG ${NATIVEUI_SKIA_BRANCH}
      DOWNLOAD_ONLY YES
    )
  endif()

  if(APPLE)
    set(NATIVEUI_SKIA_ARCHIVE "Skia-macOS-Release-universal.zip")
    set(NATIVEUI_SKIA_ARCHIVE_SHA256
        "72c3ba9a624383218bb762b0b9b76e97bc6fe0119a6e6970c6068a6b74c307cf")
    set(NATIVEUI_SKIA_CACHE_DIR
        "${CMAKE_BINARY_DIR}/_deps/nativeui-skia-macos-release-universal")
  elseif(WIN32)
    string(TOUPPER "${NATIVEUI_SKIA_WINDOWS_CRT}" _nativeui_skia_windows_crt)
    if(_nativeui_skia_windows_crt STREQUAL "MT")
      set(NATIVEUI_SKIA_ARCHIVE "Skia-Windows-Release-MT-x64.zip")
      set(NATIVEUI_SKIA_ARCHIVE_SHA256
          "ddc76e604c088af24d564e7091094251744c073000a849de1b0cb2817720cf2a")
    elseif(_nativeui_skia_windows_crt STREQUAL "MD")
      set(NATIVEUI_SKIA_ARCHIVE "Skia-Windows-Release-MD-x64.zip")
      set(NATIVEUI_SKIA_ARCHIVE_SHA256
          "1d9b20f3048ef7f36397bc5096f4cd3f98662f2609b2ec680dd482433ad1fc7c")
    else()
      message(FATAL_ERROR
          "NATIVEUI_SKIA_WINDOWS_CRT must be MD or MT, got '${NATIVEUI_SKIA_WINDOWS_CRT}'")
    endif()
    set(NATIVEUI_SKIA_CACHE_DIR
        "${CMAKE_BINARY_DIR}/_deps/nativeui-skia-windows-${_nativeui_skia_windows_crt}-x64")
  elseif(UNIX)
    set(NATIVEUI_SKIA_ARCHIVE "Skia-Linux-Release-x64.zip")
    set(NATIVEUI_SKIA_ARCHIVE_SHA256
        "f2876dd8b2ae72dadcd99f6feec2c18b7c15308893e7a58a9b9a3420ba1e6de4")
    set(NATIVEUI_SKIA_CACHE_DIR
        "${CMAKE_BINARY_DIR}/_deps/nativeui-skia-linux-release-x64")
  else()
    message(FATAL_ERROR "NativeUI Skia binaries are not configured for this platform")
  endif()

  set(_nativeui_skia_archive_path
      "${CMAKE_BINARY_DIR}/_deps/${NATIVEUI_SKIA_ARCHIVE}")
  if(NOT EXISTS "${NATIVEUI_SKIA_CACHE_DIR}/include/core/SkCanvas.h")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
    file(DOWNLOAD
      "${NATIVEUI_SKIA_BASE_URL}/${NATIVEUI_SKIA_ARCHIVE}"
      "${_nativeui_skia_archive_path}"
      EXPECTED_HASH "SHA256=${NATIVEUI_SKIA_ARCHIVE_SHA256}"
      SHOW_PROGRESS
      STATUS _nativeui_skia_download_status
    )
    list(GET _nativeui_skia_download_status 0 _nativeui_skia_download_code)
    if(NOT _nativeui_skia_download_code EQUAL 0)
      list(GET _nativeui_skia_download_status 1 _nativeui_skia_download_message)
      message(FATAL_ERROR
          "Failed to download pinned Skia archive: ${_nativeui_skia_download_message}")
    endif()
    file(REMOVE_RECURSE "${NATIVEUI_SKIA_CACHE_DIR}")
    file(MAKE_DIRECTORY "${NATIVEUI_SKIA_CACHE_DIR}")
    file(ARCHIVE_EXTRACT
      INPUT "${_nativeui_skia_archive_path}"
      DESTINATION "${NATIVEUI_SKIA_CACHE_DIR}"
    )
  endif()

  set(NATIVEUI_SKIA_ROOT "${NATIVEUI_SKIA_CACHE_DIR}")
endif()
