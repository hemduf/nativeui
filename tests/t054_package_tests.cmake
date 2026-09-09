if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T054 package tests require SOURCE_DIR")
endif()
if(NOT DEFINED BUILD_DIR OR NOT IS_DIRECTORY "${BUILD_DIR}")
  message(FATAL_ERROR "T054 package tests require BUILD_DIR")
endif()
if(NOT DEFINED BUILD_PACKAGE_DIR OR NOT IS_DIRECTORY "${BUILD_PACKAGE_DIR}")
  message(FATAL_ERROR "T054 package tests require BUILD_PACKAGE_DIR")
endif()
if(NOT DEFINED INSTALL_CMAKE_DIR OR INSTALL_CMAKE_DIR STREQUAL "")
  message(FATAL_ERROR "T054 package tests require INSTALL_CMAKE_DIR")
endif()
if(NOT DEFINED CONFIG OR CONFIG STREQUAL "")
  set(CONFIG Release)
endif()

set(_root "${BUILD_DIR}/t054-package-consumers")
set(_prefix_a "${_root}/prefix-a")
set(_prefix_b "${_root}/prefix-b")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

function(_t054_package_fail label output)
  message(FATAL_ERROR "T054 ${label} failed\n${output}")
endfunction()

set(_generator_args)
if(WIN32)
  list(APPEND _generator_args -A x64)
else()
  find_program(_t054_ninja NAMES ninja ninja-build REQUIRED)
  list(APPEND _generator_args -G Ninja "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()

# Review coverage for the byte-level PRODUCT_NAME validation. Build the invalid
# input inside the child CMake process so control bytes and deliberately malformed
# UTF-8 are not normalized by command-line/cache argument transport.
function(_t054_expect_invalid_product_bytes label expected_diagnostic)
  set(_codes ${ARGN})
  string(JOIN " " _codes_text ${_codes})
  set(_script "${_root}/product-${label}.cmake")
  file(WRITE "${_script}"
    "include([==[${SOURCE_DIR}/cmake/NativeUIApplication.cmake]==])\n"
    "string(ASCII ${_codes_text} _invalid_bytes)\n"
    "_nativeui_validate_application_product_name(\"Bad\${_invalid_bytes}Name\")\n")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -P "${_script}"
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr)
  set(_combined "${_stdout}\n${_stderr}")
  if(_result EQUAL 0)
    _t054_package_fail("PRODUCT_NAME ${label}"
      "invalid byte sequence unexpectedly passed validation")
  endif()
  string(FIND "${_combined}" "${expected_diagnostic}" _diagnostic_index)
  if(_diagnostic_index EQUAL -1)
    _t054_package_fail("PRODUCT_NAME ${label} diagnostic"
      "missing '${expected_diagnostic}' in:\n${_combined}")
  endif()
endfunction()

_t054_expect_invalid_product_bytes(ascii-control "ASCII control byte" 31)
_t054_expect_invalid_product_bytes(invalid-utf8 "not valid UTF-8" 195 40)

function(_t054_write_consumer source_dir)
  file(MAKE_DIRECTORY "${source_dir}")
  file(WRITE "${source_dir}/main.cpp" [=[
#include <nativeui/headless.hpp>
#include <nativeui/window.hpp>

using PollMember = bool (ui::EmbeddedView::*)();
volatile PollMember t054_platform_link_anchor = &ui::EmbeddedView::poll;

static int t054_run() {
    ui::HeadlessRenderer renderer({4.0f, 3.0f});
    return (renderer.pixel_width() == 4 && renderer.pixel_height() == 3 &&
            t054_platform_link_anchor != nullptr)
               ? 0
               : 1;
}

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) { return t054_run(); }
#else
int main() { return t054_run(); }
#endif
]=])
  file(WRITE "${source_dir}/extra.cpp"
    "int t054_caller_owned_extension() { return 54; }\n")

  file(WRITE "${source_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(T054ExternalApplication LANGUAGES CXX)
find_package(NativeUI CONFIG REQUIRED)
if(NOT COMMAND nativeui_add_application)
  message(FATAL_ERROR "nativeui_add_application missing from NativeUI package")
endif()

nativeui_add_application(T054App
  PRODUCT_NAME "T054 App"
  BUNDLE_ID com.nativeui.t054.package
  VERSION 007.08.0009
  SOURCES main.cpp)
# T054 must leave the target caller-owned and composable after helper creation.
target_sources(T054App PRIVATE extra.cpp)
target_compile_definitions(T054App PRIVATE T054_CALLER_EXTENSION=1)

nativeui_add_application(T054Sibling
  PRODUCT_NAME "T054 Sibling"
  BUNDLE_ID com.nativeui.t054.sibling
  VERSION 1.2.3
  SOURCES main.cpp)

get_target_property(_output T054App OUTPUT_NAME)
get_target_property(_type T054App TYPE)
get_target_property(_bundle T054App MACOSX_BUNDLE)
get_target_property(_win32 T054App WIN32_EXECUTABLE)
get_target_property(_id_a T054App NATIVEUI_CONSUMER_ID)
get_target_property(_id_b T054Sibling NATIVEUI_CONSUMER_ID)
get_target_property(_prefix_a T054App NATIVEUI_OBJC_RUNTIME_PREFIX)
get_target_property(_prefix_b T054Sibling NATIVEUI_OBJC_RUNTIME_PREFIX)
if(NOT _output STREQUAL "T054 App")
  message(FATAL_ERROR "T054 OUTPUT_NAME drifted: '${_output}'")
endif()
if(NOT _type STREQUAL "EXECUTABLE")
  message(FATAL_ERROR "T054 helper did not create an executable")
endif()
if(NOT _id_a STREQUAL "com.nativeui.t054.package" OR
   NOT _id_b STREQUAL "com.nativeui.t054.sibling")
  message(FATAL_ERROR "T054 did not delegate exact consumer identities")
endif()
if(_prefix_a STREQUAL _prefix_b)
  message(FATAL_ERROR "T054 two-app Objective-C runtime prefixes collide")
endif()
if(APPLE)
  if(NOT _bundle)
    message(FATAL_ERROR "T054 macOS app is not MACOSX_BUNDLE")
  endif()
  get_target_property(_bridge_a T054App NATIVEUI_OBJC_BRIDGE_TARGET)
  get_target_property(_bridge_b T054Sibling NATIVEUI_OBJC_BRIDGE_TARGET)
  if(NOT _bridge_a OR NOT _bridge_b OR _bridge_a STREQUAL _bridge_b)
    message(FATAL_ERROR "T054 two-app macOS bridge targets are missing or shared")
  endif()
  if(NOT CMAKE_NM)
    message(FATAL_ERROR "T054 macOS bridge audit requires CMAKE_NM")
  endif()
  file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/t054-objc-artifacts-$<CONFIG>.cmake"
    CONTENT
"set(T054_BRIDGE_A \"$<TARGET_FILE:${_bridge_a}>\")\nset(T054_BRIDGE_B \"$<TARGET_FILE:${_bridge_b}>\")\nset(T054_PREFIX_A \"${_prefix_a}\")\nset(T054_PREFIX_B \"${_prefix_b}\")\nset(T054_NM \"${CMAKE_NM}\")\n")
elseif(WIN32)
  if(NOT _win32)
    message(FATAL_ERROR "T054 Windows app is not WIN32_EXECUTABLE")
  endif()
else()
  if(_bundle OR _win32)
    message(FATAL_ERROR "T054 Linux app gained bundle/GUI-subsystem side effects")
  endif()
endif()
file(WRITE "${CMAKE_BINARY_DIR}/t054-contract.txt"
  "output=${_output}\n"
  "type=${_type}\n"
  "consumer_a=${_id_a}\n"
  "consumer_b=${_id_b}\n"
  "prefix_a=${_prefix_a}\n"
  "prefix_b=${_prefix_b}\n")
]=])
endfunction()

function(_t054_executable_path out_var build_dir product_name)
  if(APPLE)
    set(_path "${build_dir}/${product_name}.app/Contents/MacOS/${product_name}")
  elseif(WIN32)
    set(_path "${build_dir}/${CONFIG}/${product_name}.exe")
  elseif(EXISTS "${build_dir}/${CONFIG}/${product_name}")
    set(_path "${build_dir}/${CONFIG}/${product_name}")
  else()
    set(_path "${build_dir}/${product_name}")
  endif()
  set(${out_var} "${_path}" PARENT_SCOPE)
endfunction()

function(_t054_run_consumer label nativeui_dir out_contract out_plist)
  if(NOT EXISTS "${nativeui_dir}/NativeUIApplication.cmake")
    _t054_package_fail("${label} package surface"
      "missing ${nativeui_dir}/NativeUIApplication.cmake")
  endif()

  set(_src "${_root}/${label}-src")
  set(_build "${_root}/${label}-build")
  _t054_write_consumer("${_src}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${nativeui_dir}"
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error)
  if(NOT _configure_result EQUAL 0)
    _t054_package_fail("${label} configure"
      "${_configure_output}\n${_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build}"
      --config "${CONFIG}" --parallel 2
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error)
  if(NOT _build_result EQUAL 0)
    _t054_package_fail("${label} build" "${_build_output}\n${_build_error}")
  endif()

  _t054_executable_path(_executable "${_build}" "T054 App")
  if(NOT EXISTS "${_executable}")
    _t054_package_fail("${label} artifact" "expected executable not found: ${_executable}")
  endif()
  execute_process(
    COMMAND "${_executable}"
    RESULT_VARIABLE _run_result
    OUTPUT_VARIABLE _run_output
    ERROR_VARIABLE _run_error)
  if(NOT _run_result EQUAL 0)
    _t054_package_fail("${label} runtime" "${_run_output}\n${_run_error}")
  endif()

  if(APPLE)
    set(_objc_artifacts "${_build}/t054-objc-artifacts-${CONFIG}.cmake")
    if(NOT EXISTS "${_objc_artifacts}")
      _t054_package_fail("${label} Objective-C artifacts"
        "missing ${_objc_artifacts}")
    endif()
    include("${_objc_artifacts}")
    foreach(_required IN ITEMS T054_BRIDGE_A T054_BRIDGE_B T054_NM)
      if(NOT DEFINED ${_required} OR NOT EXISTS "${${_required}}")
        _t054_package_fail("${label} Objective-C artifacts"
          "missing ${_required}: ${${_required}}")
      endif()
    endforeach()
    if(NOT DEFINED T054_PREFIX_A OR T054_PREFIX_A STREQUAL "" OR
       NOT DEFINED T054_PREFIX_B OR T054_PREFIX_B STREQUAL "" OR
       T054_PREFIX_A STREQUAL T054_PREFIX_B OR
       T054_BRIDGE_A STREQUAL T054_BRIDGE_B)
      _t054_package_fail("${label} Objective-C isolation"
        "two application targets did not retain distinct bridge archives/prefixes")
    endif()
    foreach(_consumer IN ITEMS A B)
      execute_process(
        COMMAND "${CMAKE_COMMAND}"
          "-DARCHIVE=${T054_BRIDGE_${_consumer}}"
          "-DNM=${T054_NM}"
          "-DPREFIX=${T054_PREFIX_${_consumer}}"
          -P "${SOURCE_DIR}/tests/check_objc_runtime_prefix.cmake"
        RESULT_VARIABLE _prefix_result
        OUTPUT_VARIABLE _prefix_output
        ERROR_VARIABLE _prefix_error)
      if(NOT _prefix_result EQUAL 0)
        _t054_package_fail("${label} consumer ${_consumer} Objective-C symbol audit"
          "${_prefix_output}\n${_prefix_error}")
      endif()
    endforeach()
  endif()

  set(_contract "${_build}/t054-contract.txt")
  if(NOT EXISTS "${_contract}")
    _t054_package_fail("${label} contract" "missing ${_contract}")
  endif()
  set(${out_contract} "${_contract}" PARENT_SCOPE)

  if(APPLE)
    set(_plist "${_build}/T054 App.app/Contents/Info.plist")
    if(NOT EXISTS "${_plist}")
      _t054_package_fail("${label} plist" "missing ${_plist}")
    endif()
    file(READ "${_plist}" _plist_text)
    foreach(_expected IN ITEMS
        "<string>com.nativeui.t054.package</string>"
        "<string>T054 App</string>"
        "<string>007.08.0009</string>"
        "<key>NSHighResolutionCapable</key>")
      string(FIND "${_plist_text}" "${_expected}" _found)
      if(_found EQUAL -1)
        _t054_package_fail("${label} plist"
          "missing exact metadata '${_expected}' in ${_plist}")
      endif()
    endforeach()
    set(${out_plist} "${_plist}" PARENT_SCOPE)
  else()
    set(${out_plist} "" PARENT_SCOPE)
  endif()
endfunction()

function(_t054_configure_icon_metadata label nativeui_dir out_metadata)
  set(_src "${_root}/${label}-icon-src")
  set(_build "${_root}/${label}-icon-build")
  file(MAKE_DIRECTORY "${_src}")
  file(WRITE "${_src}/main.cpp" "int main() { return 0; }\n")
  file(WRITE "${_src}/nativeui-test.icns" "deterministic T054 icns fixture\n")
  file(WRITE "${_src}/nativeui-test.ico" "deterministic T054 ico fixture\n")
  file(WRITE "${_src}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.24)
project(T054IconMetadata LANGUAGES CXX)
find_package(NativeUI CONFIG REQUIRED)
nativeui_add_application(T054IconApp
  PRODUCT_NAME "T054 Icon App"
  BUNDLE_ID com.nativeui.t054.icon
  VERSION 1.2.3
  MACOS_ICON nativeui-test.icns
  WINDOWS_ICON nativeui-test.ico
  SOURCES main.cpp)
]=])
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${nativeui_dir}"
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _output
    ERROR_VARIABLE _error)
  if(NOT _result EQUAL 0)
    _t054_package_fail("${label} icon configure" "${_output}\n${_error}")
  endif()

  if(APPLE)
    # The acceptance contract is the built .app resource, not only a generated
    # plist reference. Ninja copies arbitrary .icns bytes as a bundle resource,
    # so this deterministic fixture does not depend on icon authoring tools.
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build "${_build}"
        --target T054IconApp --config "${CONFIG}" --parallel 2
      RESULT_VARIABLE _icon_build_result
      OUTPUT_VARIABLE _icon_build_output
      ERROR_VARIABLE _icon_build_error)
    if(NOT _icon_build_result EQUAL 0)
      _t054_package_fail("${label} icon build"
        "${_icon_build_output}\n${_icon_build_error}")
    endif()
    set(_resource
      "${_build}/T054 Icon App.app/Contents/Resources/nativeui-test.icns")
    if(NOT EXISTS "${_resource}")
      _t054_package_fail("${label} icon resource"
        "built .app is missing exact resource ${_resource}")
    endif()
    file(READ "${_src}/nativeui-test.icns" _source_icon_bytes)
    file(READ "${_resource}" _bundle_icon_bytes)
    if(NOT _source_icon_bytes STREQUAL _bundle_icon_bytes)
      _t054_package_fail("${label} icon resource"
        "built .app icon resource bytes differ from caller-owned input")
    endif()
    file(GLOB _metadata "${_build}/nativeui_application/*/Info.plist")
  elseif(WIN32)
    file(GLOB _metadata "${_build}/nativeui_application/*/application.rc")
  else()
    file(GLOB _metadata "${_build}/nativeui_application/*/*")
    if(_metadata)
      _t054_package_fail("${label} Linux metadata"
        "non-consuming icon keywords produced packaging metadata: ${_metadata}")
    endif()
    set(${out_metadata} "" PARENT_SCOPE)
    return()
  endif()
  list(LENGTH _metadata _metadata_count)
  if(NOT _metadata_count EQUAL 1)
    _t054_package_fail("${label} icon metadata"
      "expected exactly one generated metadata file, got ${_metadata_count}: ${_metadata}")
  endif()
  list(GET _metadata 0 _metadata_file)
  file(READ "${_metadata_file}" _metadata_text)
  if(APPLE)
    string(FIND "${_metadata_text}" "<string>nativeui-test.icns</string>" _icon_reference)
  else()
    string(FIND "${_metadata_text}" "ICON \"nativeui-test.ico\"" _icon_reference)
  endif()
  if(_icon_reference EQUAL -1)
    _t054_package_fail("${label} icon metadata"
      "generated metadata does not reference the exact icon basename")
  endif()
  set(${out_metadata} "${_metadata_file}" PARENT_SCOPE)
endfunction()

# Build-tree package parity first.
_t054_run_consumer(build-tree "${BUILD_PACKAGE_DIR}" _build_contract _build_plist)
_t054_configure_icon_metadata(build-tree "${BUILD_PACKAGE_DIR}" _build_metadata)

# Install, relocate, delete the original prefix, and consume only from the copy.
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
    --prefix "${_prefix_a}" --config "${CONFIG}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error)
if(NOT _install_result EQUAL 0)
  _t054_package_fail("install" "${_install_output}\n${_install_error}")
endif()
file(MAKE_DIRECTORY "${_prefix_b}")
file(COPY "${_prefix_a}/" DESTINATION "${_prefix_b}")
file(REMOVE_RECURSE "${_prefix_a}")
set(_nativeui_dir "${_prefix_b}/${INSTALL_CMAKE_DIR}")

_t054_run_consumer(installed-relocated "${_nativeui_dir}"
  _installed_contract _installed_plist)
_t054_configure_icon_metadata(installed-relocated "${_nativeui_dir}"
  _installed_metadata)

# Exact contract metadata must be independent of package/build location.
file(READ "${_build_contract}" _build_contract_text)
file(READ "${_installed_contract}" _installed_contract_text)
if(NOT _build_contract_text STREQUAL _installed_contract_text)
  _t054_package_fail("deterministic target metadata"
    "build-tree and relocated install-tree target contracts differ")
endif()

if(APPLE)
  file(READ "${_build_plist}" _build_plist_text)
  file(READ "${_installed_plist}" _installed_plist_text)
  if(NOT _build_plist_text STREQUAL _installed_plist_text)
    _t054_package_fail("deterministic plist"
      "clean build-tree/install-tree Info.plist contents differ")
  endif()
endif()
if(APPLE OR WIN32)
  file(READ "${_build_metadata}" _build_metadata_text)
  file(READ "${_installed_metadata}" _installed_metadata_text)
  if(NOT _build_metadata_text STREQUAL _installed_metadata_text)
    _t054_package_fail("deterministic icon metadata"
      "clean build-tree/install-tree generated icon metadata differs")
  endif()
endif()

# Installed T054 CMake must remain relocatable and contain no producer paths.
file(READ "${_nativeui_dir}/NativeUIApplication.cmake" _installed_helper)
foreach(_forbidden IN ITEMS "${SOURCE_DIR}" "${BUILD_DIR}")
  file(TO_CMAKE_PATH "${_forbidden}" _forbidden_normalized)
  string(FIND "${_installed_helper}" "${_forbidden_normalized}" _found)
  if(NOT _found EQUAL -1)
    _t054_package_fail("relocatability"
      "installed helper leaks original path ${_forbidden_normalized}")
  endif()
endforeach()

message(STATUS
  "T054 build-tree/relocated application contract passed: runnable final apps, caller composability, exact consumer identities, two-app Objective-C bridge symbol isolation, platform metadata/resources and deterministic package output")