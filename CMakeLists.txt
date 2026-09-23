cmake_minimum_required(VERSION 3.24)
project(NativeUI VERSION 0.1.0 LANGUAGES C CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NATIVEUI_BUILD_PLATFORM "Build Pugl/OpenGL windowing integration" ON)
option(NATIVEUI_BUILD_EXAMPLES "Build the NativeUI demo and examples" ON)
option(NATIVEUI_BUILD_FEATURE_EXAMPLES "Build examples/features executables when examples are enabled" ON)
option(NATIVEUI_BUILD_TESTS "Build and register NativeUI tests" ON)
option(NATIVEUI_BUILD_PACKAGE_TESTS "Register installed-package consumer tests" OFF)
option(NATIVEUI_ENABLE_SANITIZERS "Enable ASan+UBSan on NativeUI-owned code/tests" OFF)
option(NATIVEUI_ENABLE_PLATFORM_SMOKE_TESTS "Register native standalone/embedded smoke tests with CTest" OFF)
option(NATIVEUI_ENABLE_INSPECTOR "Enable the opt-in NativeUI debug inspector" OFF)

if(NATIVEUI_BUILD_EXAMPLES AND NOT NATIVEUI_BUILD_PLATFORM)
  message(FATAL_ERROR "NATIVEUI_BUILD_EXAMPLES requires NATIVEUI_BUILD_PLATFORM=ON")
endif()
if(NATIVEUI_BUILD_PACKAGE_TESTS AND NOT NATIVEUI_BUILD_PLATFORM)
  message(FATAL_ERROR "NATIVEUI_BUILD_PACKAGE_TESTS requires NATIVEUI_BUILD_PLATFORM=ON")
endif()

include(cmake/CPM.cmake)
include(cmake/Dependencies.cmake)
include(cmake/NativeUIConsumerPlatform.cmake)
include(cmake/NativeUIAttachPlatform.cmake)
include(cmake/NativeUIFeatureExamples.cmake)

# T023 consumes the SVG module shipped beside the pinned skia-builder archive.
# Skia's SVG DOM references text shaping even for SVGs without text nodes, and
# skshaper in turn references the packaged SkUnicode ICU implementation. Keep
# the Unix static-link dependency order from users toward providers.
# Emscripten's platform module sets CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY, so
# explicit package paths must opt out of the cross-compilation root re-writing.
find_library(NATIVEUI_SKIA_SVG_LIBRARY
  NAMES svg
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_SHAPER_LIBRARY
  NAMES skshaper
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_UNICODE_ICU_LIBRARY
  NAMES skunicode_icu
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  REQUIRED
)
find_library(NATIVEUI_SKIA_UNICODE_CORE_LIBRARY
  NAMES skunicode_core
  PATHS "${_skia_lib_dir}"
  NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH
  REQUIRED
)
add_library(SkiaBuilder::svg STATIC IMPORTED GLOBAL)
set_target_properties(SkiaBuilder::svg PROPERTIES
  IMPORTED_LOCATION "${NATIVEUI_SKIA_SVG_LIBRARY}"
)
set_property(TARGET SkiaBuilder::svg APPEND PROPERTY INTERFACE_LINK_LIBRARIES
  "${NATIVEUI_SKIA_SHAPER_LIBRARY};${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY};${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY};SkiaBuilder::skia")

# Stable file names are baked into the generated package configs while every
# path remains relative to the package prefix in the installed configuration.
get_filename_component(NATIVEUI_SKIA_MAIN_FILENAME "${_skia_lib}" NAME)
get_filename_component(NATIVEUI_SKIA_SVG_FILENAME "${NATIVEUI_SKIA_SVG_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_SHAPER_FILENAME "${NATIVEUI_SKIA_SHAPER_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_UNICODE_ICU_FILENAME "${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY}" NAME)
get_filename_component(NATIVEUI_SKIA_UNICODE_CORE_FILENAME "${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY}" NAME)

set(NATIVEUI_INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/NativeUI")
set(NATIVEUI_INSTALL_SKIA_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/skia")
set(NATIVEUI_INSTALL_PUGL_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/pugl")
set(NATIVEUI_INSTALL_SOURCE_DIR "${CMAKE_INSTALL_DATADIR}/nativeui/source")

function(nativeui_enable_project_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

function(nativeui_enable_sanitizers target)
  if(NOT NATIVEUI_ENABLE_SANITIZERS)
    return()
  endif()

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # The pinned skia-builder binaries are consumed as a third-party ABI
    # boundary. UBSan's vptr instrumentation emits RTTI references for Skia
    # polymorphic types which these prebuilt archives do not provide. Disable
    # only that check while retaining ASan and the remaining UBSan coverage.
    target_compile_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-sanitize=vptr
      -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE
      -fsanitize=address,undefined
      -fno-omit-frame-pointer
    )
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /fsanitize=address)
    message(WARNING "MSVC sanitizer mode enables ASan; UBSan is unavailable with MSVC")
  else()
    message(FATAL_ERROR "NATIVEUI_ENABLE_SANITIZERS is unsupported by ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()

# Core runtime + Skia-only services. This target deliberately has no Pugl or
# OpenGL dependency and is compiled once regardless of final consumer identity.
add_library(nativeui_core STATIC
  src/skia_core.cpp
  src/skia_image.cpp
  src/skia_shader.cpp
  src/skia_svg.cpp
  src/headless.cpp
  src/dispatcher.cpp
  src/desktop_services.cpp
)
add_library(NativeUI::Core ALIAS nativeui_core)
set_target_properties(nativeui_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  EXPORT_NAME Core
)
target_include_directories(nativeui_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(nativeui_core PUBLIC cxx_std_20)
if(NATIVEUI_ENABLE_INSPECTOR)
  # UI is header-defined and also consumed from NativeUI-owned .cpp files.
  # Propagate the flag publicly so every translation unit sees one ABI/layout.
  target_compile_definitions(nativeui_core PUBLIC NATIVEUI_ENABLE_INSPECTOR=1)
endif()
target_link_libraries(nativeui_core PUBLIC SkiaBuilder::svg)
nativeui_enable_project_warnings(nativeui_core)
nativeui_enable_sanitizers(nativeui_core)

if(NATIVEUI_BUILD_PLATFORM)
  # On macOS the Pugl C core is generic while Objective-C sources are compiled
  # later by _nativeui_attach_consumer_platform() for each final target. On
  # Windows/Linux the existing generic Pugl target remains unchanged.
  if(APPLE)
    _nativeui_prepare_macos_platform_common()
  endif()

  add_library(nativeui STATIC
    src/pugl_skia.cpp
  )
  add_library(NativeUI::NativeUI ALIAS nativeui)
  set_target_properties(nativeui PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_features(nativeui PUBLIC cxx_std_20)
  target_compile_definitions(nativeui PRIVATE SK_GL)
  if(NATIVEUI_BUILD_TESTS)
    # Source-private renderer readback used only by ImageTexture CPU/GPU parity tests.
    # Installed/package platform sources compile without this seam.
    target_compile_definitions(nativeui PRIVATE
      NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS=1
    )
  endif()
  if(WIN32)
    # windows.h defines function-like min/max macros unless NOMINMAX is set.
    # The C++ platform bridge intentionally uses std::min/std::max throughout;
    # keep the Win32 macros out of this target instead of rewriting portable
    # code around macro expansion. Pugl's C target is intentionally unchanged.
    target_compile_definitions(nativeui PRIVATE NOMINMAX)
  endif()
  if(APPLE)
    target_compile_definitions(nativeui PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(nativeui
      PUBLIC NativeUI::Core
      PRIVATE nativeui_pugl_common NativeUI::OpenGL
    )
  else()
    target_link_libraries(nativeui
      PUBLIC NativeUI::Core
      PRIVATE NativeUI::Pugl NativeUI::OpenGL
    )
  endif()
  nativeui_enable_project_warnings(nativeui)
  nativeui_enable_sanitizers(nativeui)
endif()

# T047 package/export contract. Only the generic Core library is exported.
# Platform implementation sources remain private package machinery compiled by
# nativeui_attach_platform() into each final consumer as required by T053.
install(TARGETS nativeui_core
  EXPORT NativeUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(DIRECTORY include/nativeui
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
install(EXPORT NativeUITargets
  FILE NativeUITargets.cmake
  NAMESPACE NativeUI::
  DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
)

set(_nativeui_install_package_dir "${CMAKE_CURRENT_BINARY_DIR}/nativeui-install-package")
file(MAKE_DIRECTORY "${_nativeui_install_package_dir}")
configure_package_config_file(
  cmake/NativeUIConfig.cmake.in
  "${_nativeui_install_package_dir}/NativeUIConfig.cmake"
  INSTALL_DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
  PATH_VARS
    NATIVEUI_INSTALL_SKIA_DIR
    NATIVEUI_INSTALL_PUGL_DIR
    NATIVEUI_INSTALL_SOURCE_DIR
)
write_basic_package_version_file(
  "${_nativeui_install_package_dir}/NativeUIConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(FILES
  "${_nativeui_install_package_dir}/NativeUIConfig.cmake"
  "${_nativeui_install_package_dir}/NativeUIConfigVersion.cmake"
  cmake/NativeUIConsumerPlatform.cmake
  cmake/NativeUIAttachPlatform.cmake
  cmake/NativeUIApplication.cmake
  cmake/NativeUIWebShell.html
  cmake/NativeUIBinaryData.cmake
  cmake/NativeUIEmbedResource.cmake
  DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}"
)

# Preserve the exact pinned Skia binaries/headers in the package. This avoids a
# consumer silently resolving an ABI-incompatible system Skia.
install(DIRECTORY "${_skia_include}/"
  DESTINATION "${NATIVEUI_INSTALL_SKIA_DIR}/include"
)
install(FILES
  "${_skia_lib}"
  "${NATIVEUI_SKIA_SVG_LIBRARY}"
  "${NATIVEUI_SKIA_SHAPER_LIBRARY}"
  "${NATIVEUI_SKIA_UNICODE_ICU_LIBRARY}"
  "${NATIVEUI_SKIA_UNICODE_CORE_LIBRARY}"
  DESTINATION "${NATIVEUI_INSTALL_SKIA_DIR}/lib"
)

if(NATIVEUI_BUILD_PLATFORM)
  # Pugl is a source dependency by design: the final consumer owns its platform
  # backend, and macOS needs separately compiled Objective-C runtime names.
  install(DIRECTORY "${NATIVEUI_PUGL_SOURCE_DIR}/include/"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}/include"
  )
  install(DIRECTORY "${NATIVEUI_PUGL_SOURCE_DIR}/src/"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}/src"
  )
  install(FILES "${NATIVEUI_PUGL_SOURCE_DIR}/COPYING"
    DESTINATION "${NATIVEUI_INSTALL_PUGL_DIR}"
    OPTIONAL
  )
  install(FILES src/pugl_skia.cpp
    DESTINATION "${NATIVEUI_INSTALL_SOURCE_DIR}/src"
  )
  install(DIRECTORY src/detail/
    DESTINATION "${NATIVEUI_INSTALL_SOURCE_DIR}/src/detail"
  )
endif()

# Build-tree package parity: expose the same NativeUI::Core target and public
# nativeui_attach_platform() helper to an external CMake project without install.
set(_nativeui_build_package_dir "${CMAKE_CURRENT_BINARY_DIR}/nativeui-package")
file(MAKE_DIRECTORY "${_nativeui_build_package_dir}")
export(TARGETS nativeui_core
  FILE "${_nativeui_build_package_dir}/NativeUITargets.cmake"
  NAMESPACE NativeUI::
)
configure_file(
  cmake/NativeUIBuildTreeConfig.cmake.in
  "${_nativeui_build_package_dir}/NativeUIConfig.cmake"
  @ONLY
)
write_basic_package_version_file(
  "${_nativeui_build_package_dir}/NativeUIConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_file(cmake/NativeUIConsumerPlatform.cmake
  "${_nativeui_build_package_dir}/NativeUIConsumerPlatform.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIAttachPlatform.cmake
  "${_nativeui_build_package_dir}/NativeUIAttachPlatform.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIApplication.cmake
  "${_nativeui_build_package_dir}/NativeUIApplication.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIWebShell.html
  "${_nativeui_build_package_dir}/NativeUIWebShell.html"
  COPYONLY
)
configure_file(cmake/NativeUIBinaryData.cmake
  "${_nativeui_build_package_dir}/NativeUIBinaryData.cmake"
  COPYONLY
)
configure_file(cmake/NativeUIEmbedResource.cmake
  "${_nativeui_build_package_dir}/NativeUIEmbedResource.cmake"
  COPYONLY
)

enable_testing()

if(NATIVEUI_BUILD_EXAMPLES OR NATIVEUI_BUILD_TESTS)
  add_subdirectory(examples)
endif()

if(NATIVEUI_BUILD_TESTS OR NATIVEUI_BUILD_PACKAGE_TESTS)
  add_subdirectory(tests)
endif()

# T072 is Linux/X11-only infrastructure. macOS and Windows never discover or
# link libdbus-1; Linux platform builds require the system development package.
if(NATIVEUI_BUILD_PLATFORM AND UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  include(cmake/NativeUILinuxDbus.cmake)
  nativeui_add_linux_dbus_transport()
endif()

# Throwaway WebAssembly spike target: validates that the pinned wasm32 Skia
# package and the headless core compile, link and render under Emscripten.
option(NATIVEUI_BUILD_WASM_SPIKE "Build the WebAssembly headless spike executable" OFF)
if(EMSCRIPTEN AND NATIVEUI_BUILD_WASM_SPIKE)
  add_executable(nativeui_wasm_spike wasm_spike/main.cpp)
  target_link_libraries(nativeui_wasm_spike PRIVATE NativeUI::Core)
  # Browser-consumer packaging spike: the font reaches the wasm heap through
  # Emscripten's preload file system exactly like the demo assets would.
  target_link_options(nativeui_wasm_spike PRIVATE
    "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/tests/resources/NativeUITestLatin.ttf@/nativeui/fonts/NativeUITestLatin.ttf")
  nativeui_enable_project_warnings(nativeui_wasm_spike)

  # Platform-level wasm qualification: one Node test owns/releases the single
  # Emscripten main-loop slot, and one Chromium test exercises simultaneous
  # NativeUI windows plus survivor teardown on real WebGL/DOM views.
  if(NATIVEUI_BUILD_PLATFORM)
    add_executable(nativeui_wasm_main_loop_smoke wasm_spike/main_loop.cpp)
    target_link_libraries(nativeui_wasm_main_loop_smoke PRIVATE NativeUI::NativeUI)
    target_link_options(nativeui_wasm_main_loop_smoke PRIVATE "-sEXIT_RUNTIME=1")
    nativeui_enable_project_warnings(nativeui_wasm_main_loop_smoke)

    add_executable(nativeui_wasm_lifecycle_smoke wasm_spike/lifecycle.cpp)
    target_link_libraries(nativeui_wasm_lifecycle_smoke PRIVATE NativeUI::NativeUI)
    target_link_options(nativeui_wasm_lifecycle_smoke PRIVATE
      "-sEXIT_RUNTIME=0"
      "-sALLOW_MEMORY_GROWTH=1"
      "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/NativeUIWebShell.html")
    set_target_properties(nativeui_wasm_lifecycle_smoke PROPERTIES SUFFIX ".html")
    nativeui_enable_project_warnings(nativeui_wasm_lifecycle_smoke)

    add_executable(nativeui_wasm_embedded_smoke wasm_spike/embedded.cpp)
    target_link_libraries(nativeui_wasm_embedded_smoke PRIVATE NativeUI::NativeUI)
    target_link_options(nativeui_wasm_embedded_smoke PRIVATE
      "-sEXIT_RUNTIME=0"
      "-sALLOW_MEMORY_GROWTH=1"
      "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/NativeUIWebShell.html")
    set_target_properties(nativeui_wasm_embedded_smoke PROPERTIES SUFFIX ".html")
    nativeui_enable_project_warnings(nativeui_wasm_embedded_smoke)

    # First real browser window: the interactive standalone example proves the
    # WebGL surface, DOM input translation and browser main loop end to end.
    add_executable(nativeui_wasm_app examples/standalone.cpp)
    target_link_libraries(nativeui_wasm_app PRIVATE NativeUI::NativeUI)
    target_link_options(nativeui_wasm_app PRIVATE
      "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/cmake/NativeUIWebShell.html")
    # Browser builds have no system fonts: the platform registers every face
    # preloaded into /nativeui/fonts automatically. Keep the repository build
    # deterministic by defaulting to NativeUI's bundled Latin test face; a
    # consumer/demo can override NATIVEUI_WASM_APP_FONT with its production face.
    set(NATIVEUI_WASM_APP_FONT
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/resources/NativeUITestLatin.ttf"
        CACHE FILEPATH "Font preloaded for the WebAssembly example")
    if(EXISTS "${NATIVEUI_WASM_APP_FONT}")
      get_filename_component(_nativeui_wasm_app_font_name "${NATIVEUI_WASM_APP_FONT}" NAME)
      target_link_options(nativeui_wasm_app PRIVATE
        "--preload-file=${NATIVEUI_WASM_APP_FONT}@/nativeui/fonts/${_nativeui_wasm_app_font_name}")
    endif()
    set_target_properties(nativeui_wasm_app PROPERTIES
      SUFFIX ".html"
      LINK_FLAGS "-sEXIT_RUNTIME=0 -sALLOW_MEMORY_GROWTH=1")
    nativeui_enable_project_warnings(nativeui_wasm_app)
  endif()
endif()
