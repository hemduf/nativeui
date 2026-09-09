if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T047 package tests require SOURCE_DIR")
endif()
if(NOT DEFINED BUILD_DIR OR NOT IS_DIRECTORY "${BUILD_DIR}")
  message(FATAL_ERROR "T047 package tests require BUILD_DIR")
endif()
if(NOT DEFINED BUILD_PACKAGE_DIR OR NOT EXISTS "${BUILD_PACKAGE_DIR}/NativeUIConfig.cmake")
  message(FATAL_ERROR "T047 package tests require BUILD_PACKAGE_DIR")
endif()
if(NOT DEFINED INSTALL_CMAKE_DIR OR INSTALL_CMAKE_DIR STREQUAL "")
  message(FATAL_ERROR "T047 package tests require INSTALL_CMAKE_DIR")
endif()
if(NOT DEFINED CONFIG OR CONFIG STREQUAL "")
  set(CONFIG Release)
endif()

set(_root "${BUILD_DIR}/t047-package-tests")
set(_prefix "${_root}/prefix")
set(_relocated "${_root}/relocated")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

set(_generator_args)
if(DEFINED GENERATOR AND NOT GENERATOR STREQUAL "")
  list(APPEND _generator_args -G "${GENERATOR}")
endif()
if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
  list(APPEND _generator_args -A "${GENERATOR_PLATFORM}")
endif()

function(_t047_fail label output)
  message(FATAL_ERROR "T047 ${label} failed\n${output}")
endfunction()

function(_t047_write_consumer source_dir platform)
  file(MAKE_DIRECTORY "${source_dir}")

  if(platform)
    # A volatile pointer-to-member initializer forces the final executable to
    # resolve a non-inline platform symbol from the attached implementation,
    # rather than merely compiling an otherwise dead static archive.
    file(WRITE "${source_dir}/main.cpp" [=[
#include <nativeui/headless.hpp>
#include <nativeui/window.hpp>

using PollMember = bool (ui::EmbeddedView::*)();
volatile PollMember t047_platform_link_anchor = &ui::EmbeddedView::poll;

int main() {
    ui::HeadlessRenderer renderer({4.0f, 3.0f});
    const bool core_ok = renderer.pixel_width() == 4 && renderer.pixel_height() == 3;
    const bool platform_linked = t047_platform_link_anchor != nullptr;
    return (core_ok && platform_linked) ? 0 : 1;
}
]=])
  else()
    file(WRITE "${source_dir}/main.cpp" [=[
#include <nativeui/headless.hpp>
int main() {
    ui::HeadlessRenderer renderer({4.0f, 3.0f});
    return (renderer.pixel_width() == 4 && renderer.pixel_height() == 3) ? 0 : 1;
}
]=])
  endif()

  set(_attach "")
  if(platform)
    set(_attach
      "nativeui_attach_platform(TARGET t047_consumer CONSUMER_ID com.nativeui.t047.external)\n")
  endif()

  file(WRITE "${source_dir}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.24)\n"
"project(T047ExternalConsumer LANGUAGES CXX)\n"
"find_package(NativeUI CONFIG REQUIRED)\n"
"if(NOT TARGET NativeUI::Core)\n"
"  message(FATAL_ERROR \"NativeUI::Core missing\")\n"
"endif()\n"
"if(TARGET NativeUI::NativeUI)\n"
"  message(FATAL_ERROR \"NativeUI::NativeUI must not be exported\")\n"
"endif()\n"
"if(NOT COMMAND nativeui_attach_platform)\n"
"  message(FATAL_ERROR \"nativeui_attach_platform missing\")\n"
"endif()\n"
"add_executable(t047_consumer main.cpp)\n"
"target_link_libraries(t047_consumer PRIVATE NativeUI::Core)\n"
"${_attach}")
endfunction()

function(_t047_configure_build name nativeui_dir platform)
  set(_src "${_root}/${name}-src")
  set(_build "${_root}/${name}-build")
  _t047_write_consumer("${_src}" ${platform})

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${nativeui_dir}"
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
  )
  if(NOT _configure_result EQUAL 0)
    _t047_fail("${name} configure"
      "${_configure_output}\n${_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build}"
      --config "${CONFIG}" --parallel 2
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error
  )
  if(NOT _build_result EQUAL 0)
    _t047_fail("${name} build" "${_build_output}\n${_build_error}")
  endif()

  set(_exe "${_build}/t047_consumer")
  if(WIN32)
    set(_exe "${_build}/${CONFIG}/t047_consumer.exe")
  elseif(EXISTS "${_build}/${CONFIG}/t047_consumer")
    set(_exe "${_build}/${CONFIG}/t047_consumer")
  endif()
  if(NOT EXISTS "${_exe}")
    _t047_fail("${name} artifact" "expected executable not found: ${_exe}")
  endif()

  execute_process(
    COMMAND "${_exe}"
    RESULT_VARIABLE _run_result
    OUTPUT_VARIABLE _run_output
    ERROR_VARIABLE _run_error
  )
  if(NOT _run_result EQUAL 0)
    _t047_fail("${name} runtime" "${_run_output}\n${_run_error}")
  endif()
endfunction()

# Build-tree and install-tree must expose the same public target/helper surface.
_t047_configure_build(build-tree "${BUILD_PACKAGE_DIR}" TRUE)

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
    --prefix "${_prefix}" --config "${CONFIG}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error
)
if(NOT _install_result EQUAL 0)
  _t047_fail("install" "${_install_output}\n${_install_error}")
endif()

set(_installed_nativeui_dir "${_prefix}/${INSTALL_CMAKE_DIR}")
if(NOT EXISTS "${_installed_nativeui_dir}/NativeUIConfig.cmake" OR
   NOT EXISTS "${_installed_nativeui_dir}/NativeUITargets.cmake")
  _t047_fail("installed config"
    "missing config/targets under ${_installed_nativeui_dir}")
endif()

# Distributed packages must carry the repository's legal/licensing payload.
# At minimum LICENSE/NOTICE/THIRD_PARTY are required by #47; the commercial
# distribution path also carries the EULA/privacy/terms/legal documents.
set(_legal_dir "${_prefix}/share/nativeui/legal")
foreach(_legal_doc IN ITEMS
    LICENSE.md NOTICE.md THIRD_PARTY.md
    EULA.md PRIVACY.md TERMS.md LEGAL.md)
  if(NOT EXISTS "${_legal_dir}/${_legal_doc}")
    _t047_fail("legal payload"
      "missing installed legal document: ${_legal_dir}/${_legal_doc}")
  endif()
endforeach()

# Core-only consumers need no Pugl discovery; platform attachment is exercised
# independently through the same installed config.
_t047_configure_build(install-core "${_installed_nativeui_dir}" FALSE)
_t047_configure_build(install-platform "${_installed_nativeui_dir}" TRUE)

# Installed CMake files must not embed the original source/build roots.
file(GLOB_RECURSE _installed_cmake_files "${_prefix}/*.cmake")
foreach(_cmake_file IN LISTS _installed_cmake_files)
  file(READ "${_cmake_file}" _cmake_text)
  string(FIND "${_cmake_text}" "${SOURCE_DIR}" _source_leak)
  string(FIND "${_cmake_text}" "${BUILD_DIR}" _build_leak)
  if(NOT _source_leak EQUAL -1 OR NOT _build_leak EQUAL -1)
    _t047_fail("relocatability scan"
      "installed CMake file leaks original tree path: ${_cmake_file}")
  endif()
endforeach()

# Copy the complete install prefix and prove platform attachment still builds
# from the relocated package without the original install path.
file(MAKE_DIRECTORY "${_relocated}")
file(COPY "${_prefix}/" DESTINATION "${_relocated}")
set(_relocated_nativeui_dir "${_relocated}/${INSTALL_CMAKE_DIR}")
_t047_configure_build(relocated-platform "${_relocated_nativeui_dir}" TRUE)

if(APPLE)
  # The installed public helper must preserve T053's strongest acceptance gate:
  # two independent MODULE consumers, distinct class/metaclass namespaces, and
  # simultaneous loading in one Objective-C runtime.
  set(_mac_build "${_root}/mac-two-consumers-build")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${SOURCE_DIR}/tests/t047_macos_package_consumers"
      -B "${_mac_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${_installed_nativeui_dir}"
    RESULT_VARIABLE _mac_configure_result
    OUTPUT_VARIABLE _mac_configure_output
    ERROR_VARIABLE _mac_configure_error
  )
  if(NOT _mac_configure_result EQUAL 0)
    _t047_fail("macOS two-consumer configure"
      "${_mac_configure_output}\n${_mac_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_mac_build}"
      --config "${CONFIG}" --parallel 2
    RESULT_VARIABLE _mac_build_result
    OUTPUT_VARIABLE _mac_build_output
    ERROR_VARIABLE _mac_build_error
  )
  if(NOT _mac_build_result EQUAL 0)
    _t047_fail("macOS two-consumer build"
      "${_mac_build_output}\n${_mac_build_error}")
  endif()

  set(_artifacts "${_mac_build}/t047_artifacts.cmake")
  if(NOT EXISTS "${_artifacts}")
    _t047_fail("macOS two-consumer artifacts" "missing ${_artifacts}")
  endif()
  include("${_artifacts}")
  foreach(_required IN ITEMS
      T047_BRIDGE_A T047_BRIDGE_B T047_MODULE_A T047_MODULE_B T047_LOADER T047_CORE)
    if(NOT DEFINED ${_required} OR NOT EXISTS "${${_required}}")
      _t047_fail("macOS two-consumer artifacts"
        "missing ${_required}: ${${_required}}")
    endif()
  endforeach()
  if(T047_BRIDGE_A STREQUAL T047_BRIDGE_B OR
     T047_PREFIX_A STREQUAL T047_PREFIX_B)
    _t047_fail("macOS two-consumer isolation"
      "consumer bridge archives or runtime prefixes collide")
  endif()

  find_program(_nm NAMES nm REQUIRED)
  foreach(_label IN ITEMS A B)
    execute_process(
      COMMAND "${CMAKE_COMMAND}"
        "-DARCHIVE=${T047_BRIDGE_${_label}}"
        "-DNM=${_nm}"
        "-DPREFIX=${T047_PREFIX_${_label}}"
        -P "${SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
      RESULT_VARIABLE _audit_result
      OUTPUT_VARIABLE _audit_output
      ERROR_VARIABLE _audit_error
    )
    if(NOT _audit_result EQUAL 0)
      _t047_fail("macOS consumer ${_label} symbol audit"
        "${_audit_output}\n${_audit_error}")
    endif()
  endforeach()

  execute_process(
    COMMAND "${T047_LOADER}"
      "${T047_MODULE_A}" "${T047_PREFIX_A}"
      "${T047_MODULE_B}" "${T047_PREFIX_B}"
    RESULT_VARIABLE _load_result
    OUTPUT_VARIABLE _load_output
    ERROR_VARIABLE _load_error
  )
  if(NOT _load_result EQUAL 0)
    _t047_fail("macOS two-consumer runtime coexistence"
      "${_load_output}\n${_load_error}")
  endif()
endif()

# Corrupt the relocated package deliberately and prove package discovery fails
# instead of falling back to an unpinned/system Skia.
file(GLOB _skia_archives "${_relocated}/share/nativeui/skia/lib/*")
list(LENGTH _skia_archives _skia_archive_count)
if(_skia_archive_count LESS 1)
  _t047_fail("pinned dependency fixture" "no installed Skia archive found")
endif()
list(GET _skia_archives 0 _removed_skia_archive)
file(REMOVE "${_removed_skia_archive}")
set(_broken_src "${_root}/broken-src")
set(_broken_build "${_root}/broken-build")
_t047_write_consumer("${_broken_src}" FALSE)
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${_broken_src}" -B "${_broken_build}"
    ${_generator_args}
    "-DNativeUI_DIR=${_relocated_nativeui_dir}"
  RESULT_VARIABLE _broken_result
  OUTPUT_VARIABLE _broken_output
  ERROR_VARIABLE _broken_error
)
if(_broken_result EQUAL 0)
  _t047_fail("pinned dependency failure" "broken Skia package unexpectedly configured")
endif()
set(_broken_combined "${_broken_output}\n${_broken_error}")
string(FIND "${_broken_combined}"
  "NativeUI package is incomplete: required pinned Skia file is missing"
  _diagnostic_found)
if(_diagnostic_found EQUAL -1)
  _t047_fail("pinned dependency diagnostic" "${_broken_combined}")
endif()

message(STATUS
  "T047 package contract passed: build-tree/install-tree parity, external Core/platform consumers, relocation, legal payload, no path leakage, pinned dependency failure, and applicable macOS runtime isolation")
