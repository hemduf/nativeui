include_guard(GLOBAL)
include(CMakeParseArguments)

function(_nativeui_linux_dbus_source_root out_var)
  if(DEFINED NATIVEUI_PLATFORM_SOURCE_ROOT AND
     EXISTS "${NATIVEUI_PLATFORM_SOURCE_ROOT}/src/linux_dbus.cpp")
    set(${out_var} "${NATIVEUI_PLATFORM_SOURCE_ROOT}" PARENT_SCOPE)
    return()
  endif()

  get_filename_component(_nativeui_source_root
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
  if(NOT EXISTS "${_nativeui_source_root}/src/linux_dbus.cpp")
    message(FATAL_ERROR
      "NativeUI Linux D-Bus transport cannot locate its private source payload")
  endif()
  set(${out_var} "${_nativeui_source_root}" PARENT_SCOPE)
endfunction()

function(_nativeui_link_linux_dbus_transport target)
  if(TARGET NativeUI::NativeUI)
    target_link_libraries(nativeui PRIVATE "${target}")
  elseif(TARGET _nativeui_package_platform)
    target_link_libraries(_nativeui_package_platform PRIVATE "${target}")
  endif()
endfunction()

function(nativeui_add_linux_dbus_transport)
  cmake_parse_arguments(PARSE_ARGV 0 NUI "TRANSPORT_ONLY" "" "")
  if(NUI_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR
      "nativeui_add_linux_dbus_transport received unknown arguments: ${NUI_UNPARSED_ARGUMENTS}")
  endif()

  if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "nativeui_add_linux_dbus_transport() is Linux/Unix-only")
  endif()

  if(TARGET nativeui_linux_dbus)
    get_target_property(_nativeui_existing_transport_only
      nativeui_linux_dbus NATIVEUI_LINUX_DBUS_TRANSPORT_ONLY)
    if(_nativeui_existing_transport_only AND NOT NUI_TRANSPORT_ONLY)
      message(FATAL_ERROR
        "NativeUI Linux D-Bus transport was created in TRANSPORT_ONLY mode and cannot be reused for production Application integration")
    endif()
    _nativeui_link_linux_dbus_transport(nativeui_linux_dbus)
    return()
  endif()

  _nativeui_linux_dbus_source_root(_nativeui_source_root)

  foreach(_nativeui_required IN ITEMS
      "${_nativeui_source_root}/src/linux_dbus.cpp"
      "${_nativeui_source_root}/src/linux_dbus_codec.cpp"
      "${_nativeui_source_root}/src/linux_application_backend.cpp"
      "${_nativeui_source_root}/src/detail/linux_dbus.hpp"
      "${_nativeui_source_root}/src/detail/linux_dbus_codec.hpp"
      "${_nativeui_source_root}/src/detail/linux_dbus_application_transport_owner.hpp"
      "${_nativeui_source_root}/src/detail/application_platform_state.hpp")
    if(NOT EXISTS "${_nativeui_required}")
      message(FATAL_ERROR
        "NativeUI Linux D-Bus package is incomplete: ${_nativeui_required}")
    endif()
  endforeach()

  find_package(PkgConfig REQUIRED)
  find_package(Threads REQUIRED)
  pkg_check_modules(NATIVEUI_DBUS REQUIRED IMPORTED_TARGET dbus-1)

  set(_nativeui_linux_dbus_sources
    "${_nativeui_source_root}/src/linux_dbus.cpp"
    "${_nativeui_source_root}/src/linux_dbus_codec.cpp"
  )
  if(NOT NUI_TRANSPORT_ONLY)
    list(APPEND _nativeui_linux_dbus_sources
      "${_nativeui_source_root}/src/linux_application_backend.cpp")
  endif()

  add_library(nativeui_linux_dbus STATIC ${_nativeui_linux_dbus_sources})
  set_target_properties(nativeui_linux_dbus PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    NATIVEUI_LINUX_DBUS_TRANSPORT_ONLY "${NUI_TRANSPORT_ONLY}"
  )
  target_compile_features(nativeui_linux_dbus PUBLIC cxx_std_20)
  target_include_directories(nativeui_linux_dbus PRIVATE
    "${_nativeui_source_root}/src"
  )
  target_link_libraries(nativeui_linux_dbus PRIVATE
    NativeUI::Core
    PkgConfig::NATIVEUI_DBUS
    Threads::Threads
  )

  if(COMMAND nativeui_enable_project_warnings)
    nativeui_enable_project_warnings(nativeui_linux_dbus)
  endif()
  if(COMMAND nativeui_enable_sanitizers)
    nativeui_enable_sanitizers(nativeui_linux_dbus)
  endif()

  _nativeui_link_linux_dbus_transport(nativeui_linux_dbus)
endfunction()

# Source-tree packaging: Linux packages must carry the same private D-Bus
# sources and helper module so build-tree and installed native consumers use the
# canonical T072 implementation. Installed consumers do not execute this block
# because their CMake module is no longer adjacent to the repository source.
get_filename_component(_nativeui_linux_dbus_module_source_root
  "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(EXISTS "${_nativeui_linux_dbus_module_source_root}/src/linux_dbus.cpp")
  if(DEFINED NATIVEUI_INSTALL_SOURCE_DIR)
    install(FILES
      "${_nativeui_linux_dbus_module_source_root}/src/linux_dbus.cpp"
      "${_nativeui_linux_dbus_module_source_root}/src/linux_dbus_codec.cpp"
      "${_nativeui_linux_dbus_module_source_root}/src/linux_application_backend.cpp"
      DESTINATION "${NATIVEUI_INSTALL_SOURCE_DIR}/src"
    )
  endif()
  if(DEFINED NATIVEUI_INSTALL_CMAKE_DIR)
    install(FILES "${CMAKE_CURRENT_LIST_FILE}"
      DESTINATION "${NATIVEUI_INSTALL_CMAKE_DIR}")
  endif()
  if(DEFINED _nativeui_build_package_dir)
    configure_file(
      "${CMAKE_CURRENT_LIST_FILE}"
      "${_nativeui_build_package_dir}/NativeUILinuxDbus.cmake"
      COPYONLY
    )
  endif()
endif()
unset(_nativeui_linux_dbus_module_source_root)
