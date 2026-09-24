cmake_minimum_required(VERSION 3.24)
project(NativeUI_T045_Semantics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

function(nativeui_add_semantics_test target source)
  add_executable(${target} ${source})
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../include
    ${CMAKE_CURRENT_LIST_DIR}/..
  )

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()

  add_test(NAME ${target} COMMAND ${target})
endfunction()

enable_testing()
nativeui_add_semantics_test(t045_semantics ../t045_semantics_tests.cpp)
nativeui_add_semantics_test(t068_semantic_proxy t068_semantic_proxy_tests.cpp)
nativeui_add_semantics_test(t068_semantic_children_query t068_semantic_children_query_tests.cpp)
nativeui_add_semantics_test(t068_semantic_proxy_cache t068_semantic_proxy_cache_tests.cpp)
nativeui_add_semantics_test(t068_semantic_diff_regression t068_semantic_diff_regression_tests.cpp)
nativeui_add_semantics_test(t068_semantic_action_policy t068_semantic_action_policy_tests.cpp)
nativeui_add_semantics_test(t068_semantic_action_router t068_semantic_action_router_tests.cpp)
nativeui_add_semantics_test(t068_semantic_action_target_binding t068_semantic_action_target_binding_tests.cpp)
nativeui_add_semantics_test(t068_semantic_action_view_binding t068_semantic_action_view_binding_tests.cpp)
nativeui_add_semantics_test(t068_semantic_native_view_bridge t068_semantic_native_view_bridge_tests.cpp)
nativeui_add_semantics_test(t068_semantic_native_generation t068_semantic_native_generation_tests.cpp)
nativeui_add_semantics_test(t068_semantic_read_only t068_semantic_read_only_tests.cpp)
nativeui_add_semantics_test(t068_widget_semantic_values t068_widget_semantic_values_tests.cpp)
nativeui_add_semantics_test(t068_semantic_view_state t068_semantic_view_state_tests.cpp)
nativeui_add_semantics_test(t068_semantic_publication_batch t068_semantic_publication_batch_tests.cpp)
nativeui_add_semantics_test(t068_semantic_native_bounds t068_semantic_native_bounds_tests.cpp)
nativeui_add_semantics_test(t068_semantic_native_geometry_capture t068_semantic_native_geometry_capture_tests.cpp)
nativeui_add_semantics_test(semantic_macos_child_projection semantic_macos_child_projection_tests.cpp)
nativeui_add_semantics_test(semantic_macos_mapping semantic_macos_mapping_tests.cpp)
nativeui_add_semantics_test(semantic_macos_frame semantic_macos_frame_tests.cpp)

if(APPLE)
  enable_language(OBJCXX)
  nativeui_add_semantics_test(semantic_macos_appkit semantic_macos_appkit_tests.mm)
  nativeui_add_semantics_test(semantic_macos_appkit_frame semantic_macos_appkit_frame_tests.mm)
  nativeui_add_semantics_test(semantic_macos_proxy_cache semantic_macos_proxy_cache_tests.mm)
  nativeui_add_semantics_test(semantic_macos_children_callbacks semantic_macos_children_callbacks_tests.mm)
  nativeui_add_semantics_test(semantic_macos_interaction semantic_macos_interaction_tests.mm)
  set_target_properties(
    semantic_macos_appkit
    semantic_macos_appkit_frame
    semantic_macos_proxy_cache
    semantic_macos_children_callbacks
    semantic_macos_interaction
    PROPERTIES
      OBJCXX_STANDARD 20
      OBJCXX_STANDARD_REQUIRED YES
      OBJCXX_EXTENSIONS NO
  )
  find_library(T045_APPKIT_FRAMEWORK AppKit REQUIRED)
  target_link_libraries(semantic_macos_appkit PRIVATE "${T045_APPKIT_FRAMEWORK}")
  target_link_libraries(semantic_macos_appkit_frame PRIVATE "${T045_APPKIT_FRAMEWORK}")
  target_link_libraries(semantic_macos_proxy_cache PRIVATE "${T045_APPKIT_FRAMEWORK}")
  target_link_libraries(semantic_macos_children_callbacks PRIVATE "${T045_APPKIT_FRAMEWORK}")
  target_link_libraries(semantic_macos_interaction PRIVATE "${T045_APPKIT_FRAMEWORK}")
endif()

target_sources(t068_semantic_action_router PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp
)
target_sources(t068_semantic_action_view_binding PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp
)
target_sources(t068_semantic_native_view_bridge PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src/dispatcher.cpp
)
target_link_libraries(t068_semantic_action_router PRIVATE Threads::Threads)
target_link_libraries(t068_semantic_action_view_binding PRIVATE Threads::Threads)
target_link_libraries(t068_semantic_native_view_bridge PRIVATE Threads::Threads)
target_link_libraries(t068_semantic_proxy_cache PRIVATE Threads::Threads)
