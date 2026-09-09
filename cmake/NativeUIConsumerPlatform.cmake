include_guard(GLOBAL)
include(CMakeParseArguments)

# Derive the only supported NativeUI consumer-specific Objective-C runtime
# prefix from the exact original UTF-8 identity bytes. Keep this algorithm in
# one CMake function so source-tree and installed-package helpers cannot drift.
function(nativeui_compute_objc_runtime_prefix out_var consumer_id)
  if("${out_var}" STREQUAL "")
    message(FATAL_ERROR "nativeui_compute_objc_runtime_prefix requires an output variable")
  endif()
  if("${consumer_id}" STREQUAL "")
    message(FATAL_ERROR "NativeUI CONSUMER_ID must be non-empty")
  endif()

  string(SHA256 _nativeui_consumer_digest "${consumer_id}")
  string(SUBSTRING "${_nativeui_consumer_digest}" 0 12 _nativeui_digest12)

  # Only ASCII alphanumerics survive in the human-readable fragment. Every
  # maximal run of punctuation, whitespace or non-ASCII UTF-8 bytes collapses
  # to one underscore; the SHA-256 above still covers the original bytes.
  string(REGEX REPLACE "[^A-Za-z0-9]+" "_" _nativeui_fragment "${consumer_id}")
  string(REGEX REPLACE "^_+" "" _nativeui_fragment "${_nativeui_fragment}")
  string(REGEX REPLACE "_+$" "" _nativeui_fragment "${_nativeui_fragment}")
  if(_nativeui_fragment STREQUAL "")
    set(_nativeui_fragment "consumer")
  endif()

  string(LENGTH "${_nativeui_fragment}" _nativeui_fragment_length)
  if(_nativeui_fragment_length GREATER 24)
    string(SUBSTRING "${_nativeui_fragment}" 0 24 _nativeui_fragment)
    string(REGEX REPLACE "_+$" "" _nativeui_fragment "${_nativeui_fragment}")
    if(_nativeui_fragment STREQUAL "")
      set(_nativeui_fragment "consumer")
    endif()
  endif()

  set(${out_var}
      "NUI_${_nativeui_fragment}_${_nativeui_digest12}_"
      PARENT_SCOPE)
endfunction()

# Configure-time bookkeeping only. These GLOBAL CMake properties disappear
# after generation; no process/runtime registry is emitted into NativeUI.
function(_nativeui_register_consumer_identity target consumer_id)
  if("${target}" STREQUAL "")
    message(FATAL_ERROR "NativeUI consumer registration requires a target name")
  endif()
  if("${consumer_id}" STREQUAL "")
    message(FATAL_ERROR "NativeUI CONSUMER_ID must be non-empty")
  endif()

  string(SHA256 _target_key "${target}")
  string(SHA256 _identity_key "${consumer_id}")
  set(_target_property "NATIVEUI_T053_TARGET_${_target_key}")
  set(_identity_property "NATIVEUI_T053_ID_${_identity_key}")

  get_property(_target_is_set GLOBAL PROPERTY "${_target_property}" SET)
  if(_target_is_set)
    get_property(_existing_identity GLOBAL PROPERTY "${_target_property}")
    message(FATAL_ERROR
      "NativeUI target '${target}' already has consumer identity '${_existing_identity}'; duplicate attachment is not allowed")
  endif()

  get_property(_identity_is_set GLOBAL PROPERTY "${_identity_property}" SET)
  if(_identity_is_set)
    get_property(_existing_target GLOBAL PROPERTY "${_identity_property}")
    message(FATAL_ERROR
      "NativeUI CONSUMER_ID '${consumer_id}' is already registered to target '${_existing_target}'; target '${target}' cannot reuse it")
  endif()

  set_property(GLOBAL PROPERTY "${_target_property}" "${consumer_id}")
  set_property(GLOBAL PROPERTY "${_identity_property}" "${target}")
endfunction()

function(_nativeui_platform_source_roots out_pugl out_nativeui)
  if(DEFINED NATIVEUI_PUGL_SOURCE_DIR AND
     EXISTS "${NATIVEUI_PUGL_SOURCE_DIR}/include/pugl/pugl.h")
    set(_pugl_root "${NATIVEUI_PUGL_SOURCE_DIR}")
  elseif(DEFINED pugl_src_SOURCE_DIR AND
         EXISTS "${pugl_src_SOURCE_DIR}/include/pugl/pugl.h")
    set(_pugl_root "${pugl_src_SOURCE_DIR}")
  else()
    message(FATAL_ERROR
      "NativeUI consumer platform bridge cannot locate the pinned Pugl source tree. "
      "The installed package may be incomplete; expected include/pugl/pugl.h under NATIVEUI_PUGL_SOURCE_DIR.")
  endif()

  if(DEFINED NATIVEUI_PLATFORM_SOURCE_ROOT AND
     EXISTS "${NATIVEUI_PLATFORM_SOURCE_ROOT}/src/detail/native_ime_macos.m")
    set(_nativeui_root "${NATIVEUI_PLATFORM_SOURCE_ROOT}")
  else()
    get_filename_component(
      _nativeui_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  endif()
  if(NOT EXISTS "${_nativeui_root}/src/detail/native_ime_macos.m" OR
     NOT EXISTS "${_nativeui_root}/src/pugl_skia.cpp")
    message(FATAL_ERROR
      "NativeUI consumer platform bridge cannot locate NativeUI platform sources at ${_nativeui_root}")
  endif()

  set(${out_pugl} "${_pugl_root}" PARENT_SCOPE)
  set(${out_nativeui} "${_nativeui_root}" PARENT_SCOPE)
endfunction()

function(_nativeui_platform_opengl_target out_var)
  if(TARGET NativeUI::OpenGL)
    set(${out_var} NativeUI::OpenGL PARENT_SCOPE)
    return()
  endif()
  if(TARGET _nativeui_package_opengl)
    set(${out_var} _nativeui_package_opengl PARENT_SCOPE)
    return()
  endif()

  find_package(OpenGL REQUIRED)
  add_library(_nativeui_package_opengl INTERFACE)
  if(TARGET OpenGL::GL)
    target_link_libraries(_nativeui_package_opengl INTERFACE OpenGL::GL)
  elseif(TARGET OpenGL::OpenGL)
    target_link_libraries(_nativeui_package_opengl INTERFACE OpenGL::OpenGL)
    if(TARGET OpenGL::GLX)
      target_link_libraries(_nativeui_package_opengl INTERFACE OpenGL::GLX)
    endif()
  else()
    message(FATAL_ERROR
      "NativeUI platform attachment requires a usable OpenGL target from CMake FindOpenGL")
  endif()
  set(${out_var} _nativeui_package_opengl PARENT_SCOPE)
endfunction()

# The portable Pugl C core is generic and compiled once. Only Cocoa/OpenGL and
# NativeUI's Objective-C IME bridge are consumer-specific on macOS.
function(_nativeui_prepare_macos_platform_common)
  if(NOT APPLE OR TARGET nativeui_pugl_common)
    return()
  endif()

  if(NOT CMAKE_C_COMPILER_LOADED)
    enable_language(C)
  endif()
  if(NOT CMAKE_OBJC_COMPILER_LOADED)
    enable_language(OBJC)
  endif()

  _nativeui_platform_opengl_target(_nativeui_opengl_target)
  _nativeui_platform_source_roots(_pugl_root _nativeui_root)
  add_library(nativeui_pugl_common STATIC
    "${_pugl_root}/src/common.c"
    "${_pugl_root}/src/internal.c"
  )
  set_target_properties(nativeui_pugl_common PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
  )
  target_compile_features(nativeui_pugl_common PUBLIC c_std_99)
  target_include_directories(nativeui_pugl_common
    PUBLIC "${_pugl_root}/include"
    PRIVATE "${_pugl_root}/src"
  )
  target_compile_definitions(nativeui_pugl_common
    PUBLIC PUGL_STATIC
    PRIVATE PUGL_INTERNAL GL_SILENCE_DEPRECATION
  )
  target_compile_options(nativeui_pugl_common PRIVATE -Wno-deprecated-declarations)
  target_link_libraries(nativeui_pugl_common PUBLIC "${_nativeui_opengl_target}")

  if(DEFINED APPKIT_FRAMEWORK)
    set(_nativeui_appkit "${APPKIT_FRAMEWORK}")
  else()
    find_library(_nativeui_appkit AppKit REQUIRED)
  endif()
  if(DEFINED FOUNDATION_FRAMEWORK)
    set(_nativeui_foundation "${FOUNDATION_FRAMEWORK}")
  else()
    find_library(_nativeui_foundation Foundation REQUIRED)
  endif()
  if(DEFINED COREVIDEO_FRAMEWORK)
    set(_nativeui_corevideo "${COREVIDEO_FRAMEWORK}")
  else()
    find_library(_nativeui_corevideo CoreVideo REQUIRED)
  endif()
  target_link_libraries(nativeui_pugl_common PUBLIC
    "${_nativeui_appkit}"
    "${_nativeui_foundation}"
    "${_nativeui_corevideo}"
  )

  # Dependencies.cmake still defines its historical all-in-one Pugl target for
  # non-macOS platforms. It is intentionally unreachable/excluded on macOS once
  # T053 rewires NativeUI::NativeUI to this generic common target plus a final-
  # consumer bridge.
  if(TARGET nativeui_pugl)
    set_target_properties(nativeui_pugl PROPERTIES EXCLUDE_FROM_ALL TRUE)
  endif()
endfunction()

# Installed packages do not export a generic NativeUI::NativeUI platform target:
# T047's public surface is NativeUI::Core + nativeui_attach_platform(). Build the
# private generic C++/Pugl layer lazily in the consuming build when that helper is
# actually called. macOS still keeps Objective-C bridge sources per final target.
function(_nativeui_prepare_package_platform out_var)
  if(TARGET _nativeui_package_platform)
    set(${out_var} _nativeui_package_platform PARENT_SCOPE)
    return()
  endif()
  if(NOT TARGET NativeUI::Core)
    message(FATAL_ERROR
      "NativeUI package platform attachment requires imported target NativeUI::Core")
  endif()

  if(NOT CMAKE_C_COMPILER_LOADED)
    enable_language(C)
  endif()
  if(NOT CMAKE_CXX_COMPILER_LOADED)
    enable_language(CXX)
  endif()

  _nativeui_platform_source_roots(_pugl_root _nativeui_root)
  _nativeui_platform_opengl_target(_nativeui_opengl_target)

  if(APPLE)
    _nativeui_prepare_macos_platform_common()
    set(_nativeui_pugl_target nativeui_pugl_common)
  else()
    if(NOT TARGET _nativeui_package_pugl)
      set(_nativeui_pugl_sources
        "${_pugl_root}/src/common.c"
        "${_pugl_root}/src/internal.c"
      )
      if(WIN32)
        list(APPEND _nativeui_pugl_sources
          "${_pugl_root}/src/win.c"
          "${_pugl_root}/src/win_gl.c"
          "${_nativeui_root}/src/detail/native_ime_windows.c"
        )
      elseif(UNIX)
        list(APPEND _nativeui_pugl_sources
          "${_pugl_root}/src/x11.c"
          "${_pugl_root}/src/x11_gl.c"
          "${_nativeui_root}/src/detail/native_ime_x11.c"
        )
      else()
        message(FATAL_ERROR
          "NativeUI package platform attachment supports macOS, Windows and Linux/X11")
      endif()

      add_library(_nativeui_package_pugl STATIC ${_nativeui_pugl_sources})
      set_target_properties(_nativeui_package_pugl PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
      )
      target_compile_features(_nativeui_package_pugl PUBLIC c_std_99)
      target_include_directories(_nativeui_package_pugl
        PUBLIC "${_pugl_root}/include"
        PRIVATE "${_pugl_root}/src"
      )
      target_compile_definitions(_nativeui_package_pugl
        PUBLIC PUGL_STATIC
        PRIVATE PUGL_INTERNAL
      )
      target_link_libraries(_nativeui_package_pugl PUBLIC "${_nativeui_opengl_target}")

      if(WIN32)
        target_compile_definitions(_nativeui_package_pugl PRIVATE
          UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
          WINVER=0x0601 _WIN32_WINNT=0x0601
        )
        target_link_libraries(_nativeui_package_pugl PUBLIC
          dwmapi gdi32 imm32 shell32 shlwapi user32
        )
      else()
        find_package(X11 REQUIRED)
        target_compile_definitions(_nativeui_package_pugl PRIVATE
          _POSIX_C_SOURCE=200809L
          USE_XCURSOR=0
          USE_XRANDR=0
          USE_XSYNC=0
        )
        target_link_libraries(_nativeui_package_pugl PUBLIC
          X11::X11 ${CMAKE_DL_LIBS}
        )
      endif()
    endif()
    set(_nativeui_pugl_target _nativeui_package_pugl)
  endif()

  add_library(_nativeui_package_platform STATIC
    "${_nativeui_root}/src/pugl_skia.cpp"
  )
  set_target_properties(_nativeui_package_platform PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
  target_compile_features(_nativeui_package_platform PUBLIC cxx_std_20)
  target_compile_definitions(_nativeui_package_platform PRIVATE SK_GL)
  if(WIN32)
    target_compile_definitions(_nativeui_package_platform PRIVATE NOMINMAX)
  elseif(APPLE)
    target_compile_definitions(_nativeui_package_platform PRIVATE GL_SILENCE_DEPRECATION)
  endif()
  target_link_libraries(_nativeui_package_platform
    PUBLIC NativeUI::Core
    PRIVATE "${_nativeui_pugl_target}" "${_nativeui_opengl_target}"
  )

  set(${out_var} _nativeui_package_platform PARENT_SCOPE)
endfunction()

# Internal T053 source-tree/package primitive. T047 supplies the public
# nativeui_attach_platform() validation wrapper; all high-level helpers must
# delegate here instead of reproducing the prefix or bridge logic.
function(_nativeui_attach_consumer_platform)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "" "TARGET;CONSUMER_ID;OUT_BRIDGE" "")
  if(NUI_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "NativeUI consumer platform attachment received unknown arguments: ${NUI_UNPARSED_ARGUMENTS}")
  endif()
  if(NOT NUI_TARGET)
    message(FATAL_ERROR "NativeUI consumer platform attachment requires TARGET")
  endif()
  if(NOT TARGET "${NUI_TARGET}")
    message(FATAL_ERROR "NativeUI consumer platform target does not exist: '${NUI_TARGET}'")
  endif()
  if(NOT NUI_CONSUMER_ID)
    message(FATAL_ERROR "NativeUI consumer platform attachment requires CONSUMER_ID")
  endif()

  if(TARGET NativeUI::NativeUI)
    set(_nativeui_platform_target NativeUI::NativeUI)
  else()
    _nativeui_prepare_package_platform(_nativeui_platform_target)
  endif()

  _nativeui_register_consumer_identity("${NUI_TARGET}" "${NUI_CONSUMER_ID}")
  nativeui_compute_objc_runtime_prefix(_nativeui_objc_prefix "${NUI_CONSUMER_ID}")

  set(_nativeui_bridge "")
  if(APPLE)
    _nativeui_prepare_macos_platform_common()
    _nativeui_platform_source_roots(_pugl_root _nativeui_root)

    string(SHA256 _nativeui_bridge_digest
      "${NUI_TARGET}\n${NUI_CONSUMER_ID}")
    string(SUBSTRING "${_nativeui_bridge_digest}" 0 16 _nativeui_bridge_key)
    set(_nativeui_bridge "nativeui_macos_bridge_${_nativeui_bridge_key}")

    add_library("${_nativeui_bridge}" STATIC
      "${_pugl_root}/src/mac.m"
      "${_pugl_root}/src/mac_gl.m"
      "${_nativeui_root}/src/detail/native_ime_macos.m"
    )
    set_target_properties("${_nativeui_bridge}" PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      C_VISIBILITY_PRESET hidden
      CXX_VISIBILITY_PRESET hidden
      OBJC_VISIBILITY_PRESET hidden
      VISIBILITY_INLINES_HIDDEN YES
    )
    target_compile_features("${_nativeui_bridge}" PUBLIC c_std_99)
    target_include_directories("${_nativeui_bridge}"
      PUBLIC "${_pugl_root}/include"
      PRIVATE "${_pugl_root}/src"
    )
    target_compile_definitions("${_nativeui_bridge}"
      PUBLIC PUGL_STATIC
      PRIVATE
        PUGL_INTERNAL
        GL_SILENCE_DEPRECATION
        "PuglWindow=${_nativeui_objc_prefix}PuglWindow"
        "PuglWindowDelegate=${_nativeui_objc_prefix}PuglWindowDelegate"
        "PuglWrapperView=${_nativeui_objc_prefix}PuglWrapperView"
        "PuglOpenGLView=${_nativeui_objc_prefix}PuglOpenGLView"
    )
    target_compile_options("${_nativeui_bridge}" PRIVATE -Wno-deprecated-declarations)
    target_link_libraries("${_nativeui_bridge}" PUBLIC nativeui_pugl_common)

    if(COMMAND nativeui_enable_project_warnings)
      nativeui_enable_project_warnings("${_nativeui_bridge}")
    endif()

    # Xcode's Foundation MIN/MAX macros use GNU statement expressions. Pugl's
    # mac.m calls those system macros with side-effect-free arguments, so keep
    # the project-wide pedantic warning policy and disable only Clang's narrow
    # macro-expansion diagnostic for this Objective-C bridge.
    if(CMAKE_OBJC_COMPILER_ID MATCHES "Clang")
      target_compile_options("${_nativeui_bridge}" PRIVATE
        -Wno-gnu-statement-expression-from-macro-expansion
      )
    endif()
  endif()

  target_link_libraries("${NUI_TARGET}" PRIVATE "${_nativeui_platform_target}")
  if(_nativeui_bridge)
    target_link_libraries("${NUI_TARGET}" PRIVATE "${_nativeui_bridge}")
  endif()

  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_CONSUMER_ID "${NUI_CONSUMER_ID}")
  set_property(TARGET "${NUI_TARGET}" PROPERTY
    NATIVEUI_OBJC_RUNTIME_PREFIX "${_nativeui_objc_prefix}")
  if(_nativeui_bridge)
    set_property(TARGET "${NUI_TARGET}" PROPERTY
      NATIVEUI_OBJC_BRIDGE_TARGET "${_nativeui_bridge}")
  endif()

  if(NUI_OUT_BRIDGE)
    set(${NUI_OUT_BRIDGE} "${_nativeui_bridge}" PARENT_SCOPE)
  endif()
endfunction()
