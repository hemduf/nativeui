cmake_minimum_required(VERSION 3.24)
project(NativeUIT051Benchmarks LANGUAGES C CXX)

if(NOT DEFINED NATIVEUI_SOURCE_DIR OR NATIVEUI_SOURCE_DIR STREQUAL "")
  get_filename_component(NATIVEUI_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(FATAL_ERROR "T051 benchmark project must be configured with CMAKE_BUILD_TYPE=Release")
endif()

set(NATIVEUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NATIVEUI_BUILD_PLATFORM OFF CACHE BOOL "" FORCE)

add_subdirectory("${NATIVEUI_SOURCE_DIR}" nativeui)

if(NOT DEFINED NATIVEUI_BENCHMARK_COMMIT_SHA OR NATIVEUI_BENCHMARK_COMMIT_SHA STREQUAL "")
  set(NATIVEUI_BENCHMARK_COMMIT_SHA "unknown")
endif()
string(REGEX MATCH "^[0-9]+" NATIVEUI_BENCHMARK_COMPILER_MAJOR "${CMAKE_CXX_COMPILER_VERSION}")
if(NATIVEUI_BENCHMARK_COMPILER_MAJOR STREQUAL "")
  set(NATIVEUI_BENCHMARK_COMPILER_MAJOR 0)
endif()

function(nativeui_configure_t051_target target)
  target_include_directories(${target} PRIVATE
    "${NATIVEUI_SOURCE_DIR}/tests"
    "${NATIVEUI_SOURCE_DIR}/tests/benchmarks"
  )
  target_link_libraries(${target} PRIVATE NativeUI::Core)
  target_compile_features(${target} PRIVATE cxx_std_20)
  target_compile_definitions(${target} PRIVATE
    NATIVEUI_BENCHMARK_COMMIT_SHA="${NATIVEUI_BENCHMARK_COMMIT_SHA}"
    NATIVEUI_BENCHMARK_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
    NATIVEUI_BENCHMARK_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
    NATIVEUI_BENCHMARK_COMPILER_MAJOR=${NATIVEUI_BENCHMARK_COMPILER_MAJOR}
    NATIVEUI_BENCHMARK_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    NATIVEUI_BENCHMARK_OS="${CMAKE_SYSTEM_NAME}"
    NATIVEUI_BENCHMARK_ARCH="${CMAKE_SYSTEM_PROCESSOR}"
  )
  nativeui_enable_project_warnings(${target})
endfunction()

add_executable(nativeui_benchmarks
  "${NATIVEUI_SOURCE_DIR}/tests/t051_benchmarks.cpp"
)
nativeui_configure_t051_target(nativeui_benchmarks)

add_executable(nativeui_t051_workload_contract_tests
  "${NATIVEUI_SOURCE_DIR}/tests/t051_workload_contract_tests.cpp"
)
nativeui_configure_t051_target(nativeui_t051_workload_contract_tests)

add_executable(nativeui_t052_benchmark_gate
  "${NATIVEUI_SOURCE_DIR}/tests/t052_benchmark_gate.cpp"
)
nativeui_configure_t051_target(nativeui_t052_benchmark_gate)
set_target_properties(nativeui_t052_benchmark_gate PROPERTIES OUTPUT_NAME "t052-benchmark-gate")

enable_testing()
add_test(NAME t051.workload_contract COMMAND nativeui_t051_workload_contract_tests)
