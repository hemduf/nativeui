cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_module "${SOURCE_DIR}/cmake/NativeUILinuxDbus.cmake")
set(_attach_module "${SOURCE_DIR}/cmake/NativeUIAttachPlatform.cmake")
set(_header "${SOURCE_DIR}/src/detail/linux_dbus.hpp")
set(_owner_header "${SOURCE_DIR}/src/detail/linux_dbus_application_transport_owner.hpp")
set(_platform_state_header "${SOURCE_DIR}/src/detail/application_platform_state.hpp")
set(_application_backend_source "${SOURCE_DIR}/src/linux_application_backend.cpp")
set(_platform_source "${SOURCE_DIR}/src/pugl_skia.cpp")
set(_window_header "${SOURCE_DIR}/include/nativeui/window.hpp")
set(_source "${SOURCE_DIR}/src/linux_dbus.cpp")
set(_codec_source "${SOURCE_DIR}/src/linux_dbus_codec.cpp")

foreach(_required IN ITEMS
    "${_module}"
    "${_attach_module}"
    "${_header}"
    "${_owner_header}"
    "${_platform_state_header}"
    "${_application_backend_source}"
    "${_platform_source}"
    "${_window_header}"
    "${_source}"
    "${_codec_source}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "T072 RED: missing required internal Linux D-Bus transport file: ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${_module}" _dbus_module)
file(READ "${_attach_module}" _attach_module_text)
file(READ "${_platform_state_header}" _platform_state_text)
file(READ "${_application_backend_source}" _application_backend_text)
file(READ "${_platform_source}" _platform_source_text)
file(READ "${_window_header}" _window_header_text)
file(READ "${_source}" _source_text)

# The root owns only the Linux platform gate and module invocation; the module
# owns the private target/source/package details so they do not leak into
# unrelated platform configuration.
foreach(_needle IN ITEMS
    "NativeUILinuxDbus.cmake"
    "nativeui_add_linux_dbus_transport")
  string(FIND "${_root_cmake}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 contract missing root integration token: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "nativeui_linux_dbus"
    "src/linux_dbus.cpp"
    "src/linux_dbus_codec.cpp"
    "src/linux_application_backend.cpp"
    "dbus-1"
    "PkgConfig"
    "Threads::Threads"
    "target_link_libraries(nativeui PRIVATE"
    "target_link_libraries(_nativeui_package_platform PRIVATE"
    "NativeUILinuxDbus.cmake")
  string(FIND "${_dbus_module}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 contract missing Linux D-Bus module token: ${_needle}")
  endif()
endforeach()

# T072 permits the std::once_flag synchronization primitive as the sole
# intentional mutable process-wide initialization state. The initialization
# result itself must be immutable after one-time initialization, not a second
# mutable namespace-scope variable.
string(FIND "${_source_text}" "g_dbus_threads_initialized" _mutable_init_result)
if(NOT _mutable_init_result EQUAL -1)
  message(FATAL_ERROR
    "T072 process-wide libdbus initialization result must not use a mutable global")
endif()

# Installed/build-tree Linux consumers must invoke the same private transport
# module after their platform target exists. macOS/Windows remain outside the
# UNIX-and-not-APPLE branch and therefore never discover libdbus.
foreach(_needle IN ITEMS
    "if(UNIX AND NOT APPLE)"
    "NativeUILinuxDbus.cmake"
    "nativeui_add_linux_dbus_transport()")
  string(FIND "${_attach_module_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 package attachment missing Linux D-Bus token: ${_needle}")
  endif()
endforeach()

# The normative ownership amendment is bound to the actual Application object,
# not a per-window/service helper. Application keeps the private platform state
# after its generic Impl declaration so reverse member destruction shuts D-Bus
# down before the Pugl PROGRAM backend.
foreach(_needle IN ITEMS
    "ApplicationPlatformState"
    "ApplicationBackendAccess"
    "friend struct detail::ApplicationBackendAccess"
    "std::unique_ptr<Impl> impl_"
    "std::unique_ptr<detail::ApplicationPlatformState> platform_state_")
  string(FIND "${_window_header_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 Application ownership seam missing token: ${_needle}")
  endif()
endforeach()
string(FIND "${_window_header_text}" "std::unique_ptr<Impl> impl_" _impl_pos)
string(FIND "${_window_header_text}" "std::unique_ptr<detail::ApplicationPlatformState> platform_state_" _platform_state_pos)
if(_impl_pos EQUAL -1 OR _platform_state_pos EQUAL -1 OR
   _platform_state_pos LESS _impl_pos)
  message(FATAL_ERROR
    "T072 Application platform state must be declared after Impl so it is destroyed first")
endif()

foreach(_needle IN ITEMS
    "LinuxDbusApplicationTransportOwner linux_dbus_transport"
    "ApplicationBackendAccess")
  string(FIND "${_platform_state_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 source-private Application state missing token: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "ApplicationBackendAccess::register_linux_dbus_client"
    "ApplicationBackendAccess::release_linux_dbus_client"
    "ApplicationBackendAccess::linux_dbus_transport_if_started"
    "application.platform_state_->linux_dbus_transport.register_client()"
    "application.platform_state_->linux_dbus_transport.release_client(client)"
    "application.platform_state_->linux_dbus_transport.transport_if_started()")
  string(FIND "${_application_backend_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 Application backend integration missing token: ${_needle}")
  endif()
endforeach()

string(FIND "${_platform_source_text}" "detail/application_platform_state.hpp" _platform_state_include)
if(_platform_state_include EQUAL -1)
  message(FATAL_ERROR
    "T072 platform backend must complete ApplicationPlatformState before Application destruction is emitted")
endif()

# Public NativeUI headers may name only the generic private friend/state seam;
# D-Bus transport/value types and dbus.h remain source-private.
file(GLOB_RECURSE _public_headers "${SOURCE_DIR}/include/nativeui/*.hpp")
foreach(_public_header IN LISTS _public_headers)
  file(READ "${_public_header}" _public_text)
  string(FIND "${_public_text}" "dbus/dbus.h" _dbus_header_leak)
  if(NOT _dbus_header_leak EQUAL -1)
    message(FATAL_ERROR "T072 public API leak: ${_public_header} includes dbus/dbus.h")
  endif()
  foreach(_forbidden IN ITEMS
      "LinuxDbusTransport"
      "LinuxDbusClientId"
      "LinuxDbusRequestId"
      "LinuxDbusValue")
    string(FIND "${_public_text}" "${_forbidden}" _transport_leak)
    if(NOT _transport_leak EQUAL -1)
      message(FATAL_ERROR "T072 public API leak: ${_public_header} exposes ${_forbidden}")
    endif()
  endforeach()
endforeach()

message(STATUS "T072 Linux D-Bus build/Application ownership contract satisfied")