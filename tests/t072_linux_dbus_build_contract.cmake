cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED SOURCE_DIR)
  message(FATAL_ERROR "SOURCE_DIR is required")
endif()

set(_module "${SOURCE_DIR}/cmake/NativeUILinuxDbus.cmake")
set(_attach_module "${SOURCE_DIR}/cmake/NativeUIAttachPlatform.cmake")
set(_header "${SOURCE_DIR}/src/detail/linux_dbus.hpp")
set(_client_operations_header "${SOURCE_DIR}/src/detail/linux_dbus_client_operations.hpp")
set(_owner_header "${SOURCE_DIR}/src/detail/linux_dbus_application_transport_owner.hpp")
set(_platform_state_header "${SOURCE_DIR}/src/detail/application_platform_state.hpp")
set(_application_backend_source "${SOURCE_DIR}/src/linux_application_backend.cpp")
set(_client_operations_source "${SOURCE_DIR}/src/linux_dbus_client_operations.cpp")
set(_platform_source "${SOURCE_DIR}/src/pugl_skia.cpp")
set(_window_header "${SOURCE_DIR}/include/nativeui/window.hpp")
set(_source "${SOURCE_DIR}/src/linux_dbus.cpp")
set(_codec_source "${SOURCE_DIR}/src/linux_dbus_codec.cpp")
set(_linux_build_doc "${SOURCE_DIR}/docs/linux-build.md")
set(_t072_test_cmake "${SOURCE_DIR}/tests/t072/CMakeLists.txt")
set(_explicit_address_lifecycle_test
  "${SOURCE_DIR}/tests/t072/explicit_address_lifecycle_contract.cpp")
set(_bus_address_test "${SOURCE_DIR}/tests/t072/bus_address_contract.cpp")
set(_accessibility_discovery_test
  "${SOURCE_DIR}/tests/t072/accessibility_bus_discovery_contract.cpp")
set(_accessibility_unavailable_test
  "${SOURCE_DIR}/tests/t072/accessibility_bus_unavailable_contract.cpp")

foreach(_required IN ITEMS
    "${_module}"
    "${_attach_module}"
    "${_header}"
    "${_client_operations_header}"
    "${_owner_header}"
    "${_platform_state_header}"
    "${_application_backend_source}"
    "${_client_operations_source}"
    "${_platform_source}"
    "${_window_header}"
    "${_source}"
    "${_codec_source}"
    "${_linux_build_doc}"
    "${_t072_test_cmake}"
    "${_explicit_address_lifecycle_test}"
    "${_bus_address_test}"
    "${_accessibility_discovery_test}"
    "${_accessibility_unavailable_test}")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "T072 RED: missing required internal Linux D-Bus transport file: ${_required}")
  endif()
endforeach()

file(READ "${SOURCE_DIR}/CMakeLists.txt" _root_cmake)
file(READ "${_module}" _dbus_module)
file(READ "${_attach_module}" _attach_module_text)
file(READ "${_platform_state_header}" _platform_state_text)
file(READ "${_application_backend_source}" _application_backend_text)
file(READ "${_header}" _header_text)
file(READ "${_client_operations_header}" _client_operations_header_text)
file(READ "${_client_operations_source}" _client_operations_source_text)
file(READ "${_owner_header}" _owner_header_text)
file(READ "${_platform_source}" _platform_source_text)
file(READ "${_window_header}" _window_header_text)
file(READ "${_source}" _source_text)
file(READ "${_linux_build_doc}" _linux_build_doc_text)
file(READ "${_t072_test_cmake}" _t072_test_cmake_text)

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
    "src/linux_dbus_client_operations.cpp"
    "src/linux_application_backend.cpp"
    "src/detail/linux_dbus_client_operations.hpp"
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

# The typed client-facing layer is the only production seam for T064/T068. It
# must preserve ResourceLimit/InvalidArgument/Shutdown rather than reducing
# synchronous rejection to an invalid ID or boolean.
foreach(_needle IN ITEMS
    "LinuxDbusImmediateResult"
    "LinuxDbusErrorCode code"
    "LinuxDbusRequestStartResult"
    "LinuxDbusSubscriptionResult"
    "LinuxDbusObjectPathResult"
    "LinuxDbusClientOperations")
  string(FIND "${_client_operations_header_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 typed client operation contract missing token: ${_needle}")
  endif()
endforeach()
foreach(_needle IN ITEMS
    "LinuxDbusErrorCode::ResourceLimit"
    "LinuxDbusErrorCode::InvalidArgument"
    "LinuxDbusErrorCode::Shutdown"
    "kLinuxDbusMaxPendingCalls"
    "kLinuxDbusMaxSubscriptions"
    "kLinuxDbusMaxObjectPaths"
    "transport_.start(bus_address)")
  string(FIND "${_client_operations_source_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 typed immediate failure implementation missing token: ${_needle}")
  endif()
endforeach()

# T181 extends the frozen transport with an explicit-address start mode and a
# one-shot accessibility-bus discovery helper without changing the default
# session path, the limits or the error taxonomy.
foreach(_needle IN ITEMS
    "LinuxDbusBusAddressDiscovery"
    "linux_dbus_valid_bus_address"
    "start(std::string_view bus_address)"
    "discover_accessibility_bus_address"
    "session_transport")
  string(FIND "${_header_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T181 explicit-address transport header missing token: ${_needle}")
  endif()
endforeach()
foreach(_needle IN ITEMS
    "dbus_connection_open_private"
    "dbus_bus_register"
    "dbus_bus_get_private(DBUS_BUS_SESSION"
    "AT_SPI_BUS_ADDRESS"
    "org.a11y.Bus"
    "/org/a11y/bus"
    "GetAddress"
    "query_accessibility_bus_address")
  string(FIND "${_source_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T181 explicit-address transport implementation missing token: ${_needle}")
  endif()
endforeach()
foreach(_needle IN ITEMS
    "t072_add_test(t072_explicit_address_lifecycle_contract explicit_address_lifecycle_contract.cpp)"
    "t072_add_test(t072_bus_address_contract bus_address_contract.cpp)"
    "t072_add_test(t072_accessibility_bus_discovery_contract accessibility_bus_discovery_contract.cpp)"
    "t072_add_test(t072_accessibility_bus_unavailable_contract accessibility_bus_unavailable_contract.cpp)"
    "target_link_libraries(t072_accessibility_bus_discovery_contract PRIVATE PkgConfig::NATIVEUI_DBUS)")
  string(FIND "${_t072_test_cmake_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T181 accessibility contract test is not registered: ${_needle}")
  endif()
endforeach()

# The Linux build/validation documentation must state the concrete system
# prerequisite, the pkg-config module expected by package consumers and the
# T181 explicit-address/accessibility-bus runtime contract.
foreach(_needle IN ITEMS
    "libdbus-1"
    "libdbus-1-dev"
    "pkg-config"
    "dbus-1"
    "macOS and Windows"
    "AT_SPI_BUS_ADDRESS"
    "org.a11y.Bus"
    "GetAddress"
    "dbus_connection_open_private"
    "dbus_bus_register"
    "one-shot")
  string(FIND "${_linux_build_doc_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 Linux prerequisite documentation missing token: ${_needle}")
  endif()
endforeach()

# T072 permits the std::once_flag synchronization primitive as the sole
# intentional mutable process-wide initialization state. The initialization
# result itself must be immutable after one-time initialization, not a second
# mutable namespace-scope variable. The libdbus initialization call itself must
# have exactly one production call site and be guarded by std::call_once.
string(FIND "${_source_text}" "g_dbus_threads_initialized" _mutable_init_result)
if(NOT _mutable_init_result EQUAL -1)
  message(FATAL_ERROR
    "T072 process-wide libdbus initialization result must not use a mutable global")
endif()
string(FIND "${_source_text}" "std::call_once(g_dbus_threads_once" _call_once_pos)
if(_call_once_pos EQUAL -1)
  message(FATAL_ERROR "T072 libdbus thread initialization must use std::call_once")
endif()
string(REGEX MATCHALL "dbus_threads_init_default\\(\\)" _thread_init_calls "${_source_text}")
list(LENGTH _thread_init_calls _thread_init_call_count)
if(NOT _thread_init_call_count EQUAL 1)
  message(FATAL_ERROR
    "T072 must contain exactly one dbus_threads_init_default() production call site; found ${_thread_init_call_count}")
endif()

# Every function pointer handed to libdbus is a foreign-C ABI boundary. These
# thunks must be explicitly noexcept so a future allocation/error-path change
# cannot silently permit a C++ exception to cross libdbus. Production code must
# additionally catch any throwing work inside the non-trivial notify/filter
# thunks before returning to C.
foreach(_needle IN ITEMS
    "static void free_notify_context(void* data) noexcept"
    "static void pending_notify(DBusPendingCall* pending, void* data) noexcept"
    "static DBusHandlerResult signal_filter(DBusConnection*, DBusMessage* message, void* data) noexcept"
    "static void object_path_unregistered(DBusConnection*, void* data) noexcept")
  string(FIND "${_source_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 libdbus callback must be an explicit noexcept boundary: ${_needle}")
  endif()
endforeach()
string(REGEX MATCH
  "static DBusHandlerResult object_path_message\\([^)]*void\\* data\\) noexcept"
  _object_path_noexcept "${_source_text}")
if(NOT _object_path_noexcept)
  message(FATAL_ERROR
    "T072 libdbus object-path callback must be an explicit noexcept boundary")
endif()

# Installed/build-tree Linux consumers must invoke the same private transport
# module after their platform target exists. macOS/Windows/WebAssembly remain
# outside the Linux branch and therefore never discover libdbus.
foreach(_needle IN ITEMS
    "if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)"
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
    "ApplicationBackendAccess"
    "LinuxDbusClientOperations* linux_dbus_operations_if_started")
  string(FIND "${_platform_state_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 source-private Application state missing token: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "std::unique_ptr<LinuxDbusClientOperations> operations_"
    "operations_if_started()"
    "operations_->register_client()")
  string(FIND "${_owner_header_text}" "${_needle}" _found)
  if(_found EQUAL -1)
    message(FATAL_ERROR "T072 Application transport owner missing typed operation seam: ${_needle}")
  endif()
endforeach()

foreach(_needle IN ITEMS
    "ApplicationBackendAccess::register_linux_dbus_client"
    "ApplicationBackendAccess::release_linux_dbus_client"
    "ApplicationBackendAccess::linux_dbus_operations_if_started"
    "ApplicationBackendAccess::linux_dbus_transport_if_started"
    "application.platform_state_->linux_dbus_transport.register_client()"
    "application.platform_state_->linux_dbus_transport.release_client(client)"
    "application.platform_state_->linux_dbus_transport.operations_if_started()"
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
      "LinuxDbusClientOperations"
      "LinuxDbusClientId"
      "LinuxDbusRequestId"
      "LinuxDbusValue"
      "LinuxDbusBusAddressDiscovery"
      "linux_dbus_valid_bus_address")
    string(FIND "${_public_text}" "${_forbidden}" _transport_leak)
    if(NOT _transport_leak EQUAL -1)
      message(FATAL_ERROR "T072 public API leak: ${_public_header} exposes ${_forbidden}")
    endif()
  endforeach()
endforeach()

message(STATUS "T072 Linux D-Bus build/Application ownership contract satisfied")
