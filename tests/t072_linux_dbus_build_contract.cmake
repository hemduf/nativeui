cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_module "${SOURCE_DIR}/cmake/NativeUILinuxDbus.cmake")
set(_header "${SOURCE_DIR}/src/detail/linux_dbus.hpp")
set(_owner_header "${SOURCE_DIR}/src/detail/linux_dbus_application_transport_owner.hpp")
set(_platform_source "${SOURCE_DIR}/src/pugl_skia.cpp")
set(_platform_impl "${SOURCE_DIR}/src/detail/pugl_skia_windows.inc")
set(_consumer_platform "${SOURCE_DIR}/cmake/NativeUIConsumerPlatform.cmake")
set(_window_header "${SOURCE_DIR}/include/nativeui/window.hpp")
set(_source "${SOURCE_DIR}/src/linux_dbus.cpp")
set(_codec_source "${SOURCE_DIR}/src/linux_dbus_codec.cpp")

foreach(_required IN ITEMS
    "${_module}"
    "${_header}"
    "${_owner_header}"
    "${_platform_source}"
    "${_platform_impl}"
    "${_consumer_platform}"
    "${_window_header}"
    "${_source}"
    "${_codec_source}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "T072 RED: missing required internal Linux D-Bus transport file: ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${_module}" _dbus_module)
file(READ "${_platform_source}" _platform_source_text)
file(READ "${_platform_impl}" _platform_impl_text)
file(READ "${_consumer_platform}" _consumer_platform_text)
file(READ "${_window_header}" _window_header_text)

# The root owns only the Linux platform gate and module invocation; the module
# owns the private target/source details so those do not leak into unrelated
# platform configuration.
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
    "dbus-1"
    "PkgConfig")
  string(FIND "${_dbus_module}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 contract missing Linux D-Bus module token: ${_needle}")
  endif()
endforeach()

# The normative ownership amendment requires the already-tested one-transport
# owner to be part of the real T060 Application backend rather than remaining a
# standalone test helper. Keep the public header free of D-Bus types: it exposes
# only one generic private friend seam for source-private platform services.
foreach(_needle IN ITEMS
    "ApplicationBackendAccess"
    "friend struct detail::ApplicationBackendAccess")
  string(FIND "${_window_header_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 RED: Application backend access seam is not wired: ${_needle}")
  endif()
endforeach()

string(FIND "${_platform_source_text}" "detail/application_backend_access.hpp" _platform_access_include)
if(_platform_access_include EQUAL -1)
  message(FATAL_ERROR "T072 RED: real platform source does not include the Application backend access seam")
endif()

foreach(_needle IN ITEMS
    "LinuxDbusApplicationTransportOwner linux_dbus_transport"
    "linux_dbus_transport.shutdown()"
    "ApplicationBackendAccess::register_linux_dbus_client"
    "ApplicationBackendAccess::release_linux_dbus_client"
    "ApplicationBackendAccess::linux_dbus_transport_if_started")
  string(FIND "${_platform_impl_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 RED: T060 Application backend does not own/expose canonical Linux D-Bus transport: ${_needle}")
  endif()
endforeach()

# Source-tree Linux final consumers must link the canonical transport target.
string(FIND "${_root_cmake}" "target_link_libraries(nativeui PRIVATE nativeui_linux_dbus)" _root_transport_link)
if(_root_transport_link EQUAL -1)
  message(FATAL_ERROR "T072 RED: source-tree platform target is not linked to the canonical Linux D-Bus transport")
endif()

# Installed/build-tree packages compile the same private transport sources only
# on Linux and link system libdbus-1/Threads. macOS/Windows must not discover or
# link D-Bus as a side effect of nativeui_attach_platform().
foreach(_needle IN ITEMS
    "src/linux_dbus.cpp"
    "src/linux_dbus_codec.cpp"
    "PkgConfig::NATIVEUI_DBUS"
    "Threads::Threads")
  string(FIND "${_consumer_platform_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 RED: package platform path missing Linux D-Bus integration token: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "src/linux_dbus.cpp"
    "src/linux_dbus_codec.cpp")
  string(FIND "${_root_cmake}" "${_needle}" _install_source)
  if(_install_source EQUAL -1)
    message(FATAL_ERROR "T072 RED: installed platform source payload omits ${_needle}")
  endif()
endforeach()

file(GLOB_RECURSE _public_headers "${SOURCE_DIR}/include/nativeui/*.hpp")
foreach(_public_header IN LISTS _public_headers)
  file(READ "${_public_header}" _public_text)
  string(FIND "${_public_text}" "dbus/dbus.h" _leak)
  if(NOT _leak EQUAL -1)
    message(FATAL_ERROR "T072 public API leak: ${_public_header} includes dbus/dbus.h")
  endif()
  string(FIND "${_public_text}" "LinuxDbusTransport" _transport_leak)
  if(NOT _transport_leak EQUAL -1)
    message(FATAL_ERROR "T072 public API leak: ${_public_header} exposes LinuxDbusTransport")
  endif()
endforeach()

message(STATUS "T072 Linux D-Bus build/Application ownership contract satisfied")
