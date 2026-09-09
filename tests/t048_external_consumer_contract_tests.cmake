if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "T048 external consumer contract requires SOURCE_DIR")
endif()

set(_fixture_root "${SOURCE_DIR}/tests/t048_external_consumers")
set(_required_files
  core/CMakeLists.txt
  core/main.cpp
  standalone/CMakeLists.txt
  standalone/main.cpp
  embedded/CMakeLists.txt
  embedded/main.cpp
)

foreach(_relative IN LISTS _required_files)
  if(NOT EXISTS "${_fixture_root}/${_relative}")
    message(FATAL_ERROR
      "T048 external consumer fixture is missing: ${_relative}")
  endif()
endforeach()

file(READ "${_fixture_root}/core/CMakeLists.txt" _core_cmake)
file(READ "${_fixture_root}/standalone/CMakeLists.txt" _standalone_cmake)
file(READ "${_fixture_root}/embedded/CMakeLists.txt" _embedded_cmake)
file(READ "${_fixture_root}/standalone/main.cpp" _standalone_main)
file(READ "${_fixture_root}/embedded/main.cpp" _embedded_main)

foreach(_cmake_text IN ITEMS
    "${_core_cmake}"
    "${_standalone_cmake}"
    "${_embedded_cmake}")
  if(NOT _cmake_text MATCHES "find_package\\(NativeUI[ ]+CONFIG[ ]+REQUIRED\\)")
    message(FATAL_ERROR
      "T048 fixture must consume NativeUI only through find_package(NativeUI CONFIG REQUIRED)")
  endif()
  if(NOT _cmake_text MATCHES "NativeUI::Core")
    message(FATAL_ERROR "T048 fixture must link NativeUI::Core")
  endif()
endforeach()

foreach(_cmake_text IN ITEMS "${_standalone_cmake}" "${_embedded_cmake}")
  if(NOT _cmake_text MATCHES "nativeui_attach_platform\\(")
    message(FATAL_ERROR "T048 native fixture must use nativeui_attach_platform()")
  endif()
  if(NOT _cmake_text MATCHES "CONSUMER_ID")
    message(FATAL_ERROR "T048 native fixture must provide a stable CONSUMER_ID")
  endif()
endforeach()

# Hosted Windows runners have no interactive desktop and Xvfb/OpenGL is not a
# reliable qualification surface for a package contract. Every native fixture
# therefore has two explicit paths: a portable --self-test that still links the
# attached platform implementation, and a real --native-smoke used where the CI
# supplies a supported graphical session. Keep both paths permanently present.
foreach(_native_main IN ITEMS "${_standalone_main}" "${_embedded_main}")
  string(FIND "${_native_main}" "--self-test" _self_test_index)
  string(FIND "${_native_main}" "--native-smoke" _native_smoke_index)
  if(_self_test_index EQUAL -1 OR _native_smoke_index EQUAL -1)
    message(FATAL_ERROR
      "T048 native fixture must expose both portable --self-test and real --native-smoke paths")
  endif()
endforeach()

file(GLOB_RECURSE _fixture_files LIST_DIRECTORIES FALSE "${_fixture_root}/*")
foreach(_fixture_file IN LISTS _fixture_files)
  file(READ "${_fixture_file}" _fixture_text)
  foreach(_forbidden IN ITEMS
      "../include"
      "../src"
      "_deps"
      "nativeui/detail/"
      "NativeUI::NativeUI"
      "NATIVEUI_PUGL_SOURCE"
      "NATIVEUI_SKIA_ROOT")
    string(FIND "${_fixture_text}" "${_forbidden}" _forbidden_index)
    if(NOT _forbidden_index EQUAL -1)
      message(FATAL_ERROR
        "T048 external fixture leaks private/source-tree contract '${_forbidden}' in ${_fixture_file}")
    endif()
  endforeach()
endforeach()

message(STATUS "T048 external consumer source contract passed")
