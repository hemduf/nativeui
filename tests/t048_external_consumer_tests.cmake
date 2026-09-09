if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T048 external consumer tests require SOURCE_DIR")
endif()
if(NOT DEFINED BUILD_DIR OR NOT IS_DIRECTORY "${BUILD_DIR}")
  message(FATAL_ERROR "T048 external consumer tests require BUILD_DIR")
endif()
if(NOT DEFINED INSTALL_CMAKE_DIR OR INSTALL_CMAKE_DIR STREQUAL "")
  message(FATAL_ERROR "T048 external consumer tests require INSTALL_CMAKE_DIR")
endif()
if(NOT DEFINED CONFIG OR CONFIG STREQUAL "")
  set(CONFIG Release)
endif()

set(_root "${BUILD_DIR}/t048-external-consumers")
set(_prefix_a "${_root}/prefix-a")
set(_prefix_b "${_root}/prefix-b")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

function(_t048_fail label output)
  message(FATAL_ERROR "T048 ${label} failed\n${output}")
endfunction()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
    --prefix "${_prefix_a}" --config "${CONFIG}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error
)
if(NOT _install_result EQUAL 0)
  _t048_fail("install" "${_install_output}\n${_install_error}")
endif()

file(MAKE_DIRECTORY "${_prefix_b}")
file(COPY "${_prefix_a}/" DESTINATION "${_prefix_b}")
file(REMOVE_RECURSE "${_prefix_a}")

set(_nativeui_dir "${_prefix_b}/${INSTALL_CMAKE_DIR}")
if(NOT EXISTS "${_nativeui_dir}/NativeUIConfig.cmake")
  _t048_fail("relocation" "relocated NativeUIConfig.cmake missing from ${_nativeui_dir}")
endif()

set(_generator_args)
if(WIN32)
  list(APPEND _generator_args -A x64)
else()
  list(APPEND _generator_args -G Ninja "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()

function(_t048_executable_path out_var build_dir target)
  if(WIN32)
    set(_path "${build_dir}/${CONFIG}/${target}.exe")
  elseif(EXISTS "${build_dir}/${CONFIG}/${target}")
    set(_path "${build_dir}/${CONFIG}/${target}")
  else()
    set(_path "${build_dir}/${target}")
  endif()
  set(${out_var} "${_path}" PARENT_SCOPE)
endfunction()

function(_t048_execute label executable)
  execute_process(
    COMMAND "${executable}" ${ARGN}
    RESULT_VARIABLE _run_result
    OUTPUT_VARIABLE _run_output
    ERROR_VARIABLE _run_error
  )
  if(NOT _run_result EQUAL 0)
    _t048_fail("${label}" "${_run_output}\n${_run_error}")
  endif()
endfunction()

function(_t048_run_fixture name target native)
  set(_source "${SOURCE_DIR}/tests/t048_external_consumers/${name}")
  set(_build "${_root}/${name}-build")

  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_source}" -B "${_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${_nativeui_dir}"
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error
  )
  if(NOT _configure_result EQUAL 0)
    _t048_fail("${name} configure" "${_configure_output}\n${_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build}"
      --config "${CONFIG}" --parallel 2
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error
  )
  if(NOT _build_result EQUAL 0)
    _t048_fail("${name} build" "${_build_output}\n${_build_error}")
  endif()

  _t048_executable_path(_executable "${_build}" "${target}")
  if(NOT EXISTS "${_executable}")
    _t048_fail("${name} artifact" "expected executable not found: ${_executable}")
  endif()

  if(native)
    # The package contract must be deterministic on every hosted native runner.
    # Building this final executable already proves that public window symbols
    # resolve through nativeui_attach_platform(); --self-test exercises the
    # relocated Core runtime without depending on an interactive desktop.
    _t048_execute("${name} self-test" "${_executable}" --self-test)

    # Keep a real native lifecycle gate where CI has a stable graphical session.
    # The existing project-level macOS smoke exercises the same AppKit/Pugl path,
    # and this external run additionally proves that the relocated package owns
    # all implementation sources needed by a real consumer.
    if(APPLE)
      _t048_execute("${name} native smoke" "${_executable}" --native-smoke)
    endif()
  else()
    _t048_execute("${name} runtime" "${_executable}")
  endif()
endfunction()

_t048_run_fixture(core t048_core_consumer FALSE)
_t048_run_fixture(standalone t048_standalone_consumer TRUE)
_t048_run_fixture(embedded t048_embedded_consumer TRUE)

if(APPLE)
  set(_mac_build "${_root}/mac-two-consumers-build")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${SOURCE_DIR}/tests/t047_macos_package_consumers"
      -B "${_mac_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${_nativeui_dir}"
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
    RESULT_VARIABLE _mac_configure_result
    OUTPUT_VARIABLE _mac_configure_output
    ERROR_VARIABLE _mac_configure_error
  )
  if(NOT _mac_configure_result EQUAL 0)
    _t048_fail("macOS relocated two-consumer configure"
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
    _t048_fail("macOS relocated two-consumer build"
      "${_mac_build_output}\n${_mac_build_error}")
  endif()

  set(_artifacts "${_mac_build}/t047_artifacts.cmake")
  if(NOT EXISTS "${_artifacts}")
    _t048_fail("macOS relocated two-consumer artifacts" "missing ${_artifacts}")
  endif()
  include("${_artifacts}")

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
      _t048_fail("macOS relocated consumer ${_label} symbol audit"
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
    _t048_fail("macOS relocated two-consumer runtime coexistence"
      "${_load_output}\n${_load_error}")
  endif()
endif()

message(STATUS
  "T048 relocated external consumers passed: cross-platform Core/self-tests, platform link attachment, macOS native lifecycle, and macOS two-consumer Objective-C isolation")
