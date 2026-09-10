include_guard(GLOBAL)

function(nativeui_add_linux_dbus_transport)
  if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "nativeui_add_linux_dbus_transport() is Linux/Unix-only")
  endif()

  if(TARGET nativeui_linux_dbus)
    return()
  endif()

  find_package(PkgConfig REQUIRED)
  find_package(Threads REQUIRED)
  pkg_check_modules(NATIVEUI_DBUS REQUIRED IMPORTED_TARGET dbus-1)

  add_library(nativeui_linux_dbus STATIC
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/linux_dbus.cpp"
  )
  set_target_properties(nativeui_linux_dbus PROPERTIES
    POSITION_INDEPENDENT_CODE ON
  )
  target_compile_features(nativeui_linux_dbus PUBLIC cxx_std_20)
  target_include_directories(nativeui_linux_dbus PRIVATE
    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src"
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
endfunction()
