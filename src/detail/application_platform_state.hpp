#pragma once

#if defined(__linux__)
#  include "linux_dbus_application_transport_owner.hpp"
#endif

namespace ui {

class Application;

namespace detail {

/// Source-private platform state whose lifetime is tied to one Application.
///
/// Application declares this member after its generic backend Impl so member
/// destruction shuts platform services down before the Pugl PROGRAM world is
/// released. Non-Linux builds intentionally keep this state empty.
struct ApplicationPlatformState final {
#if defined(__linux__)
    LinuxDbusApplicationTransportOwner linux_dbus_transport;
#endif
};

/// Source-private access used by Linux platform services such as T064/T068.
/// No D-Bus type is exposed through NativeUI's public headers.
struct ApplicationBackendAccess final {
#if defined(__linux__)
    [[nodiscard]] static LinuxDbusClientId register_linux_dbus_client(Application& application);
    static void release_linux_dbus_client(Application& application,
                                          LinuxDbusClientId client) noexcept;
    [[nodiscard]] static LinuxDbusTransport* linux_dbus_transport_if_started(
        Application& application) noexcept;
#endif
};

} // namespace detail
} // namespace ui
