if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Skia ICU runtime tests require SOURCE_DIR")
endif()
if(NOT DEFINED TEST_DIR OR NOT IS_ABSOLUTE "${TEST_DIR}")
  message(FATAL_ERROR "Skia ICU runtime tests require an absolute TEST_DIR")
endif()
if(NOT DEFINED CONFIG OR CONFIG STREQUAL "")
  set(CONFIG Release)
endif()
if(NOT EXISTS "${SOURCE_DIR}/cmake/NativeUIRuntimeData.cmake")
  message(FATAL_ERROR "Skia ICU runtime data attachment helper is missing")
endif()

set(_root "${TEST_DIR}/skia-icu-runtime")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")
set(_fixture "${SOURCE_DIR}/tests/skia_icu_runtime_consumers")
set(_runtime_module "${SOURCE_DIR}/cmake/NativeUIRuntimeData.cmake")

# These builds are separate, tiny fixtures. Keep local compilation serial, just
# like the enclosing NativeUI validation, even if the caller has a wider limit.
set(ENV{CMAKE_BUILD_PARALLEL_LEVEL} 1)
set(_generator_args)
if(DEFINED GENERATOR AND NOT GENERATOR STREQUAL "")
  list(APPEND _generator_args -G "${GENERATOR}")
  if(DEFINED GENERATOR_PLATFORM AND NOT GENERATOR_PLATFORM STREQUAL "")
    list(APPEND _generator_args -A "${GENERATOR_PLATFORM}")
  endif()
else()
  find_program(_icu_ninja NAMES ninja ninja-build)
  if(_icu_ninja)
    list(APPEND _generator_args -G Ninja)
  endif()
endif()

function(_icu_run label)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
    TIMEOUT 300)
  if(NOT "${_result}" STREQUAL "0")
    message(FATAL_ERROR "Skia ICU ${label} failed (${_result})\n${_stdout}\n${_stderr}")
  endif()
endfunction()

function(_icu_verify_data source)
  file(SHA256 "${source}" _source_hash)
  foreach(_artifact IN LISTS ARGN)
    get_filename_component(_directory "${_artifact}" DIRECTORY)
    set(_data "${_directory}/icudtl.dat")
    if(NOT EXISTS "${_artifact}" OR NOT EXISTS "${_data}")
      message(FATAL_ERROR "Skia ICU runtime data is absent beside ${_artifact}")
    endif()
    file(SHA256 "${_data}" _data_hash)
    if(NOT _data_hash STREQUAL _source_hash)
      message(FATAL_ERROR "Skia ICU runtime data changed bytes: ${_data}")
    endif()
  endforeach()
endfunction()

function(_icu_verify_recovery build source)
  set(_data_files)
  foreach(_artifact IN LISTS ARGN)
    string(SHA256 _artifact_key "${_artifact}")
    file(TIMESTAMP "${_artifact}" _artifact_time "%Y-%m-%dT%H:%M:%S.%fZ" UTC)
    set("_before_${_artifact_key}" "${_artifact_time}")
    get_filename_component(_directory "${_artifact}" DIRECTORY)
    list(APPEND _data_files "${_directory}/icudtl.dat")
  endforeach()
  list(REMOVE_DUPLICATES _data_files)
  file(REMOVE ${_data_files})
  foreach(_data IN LISTS _data_files)
    if(EXISTS "${_data}")
      message(FATAL_ERROR "Skia ICU recovery fixture could not remove ${_data}")
    endif()
  endforeach()

  _icu_run("up-to-date rebuild restores runtime data"
    "${CMAKE_COMMAND}" --build "${build}" --config "${CONFIG}")
  _icu_verify_data("${source}" ${ARGN})
  foreach(_artifact IN LISTS ARGN)
    string(SHA256 _artifact_key "${_artifact}")
    file(TIMESTAMP "${_artifact}" _artifact_time "%Y-%m-%dT%H:%M:%S.%fZ" UTC)
    if(NOT _artifact_time STREQUAL "${_before_${_artifact_key}}")
      message(FATAL_ERROR
        "Skia ICU recovery unnecessarily rebuilt the existing artifact: ${_artifact}")
    endif()
  endforeach()
endfunction()

# Exercise the Windows data contract on every host. Only the attachment wrapper
# simulates WIN32; project(), compiler discovery, targets and output suffixes all
# retain the actual host platform. Include high-bit bytes in the mock data.
string(ASCII 1 2 127 128 255 _mock_bytes)
set(_mock_data "${_root}/mock-icudtl.dat")
file(WRITE "${_mock_data}" "NativeUI ICU staging fixture\n${_mock_bytes}\n")
set(_mock_build "${_root}/mock-build")
_icu_run("portable fixture configure"
  "${CMAKE_COMMAND}" -S "${_fixture}" -B "${_mock_build}"
  ${_generator_args}
  "-DCMAKE_BUILD_TYPE=${CONFIG}"
  "-DRUNTIME_DATA_MODE=mock"
  "-DRUNTIME_DATA_MODULE=${_runtime_module}"
  "-DICU_DATA_SOURCE=${_mock_data}")
_icu_run("portable fixture build"
  "${CMAKE_COMMAND}" --build "${_mock_build}" --config "${CONFIG}")
include("${_mock_build}/icu-artifacts-${CONFIG}.cmake")
set(_mock_artifacts
  "${ICU_RUNTIME_ONE}" "${ICU_RUNTIME_TWO}"
  "${ICU_RUNTIME_SHARED}" "${ICU_RUNTIME_LIBRARY}"
  "${ICU_RUNTIME_MODULE}")
_icu_verify_data("${_mock_data}" ${_mock_artifacts})
_icu_verify_recovery("${_mock_build}" "${_mock_data}" ${_mock_artifacts})

# A malformed dependency must fail during configuration, with the missing path
# identified, rather than leaving a successful build with unavailable Unicode.
set(_missing_data "${_root}/missing-icudtl.dat")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${_fixture}" -B "${_root}/missing-build"
    ${_generator_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DRUNTIME_DATA_MODE=mock"
    "-DRUNTIME_DATA_MODULE=${_runtime_module}"
    "-DICU_DATA_SOURCE=${_missing_data}"
  RESULT_VARIABLE _missing_result
  OUTPUT_VARIABLE _missing_stdout
  ERROR_VARIABLE _missing_stderr
  TIMEOUT 120)
if("${_missing_result}" STREQUAL "0")
  message(FATAL_ERROR "Skia ICU attachment accepted a missing runtime data source")
endif()
string(FIND "${_missing_stdout}\n${_missing_stderr}" "missing-icudtl.dat" _missing_diagnostic)
if(_missing_diagnostic EQUAL -1)
  message(FATAL_ERROR
    "Skia ICU missing-source fixture failed for another reason\n"
    "${_missing_stdout}\n${_missing_stderr}")
endif()
message(STATUS "Skia ICU portable staging/bytes/idempotence/recovery contract passed")

# Both build-tree and relocated installed packages exercise the same real
# executable/module behavior. A separate host and working directory prevent
# executable-local or working-directory data from masking module fallback.
function(_icu_qualify_windows_package label package_dir build out_data_source)
  _icu_run("${label} consumer configure"
    "${CMAKE_COMMAND}" -S "${_fixture}" -B "${build}"
    ${_generator_args}
    "-DCMAKE_BUILD_TYPE=${CONFIG}"
    "-DRUNTIME_DATA_MODE=windows"
    "-DNativeUI_DIR=${package_dir}")
  _icu_run("${label} consumer build"
    "${CMAKE_COMMAND}" --build "${build}" --config "${CONFIG}")
  include("${build}/icu-artifacts-${CONFIG}.cmake")
  _icu_verify_data("${ICU_RUNTIME_DATA_SOURCE}" "${ICU_RUNTIME_CORE}" "${ICU_RUNTIME_MODULE}")
  get_filename_component(_host_directory "${ICU_RUNTIME_HOST}" DIRECTORY)
  if(EXISTS "${_host_directory}/icudtl.dat")
    message(FATAL_ERROR "Skia ICU ${label} staging wrote into the independent host directory")
  endif()
  set(_empty_working_directory "${build}/empty-working-directory")
  file(MAKE_DIRECTORY "${_empty_working_directory}")

  foreach(_attempt RANGE 1 2)
    execute_process(
      COMMAND "${ICU_RUNTIME_CORE}"
      WORKING_DIRECTORY "${_empty_working_directory}"
      RESULT_VARIABLE _core_result OUTPUT_VARIABLE _core_stdout ERROR_VARIABLE _core_stderr
      TIMEOUT 60)
    if(NOT "${_core_result}" STREQUAL "0")
      message(FATAL_ERROR
        "Skia ICU ${label} executable Unicode failed (${_core_result})\n${_core_stdout}\n${_core_stderr}")
    endif()
    execute_process(
      COMMAND "${ICU_RUNTIME_HOST}" "${ICU_RUNTIME_MODULE}"
      WORKING_DIRECTORY "${_empty_working_directory}"
      RESULT_VARIABLE _host_result OUTPUT_VARIABLE _host_stdout ERROR_VARIABLE _host_stderr
      TIMEOUT 60)
    if(NOT "${_host_result}" STREQUAL "0")
      message(FATAL_ERROR
        "Skia ICU ${label} module-local Unicode failed (${_host_result})\n${_host_stdout}\n${_host_stderr}")
    endif()
    if(EXISTS "${_host_directory}/icudtl.dat" OR
       EXISTS "${_empty_working_directory}/icudtl.dat")
      message(FATAL_ERROR "Skia ICU ${label} consumer changed its host or working directory")
    endif()
    if(_attempt EQUAL 1)
      _icu_verify_recovery("${build}" "${ICU_RUNTIME_DATA_SOURCE}"
        "${ICU_RUNTIME_CORE}" "${ICU_RUNTIME_MODULE}")
    endif()
  endforeach()
  set("${out_data_source}" "${ICU_RUNTIME_DATA_SOURCE}" PARENT_SCOPE)
  message(STATUS "Skia ICU ${label} executable/module fallback and recovery passed")
endfunction()

# The native loader contract requires Windows and the actual pinned archive.
# Keep it distinct from the portable mock instead of claiming emulated runtime
# loading establishes Windows Unicode or plugin correctness.
if(WIN32)
  if(NOT DEFINED BUILD_PACKAGE_DIR OR
     NOT EXISTS "${BUILD_PACKAGE_DIR}/NativeUIConfig.cmake")
    message(FATAL_ERROR "Windows Skia ICU runtime tests require BUILD_PACKAGE_DIR")
  endif()
  _icu_qualify_windows_package("Windows build-tree" "${BUILD_PACKAGE_DIR}"
    "${_root}/windows-build" _build_tree_data)

  if(DEFINED BUILD_DIR AND NOT BUILD_DIR STREQUAL "")
    if(NOT EXISTS "${BUILD_DIR}/CMakeCache.txt" OR
       NOT EXISTS "${BUILD_DIR}/cmake_install.cmake")
      message(FATAL_ERROR "Windows Skia ICU install qualification requires a configured BUILD_DIR")
    endif()
    if(NOT DEFINED INSTALL_CMAKE_DIR OR INSTALL_CMAKE_DIR STREQUAL "")
      set(INSTALL_CMAKE_DIR "lib/cmake/NativeUI")
    endif()
    if(IS_ABSOLUTE "${INSTALL_CMAKE_DIR}")
      message(FATAL_ERROR "Skia ICU INSTALL_CMAKE_DIR must be relative to the package prefix")
    endif()

    set(_installed_prefix "${_root}/installed-prefix")
    set(_relocated_prefix "${_root}/relocated-prefix")
    _icu_run("Windows package install"
      "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
      --prefix "${_installed_prefix}" --config "${CONFIG}")
    file(RENAME "${_installed_prefix}" "${_relocated_prefix}")
    set(_relocated_package "${_relocated_prefix}/${INSTALL_CMAKE_DIR}")
    if(NOT EXISTS "${_relocated_package}/NativeUIConfig.cmake" OR
       EXISTS "${_installed_prefix}")
      message(FATAL_ERROR "Skia ICU fixture did not relocate the complete installed package")
    endif()
    set(_relocated_build "${_root}/windows-relocated-build")
    _icu_qualify_windows_package("Windows relocated package" "${_relocated_package}"
      "${_relocated_build}" _installed_data)

    # Require the exported path to belong to the relocated fixture before
    # moving anything. A broken config must never make this test alter the
    # original archive, source dependency cache or NativeUI build-tree package.
    file(REAL_PATH "${_relocated_prefix}" _relocated_canonical)
    file(REAL_PATH "${_installed_data}" _installed_data_canonical)
    cmake_path(IS_PREFIX _relocated_canonical "${_installed_data_canonical}"
      NORMALIZE _data_is_fixture_owned)
    if(NOT _data_is_fixture_owned)
      message(FATAL_ERROR
        "Skia ICU installed data still resolves outside its relocated prefix: ${_installed_data}")
    endif()
    file(SHA256 "${_build_tree_data}" _pinned_data_hash)
    file(SHA256 "${_installed_data}" _installed_data_hash)
    if(NOT _installed_data_hash STREQUAL _pinned_data_hash)
      message(FATAL_ERROR "Skia ICU installation changed the pinned runtime data bytes")
    endif()

    # Reconfigure the already-qualified consumer with the installed payload
    # absent. find_package itself must reject the incomplete relocated package.
    # Restore the same bytes before checking diagnostics or running recovery.
    set(_saved_data "${_root}/saved-installed-icudtl.dat")
    file(RENAME "${_installed_data}" "${_saved_data}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -S "${_fixture}" -B "${_relocated_build}"
        ${_generator_args}
        "-DCMAKE_BUILD_TYPE=${CONFIG}"
        "-DRUNTIME_DATA_MODE=windows"
        "-DNativeUI_DIR=${_relocated_package}"
      RESULT_VARIABLE _package_missing_result
      OUTPUT_VARIABLE _package_missing_stdout
      ERROR_VARIABLE _package_missing_stderr
      TIMEOUT 120)
    file(RENAME "${_saved_data}" "${_installed_data}")
    if("${_package_missing_result}" STREQUAL "0")
      message(FATAL_ERROR "find_package accepted installed Skia ICU data that was missing")
    endif()
    set(_package_missing_output "${_package_missing_stdout}\n${_package_missing_stderr}")
    string(REGEX REPLACE "[ \t\r\n]+" " " _package_missing_normalized "${_package_missing_output}")
    string(FIND "${_package_missing_normalized}"
      "matching pinned Skia ICU runtime data is missing" _package_missing_reason)
    string(FIND "${_package_missing_normalized}" "${_installed_data}" _package_missing_path)
    if(_package_missing_reason EQUAL -1 OR _package_missing_path EQUAL -1)
      message(FATAL_ERROR
        "Skia ICU incomplete installed package failed for another reason\n${_package_missing_output}")
    endif()
    file(SHA256 "${_installed_data}" _restored_data_hash)
    if(NOT _restored_data_hash STREQUAL _pinned_data_hash)
      message(FATAL_ERROR "Skia ICU installed-source restoration changed bytes")
    endif()
    _icu_qualify_windows_package("Windows restored installed package" "${_relocated_package}"
      "${_relocated_build}" _restored_data)
  else()
    message(STATUS "Windows Skia ICU installed-package qualification requires BUILD_DIR")
  endif()
else()
  message(STATUS "Windows Skia ICU native loader qualification requires a Windows executor")
endif()
