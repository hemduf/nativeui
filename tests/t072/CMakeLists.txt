cmake_minimum_required(VERSION 3.24)
project(NativeUIT072Contract LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(t072_core_stub STATIC
  "${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp"
)
add_library(NativeUI::Core ALIAS t072_core_stub)
target_include_directories(t072_core_stub PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}/../../include"
)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/NativeUILinuxDbus.cmake")
nativeui_add_linux_dbus_transport()

function(t072_add_test name source)
  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../src"
    "${CMAKE_CURRENT_LIST_DIR}/../../include"
  )
  target_link_libraries(${name} PRIVATE nativeui_linux_dbus)
  if(MSVC)
    target_compile_options(${name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endfunction()

t072_add_test(t072_build_contract build_contract.cpp)
t072_add_test(t072_validation_contract validation_contract.cpp)
t072_add_test(t072_lifecycle_contract lifecycle_contract.cpp)
t072_add_test(t072_resource_limits_contract resource_limits_contract.cpp)
t072_add_test(t072_request_contract request_contract.cpp)
t072_add_test(t072_method_call_contract method_call_contract.cpp)
t072_add_test(t072_value_codec_contract value_codec_contract.cpp)
target_link_libraries(t072_value_codec_contract PRIVATE PkgConfig::NATIVEUI_DBUS)
t072_add_test(t072_object_path_contract object_path_contract.cpp)
t072_add_test(t072_signal_contract signal_contract.cpp)
