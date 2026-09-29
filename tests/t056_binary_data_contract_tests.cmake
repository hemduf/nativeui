if(NOT DEFINED SOURCE_DIR OR NOT EXISTS "${SOURCE_DIR}/cmake/NativeUIBinaryData.cmake")
  message(FATAL_ERROR "T056 contract tests require SOURCE_DIR with NativeUIBinaryData.cmake")
endif()
foreach(_required IN ITEMS
    cmake/NativeUIEmbedResource.cmake
    include/nativeui/embedded_resource.hpp
    tests/resources/t056_binary.bin)
  if(NOT EXISTS "${SOURCE_DIR}/${_required}")
    message(FATAL_ERROR "T056 contract fixture missing: ${_required}")
  endif()
endforeach()
file(TO_CMAKE_PATH "${SOURCE_DIR}" SOURCE_DIR_CMAKE)

set(_root "${CMAKE_CURRENT_BINARY_DIR}/t056-binary-data-contract")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

set(_t056_generator_args)
find_program(_t056_ninja NAMES ninja ninja-build)
if(_t056_ninja)
  list(APPEND _t056_generator_args -G Ninja)
endif()

function(_t056_write_fixture case body)
  set(_src "${_root}/${case}-src")
  file(MAKE_DIRECTORY "${_src}")
  file(WRITE "${_src}/alpha.txt" "alpha")
  file(WRITE "${_src}/unicode.txt" "unicode")
  file(WRITE "${_src}/empty.bin" "")
  file(COPY "${SOURCE_DIR}/tests/resources/t056_binary.bin" DESTINATION "${_src}")
  set(_project [=[
cmake_minimum_required(VERSION 3.24)
project(T056Contract LANGUAGES CXX)
add_library(nativeui_core_stub INTERFACE)
target_include_directories(nativeui_core_stub INTERFACE "@SOURCE_DIR_CMAKE@/include")
add_library(NativeUI::Core ALIAS nativeui_core_stub)
include("@SOURCE_DIR_CMAKE@/cmake/NativeUIBinaryData.cmake")
@CASE_BODY@
]=])
  set(CASE_BODY "${body}")
  string(CONFIGURE "${_project}" _configured @ONLY)
  file(WRITE "${_src}/CMakeLists.txt" "${_configured}")
endfunction()

function(_t056_configure_case case expect_success body)
  _t056_write_fixture("${case}" "${body}")
  set(_src "${_root}/${case}-src")
  set(_build "${_root}/${case}-build")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_src}" -B "${_build}" ${_t056_generator_args}
    RESULT_VARIABLE _result
    OUTPUT_VARIABLE _stdout
    ERROR_VARIABLE _stderr
  )
  set(_combined "${_stdout}\n${_stderr}")
  string(REGEX REPLACE "[ \t\r\n]+" " " _normalized "${_combined}")
  if(expect_success)
    if(NOT _result EQUAL 0)
      message(FATAL_ERROR "T056 ${case} unexpectedly failed (${_result})\n${_combined}")
    endif()
  else()
    if(_result EQUAL 0)
      message(FATAL_ERROR "T056 ${case} unexpectedly succeeded")
    endif()
    foreach(_expected IN LISTS ARGN)
      string(FIND "${_normalized}" "${_expected}" _found)
      if(_found EQUAL -1)
        message(FATAL_ERROR
          "T056 ${case} failed for the wrong reason\n"
          "missing diagnostic fragment: ${_expected}\noutput:\n${_combined}")
      endif()
    endforeach()
  endif()
endfunction()

# Namespace and basic helper validation.
_t056_configure_case(valid_minimal TRUE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt)")
_t056_configure_case(missing_namespace FALSE
  "nativeui_add_binary_data(MyResources SOURCES alpha.txt)"
  "NAMESPACE is required")
foreach(_ns IN ITEMS "::bad" "bad::" "bad::::name" "9bad" "bad-name" "bad.name")
  string(SHA256 _case_hash "${_ns}")
  string(SUBSTRING "${_case_hash}" 0 8 _case_suffix)
  _t056_configure_case("bad_namespace_${_case_suffix}" FALSE
    "nativeui_add_binary_data(MyResources NAMESPACE ${_ns} SOURCES alpha.txt)"
    "invalid NAMESPACE")
endforeach()
_t056_configure_case(duplicate_namespace FALSE
  "nativeui_add_binary_data(One NAMESPACE shared::resources SOURCES alpha.txt)\nnativeui_add_binary_data(Two NAMESPACE shared::resources SOURCES unicode.txt)"
  "NAMESPACE 'shared::resources'" "already used")
_t056_configure_case(existing_target FALSE
  "add_library(MyResources INTERFACE)\nnativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt)"
  "target 'MyResources' already exists")
_t056_configure_case(missing_sources FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources)"
  "SOURCES requires at least one file")

# Source containment, alias and uniqueness validation.
_t056_configure_case(missing_source FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES missing.bin)"
  "source is not an existing regular file")
file(WRITE "${_root}/outside.bin" "outside")
_t056_configure_case(path_escape FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES ../outside.bin)"
  "source escapes BASE_DIR")

# Symlink-aware containment: exercise where the host permits creating symlinks.
_t056_write_fixture(symlink_escape
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES link.bin)")
file(CREATE_LINK "${_root}/outside.bin" "${_root}/symlink_escape-src/link.bin" SYMBOLIC RESULT _link_result)
if(_link_result STREQUAL "0")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_root}/symlink_escape-src" -B "${_root}/symlink_escape-build" ${_t056_generator_args}
    RESULT_VARIABLE _link_configure_result OUTPUT_VARIABLE _link_out ERROR_VARIABLE _link_err)
  if(_link_configure_result EQUAL 0)
    message(FATAL_ERROR "T056 symlink escape unexpectedly configured successfully")
  endif()
  string(REGEX REPLACE "[ \t\r\n]+" " " _link_normalized "${_link_out}\n${_link_err}")
  string(FIND "${_link_normalized}" "source escapes BASE_DIR" _link_diag)
  if(_link_diag EQUAL -1)
    message(FATAL_ERROR "T056 symlink escape failed for wrong reason\n${_link_out}\n${_link_err}")
  endif()
else()
  message(STATUS "T056 symlink containment check skipped: ${_link_result}")
endif()
_t056_configure_case(duplicate_source FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt ./alpha.txt)"
  "duplicate canonical source")
_t056_configure_case(source_equals FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES bad=name.bin)"
  "contains '='")
_t056_configure_case(alias_unknown_source FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt ALIASES other.txt=other)"
  "does not exactly match")
_t056_configure_case(alias_duplicate FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt ALIASES alpha.txt=one alpha.txt=two)"
  "more than one alias")
foreach(_alias IN ITEMS "/bad" "bad/" "bad//id" "./bad" "bad/../id")
  string(SHA256 _case_hash "${_alias}")
  string(SUBSTRING "${_case_hash}" 0 8 _case_suffix)
  _t056_configure_case("bad_alias_${_case_suffix}" FALSE
    "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt ALIASES \"alpha.txt=${_alias}\")"
    "resource ID")
endforeach()
_t056_configure_case(alias_backslash FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt ALIASES [=[alpha.txt=bad\\id]=])"
  "resource ID" "backslashes")
_t056_configure_case(duplicate_final_id FALSE
  "nativeui_add_binary_data(MyResources NAMESPACE myapp::resources SOURCES alpha.txt unicode.txt ALIASES alpha.txt=same unicode.txt=same)"
  "duplicate final resource ID 'same'")

# A full build proves exact bytes (including NUL/high-bit/empty), deterministic
# ordering, aliasing, Unicode IDs and target coexistence.
set(_integration_body [=[
nativeui_add_binary_data(MyResources
  NAMESPACE myapp::resources
  SOURCES unicode.txt empty.bin t056_binary.bin alpha.txt
  ALIASES
    "t056_binary.bin=assets/binary"
    "unicode.txt=éxtra/ß.bin"
)
nativeui_add_binary_data(OtherResources
  NAMESPACE other::resources
  SOURCES unicode.txt
  ALIASES "unicode.txt=assets/binary"
)
add_executable(t056_check main.cpp)
target_link_libraries(t056_check PRIVATE MyResources OtherResources)
]=])
_t056_write_fixture(integration "${_integration_body}")
set(_integration_src "${_root}/integration-src")
set(_integration_build "${_root}/integration-build")
file(WRITE "${_integration_src}/main.cpp" [=[
#include <nativeui_binary_data/MyResources/resources.hpp>
#include <nativeui_binary_data/OtherResources/resources.hpp>
#include <array>
#include <cstddef>
#include <string_view>

static const ui::EmbeddedResourceEntry* find(std::span<const ui::EmbeddedResourceEntry> table,
                                             std::string_view id) {
  for (const auto& entry : table) {
    if (entry.id == id) return &entry;
  }
  return nullptr;
}

int main() {
  const auto primary = myapp::resources::table();
  if (primary.size() != 4) return 1;
  if (!(primary[0].id == "alpha.txt" && primary[1].id == "assets/binary" &&
        primary[2].id == "empty.bin" && primary[3].id == "éxtra/ß.bin")) return 2;
  const auto* binary = find(primary, "assets/binary");
  if (!binary || binary->bytes.size() != 6) return 3;
  const std::array<unsigned char, 6> expected{0, 1, 127, 128, 255, 10};
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (std::to_integer<unsigned char>(binary->bytes[i]) != expected[i]) return 4;
  }
  const auto* empty = find(primary, "empty.bin");
  if (!empty || !empty->bytes.empty()) return 5;
  const auto secondary = other::resources::table();
  if (secondary.size() != 1 || secondary[0].id != "assets/binary") return 6;
  if (secondary[0].bytes.size() != 7) return 7;
  return 0;
}
]=])
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${_integration_src}" -B "${_integration_build}" ${_t056_generator_args}
  RESULT_VARIABLE _configure_result OUTPUT_VARIABLE _configure_out ERROR_VARIABLE _configure_err)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR "T056 integration configure failed\n${_configure_out}\n${_configure_err}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_integration_build}" --config Release
  RESULT_VARIABLE _build_result OUTPUT_VARIABLE _build_out ERROR_VARIABLE _build_err)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR "T056 integration build failed\n${_build_out}\n${_build_err}")
endif()
set(_exe "${_integration_build}/t056_check${CMAKE_EXECUTABLE_SUFFIX}")
if(WIN32 AND EXISTS "${_integration_build}/Release/t056_check.exe")
  set(_exe "${_integration_build}/Release/t056_check.exe")
endif()
execute_process(COMMAND "${_exe}" RESULT_VARIABLE _run_result)
if(NOT _run_result EQUAL 0)
  message(FATAL_ERROR "T056 integration runtime check failed: ${_run_result}")
endif()

# Exact SHA-256 symbol vectors are part of the public generation contract.
set(_generated_root "${_integration_build}/nativeui_binary_data/MyResources/src")
foreach(_vector IN ITEMS
    "nativeui_bd_b408879fa97c_9b331f95797e6a85"
    "nativeui_bd_b408879fa97c_8d97696831deb1f1"
    "nativeui_bd_b408879fa97c_f997199136b2191b"
    "nativeui_bd_b408879fa97c_cddd5c79e2a7bcc7")
  file(GLOB _payloads "${_generated_root}/resource-*.cpp")
  set(_found FALSE)
  foreach(_payload IN LISTS _payloads)
    file(READ "${_payload}" _payload_text)
    string(FIND "${_payload_text}" "${_vector}" _index)
    if(NOT _index EQUAL -1)
      set(_found TRUE)
    endif()
  endforeach()
  if(NOT _found)
    message(FATAL_ERROR "T056 generated payload is missing exact digest symbol ${_vector}")
  endif()
endforeach()

# Generated text is path/timestamp independent and byte-identical across clean builds.
set(_second_build "${_root}/integration-build-2")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -S "${_integration_src}" -B "${_second_build}" ${_t056_generator_args}
  RESULT_VARIABLE _second_configure OUTPUT_QUIET ERROR_VARIABLE _second_error)
if(NOT _second_configure EQUAL 0)
  message(FATAL_ERROR "T056 reproducibility configure failed\n${_second_error}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_second_build}" --config Release --target MyResources
  RESULT_VARIABLE _second_build_result OUTPUT_QUIET ERROR_VARIABLE _second_build_error)
if(NOT _second_build_result EQUAL 0)
  message(FATAL_ERROR "T056 reproducibility build failed\n${_second_build_error}")
endif()
foreach(_relative IN ITEMS
    src/resources.cpp src/resource-0000.cpp src/resource-0001.cpp src/resource-0002.cpp src/resource-0003.cpp
    include/nativeui_binary_data/MyResources/resources.hpp)
  set(_first "${_integration_build}/nativeui_binary_data/MyResources/${_relative}")
  set(_second "${_second_build}/nativeui_binary_data/MyResources/${_relative}")
  execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${_first}" "${_second}" RESULT_VARIABLE _compare)
  if(NOT _compare EQUAL 0)
    message(FATAL_ERROR "T056 generated file is not reproducible: ${_relative}")
  endif()
  file(READ "${_first}" _text)
  foreach(_forbidden IN ITEMS "${_integration_src}" "${_integration_build}" "${_second_build}")
    string(FIND "${_text}" "${_forbidden}" _forbidden_index)
    if(NOT _forbidden_index EQUAL -1)
      message(FATAL_ERROR "T056 generated file leaks an absolute source/build path: ${_relative}")
    endif()
  endforeach()
endforeach()

# Incremental rebuild: changing alpha.txt regenerates/recompiles only its payload
# source; an unrelated payload file is not rewritten. Change size deliberately to
# catch any configure-time byte-count coupling.
set(_changed_payload "${_integration_build}/nativeui_binary_data/MyResources/src/resource-0000.cpp")
set(_stable_payload "${_integration_build}/nativeui_binary_data/MyResources/src/resource-0001.cpp")
file(TIMESTAMP "${_changed_payload}" _changed_before "%Y-%m-%dT%H:%M:%S")
file(TIMESTAMP "${_stable_payload}" _stable_before "%Y-%m-%dT%H:%M:%S")
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 2)
file(WRITE "${_integration_src}/alpha.txt" "alpha-now-longer")
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${_integration_build}" --config Release --target MyResources
  RESULT_VARIABLE _incremental_result OUTPUT_VARIABLE _incremental_out ERROR_VARIABLE _incremental_err)
if(NOT _incremental_result EQUAL 0)
  message(FATAL_ERROR "T056 incremental rebuild failed\n${_incremental_out}\n${_incremental_err}")
endif()
file(TIMESTAMP "${_changed_payload}" _changed_after "%Y-%m-%dT%H:%M:%S")
file(TIMESTAMP "${_stable_payload}" _stable_after "%Y-%m-%dT%H:%M:%S")
if(_changed_before STREQUAL _changed_after)
  message(FATAL_ERROR "T056 changed resource payload was not regenerated")
endif()
if(NOT _stable_before STREQUAL _stable_after)
  message(FATAL_ERROR "T056 unchanged resource payload was unexpectedly rewritten")
endif()
file(READ "${_changed_payload}" _changed_text)
string(FIND "${_changed_text}" "std::array<std::byte, 16>" _changed_size)
if(_changed_size EQUAL -1)
  message(FATAL_ERROR "T056 changed-size resource did not regenerate with the new byte count")
endif()

message(STATUS "T056 binary-data configure/build/bytes/digest/reproducibility/incremental contract passed")
