cmake_minimum_required(VERSION 3.24)
project(T056ExternalConsumer LANGUAGES CXX)

find_package(NativeUI CONFIG REQUIRED)

if(NOT TARGET NativeUI::Core)
  message(FATAL_ERROR "T056 external consumer requires NativeUI::Core")
endif()
if(NOT COMMAND nativeui_add_binary_data)
  message(FATAL_ERROR "T056 external consumer requires nativeui_add_binary_data()")
endif()

nativeui_add_binary_data(T056PackageResources
  NAMESPACE t056_external::resources
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
  SOURCES
    resources/message.txt
    resources/other.txt
  ALIASES
    "resources/message.txt=message"
    [=[resources/other.txt=punct;../semi]=]
)

get_target_property(_resource_type T056PackageResources TYPE)
if(NOT _resource_type STREQUAL "STATIC_LIBRARY")
  message(FATAL_ERROR "nativeui_add_binary_data must create a STATIC_LIBRARY, got ${_resource_type}")
endif()

add_library(t056_embedded_resource_header OBJECT header.cpp)
target_link_libraries(t056_embedded_resource_header PRIVATE NativeUI::Core)

add_library(t056_umbrella_header OBJECT umbrella.cpp)
target_link_libraries(t056_umbrella_header PRIVATE NativeUI::Core)

add_executable(t056_package_consumer main.cpp)
target_link_libraries(t056_package_consumer PRIVATE T056PackageResources)

# Ordinary target customization must remain possible after helper invocation.
target_compile_definitions(t056_package_consumer PRIVATE T056_POST_HELPER=1)
