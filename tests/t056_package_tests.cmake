if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T056 package tests require SOURCE_DIR")
endif()
if(NOT DEFINED BUILD_DIR OR NOT IS_DIRECTORY "${BUILD_DIR}")
  message(FATAL_ERROR "T056 package tests require BUILD_DIR")
endif()
if(NOT DEFINED BUILD_PACKAGE_DIR OR NOT IS_DIRECTORY "${BUILD_PACKAGE_DIR}")
  message(FATAL_ERROR "T056 package tests require BUILD_PACKAGE_DIR")
endif()
if(NOT DEFINED INSTALL_CMAKE_DIR OR INSTALL_CMAKE_DIR STREQUAL "")
  message(FATAL_ERROR "T056 package tests require INSTALL_CMAKE_DIR")
endif()
if(NOT DEFINED CONFIG OR CONFIG STREQUAL "")
  set(CONFIG Release)
endif()

set(_root "${BUILD_DIR}/t056-package-consumers")
set(_prefix_a "${_root}/prefix-a")
set(_prefix_b "${_root}/prefix-b")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

function(_t056_package_fail label output)
  message(FATAL_ERROR "T056 ${label} failed\n${output}")
endfunction()

set(_generator_args)
if(WIN32)
  list(APPEND _generator_args -A x64)
else()
  find_program(_t056_ninja NAMES ninja ninja-build REQUIRED)
  list(APPEND _generator_args -G Ninja "-DCMAKE_BUILD_TYPE=${CONFIG}")
endif()

function(_t056_package_executable_path out_var build_dir target)
  if(WIN32)
    set(_path "${build_dir}/${CONFIG}/${target}.exe")
  elseif(EXISTS "${build_dir}/${CONFIG}/${target}")
    set(_path "${build_dir}/${CONFIG}/${target}")
  else()
    set(_path "${build_dir}/${target}")
  endif()
  set(${out_var} "${_path}" PARENT_SCOPE)
endfunction()

function(_t056_assert_generated_clean build_dir)
  set(_generated_root "${build_dir}/nativeui_binary_data/T056PackageResources")
  foreach(_required IN ITEMS
      "${_generated_root}/include/nativeui_binary_data/T056PackageResources/resources.hpp"
      "${_generated_root}/src/resources.cpp"
      "${_generated_root}/src/resource-0000.cpp"
      "${_generated_root}/src/resource-0001.cpp")
    if(NOT EXISTS "${_required}")
      _t056_package_fail("generated artifact" "missing ${_required}")
    endif()
    file(READ "${_required}" _generated_text)
    foreach(_forbidden IN ITEMS "${SOURCE_DIR}" "${BUILD_DIR}" "${CMAKE_CURRENT_LIST_DIR}")
      file(TO_CMAKE_PATH "${_forbidden}" _forbidden_normalized)
      string(FIND "${_generated_text}" "${_forbidden_normalized}" _found)
      if(NOT _found EQUAL -1)
        _t056_package_fail("generated relocatability" "${_required} leaks ${_forbidden_normalized}")
      endif()
    endforeach()
  endforeach()
endfunction()

function(_t056_run_consumer label nativeui_dir)
  set(_build "${_root}/${label}-build")
  execute_process(
    COMMAND "${CMAKE_COMMAND}"
      -S "${SOURCE_DIR}/tests/t056_external_consumer"
      -B "${_build}"
      ${_generator_args}
      "-DNativeUI_DIR=${nativeui_dir}"
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE
    RESULT_VARIABLE _configure_result
    OUTPUT_VARIABLE _configure_output
    ERROR_VARIABLE _configure_error)
  if(NOT _configure_result EQUAL 0)
    _t056_package_fail("${label} configure" "${_configure_output}\n${_configure_error}")
  endif()

  execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_build}"
      --config "${CONFIG}" --parallel 2
    RESULT_VARIABLE _build_result
    OUTPUT_VARIABLE _build_output
    ERROR_VARIABLE _build_error)
  if(NOT _build_result EQUAL 0)
    _t056_package_fail("${label} build" "${_build_output}\n${_build_error}")
  endif()

  _t056_package_executable_path(_executable "${_build}" t056_package_consumer)
  if(NOT EXISTS "${_executable}")
    _t056_package_fail("${label} artifact" "expected executable not found: ${_executable}")
  endif()
  execute_process(
    COMMAND "${_executable}"
    RESULT_VARIABLE _run_result
    OUTPUT_VARIABLE _run_output
    ERROR_VARIABLE _run_error)
  if(NOT _run_result EQUAL 0)
    _t056_package_fail("${label} runtime" "${_run_output}\n${_run_error}")
  endif()

  _t056_assert_generated_clean("${_build}")
endfunction()

# The build-tree package must expose the same helper and generator as install.
foreach(_module IN ITEMS NativeUIBinaryData.cmake NativeUIEmbedResource.cmake)
  if(NOT EXISTS "${BUILD_PACKAGE_DIR}/${_module}")
    _t056_package_fail("build-tree package" "missing ${BUILD_PACKAGE_DIR}/${_module}")
  endif()
endforeach()
_t056_run_consumer(build-tree "${BUILD_PACKAGE_DIR}")

# Install, relocate, delete the original prefix, and consume only from the copy.
execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
    --prefix "${_prefix_a}" --config "${CONFIG}"
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error)
if(NOT _install_result EQUAL 0)
  _t056_package_fail("install" "${_install_output}\n${_install_error}")
endif()
file(MAKE_DIRECTORY "${_prefix_b}")
file(COPY "${_prefix_a}/" DESTINATION "${_prefix_b}")
file(REMOVE_RECURSE "${_prefix_a}")

set(_nativeui_dir "${_prefix_b}/${INSTALL_CMAKE_DIR}")
foreach(_module IN ITEMS
    NativeUIConfig.cmake
    NativeUIBinaryData.cmake
    NativeUIEmbedResource.cmake)
  if(NOT EXISTS "${_nativeui_dir}/${_module}")
    _t056_package_fail("relocated package" "missing ${_nativeui_dir}/${_module}")
  endif()
endforeach()

file(TO_CMAKE_PATH "${SOURCE_DIR}" _source_normalized)
file(TO_CMAKE_PATH "${BUILD_DIR}" _build_normalized)
foreach(_package_file IN ITEMS
    "${_nativeui_dir}/NativeUIConfig.cmake"
    "${_nativeui_dir}/NativeUIBinaryData.cmake"
    "${_nativeui_dir}/NativeUIEmbedResource.cmake")
  file(READ "${_package_file}" _package_text)
  foreach(_forbidden IN ITEMS "${_source_normalized}" "${_build_normalized}")
    string(FIND "${_package_text}" "${_forbidden}" _found)
    if(NOT _found EQUAL -1)
      _t056_package_fail("relocatability" "${_package_file} leaks original path ${_forbidden}")
    endif()
  endforeach()
endforeach()

_t056_run_consumer(installed-relocated "${_nativeui_dir}")

message(STATUS "T056 build-tree and relocated install-tree binary-data consumers passed")
