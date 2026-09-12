#pragma once

#include "linux_dbus_client_operations.hpp"

#include <nativeui/desktop_services.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>

namespace ui {
class Application;
}

namespace ui::detail {

/// Narrow protocol seam between the Linux portal backend and T072. Production
/// uses exactly one Application-owned T072 client; tests may inject a fake bus
/// without starting a second D-Bus stack.
class LinuxPortalBus {
public:
    virtual ~LinuxPortalBus() = default;

    [[nodiscard]] virtual std::string unique_name() const = 0;
    [[nodiscard]] virtual LinuxDbusSubscriptionResult subscribe_signal(
        Dispatcher dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback) = 0;
    [[nodiscard]] virtual bool unsubscribe_signal(LinuxDbusSubscriptionId id) = 0;
    [[nodiscard]] virtual LinuxDbusRequestStartResult call_method(
        Dispatcher dispatcher,
        const LinuxDbusMethodCall& call,
        LinuxDbusCompletionCallback callback) = 0;
    [[nodiscard]] virtual bool cancel_request(LinuxDbusRequestId id) = 0;
};

/// XDG Desktop Portal X11 parent identifier. Zero means that no suitable
/// native owner is available and therefore maps to the protocol's empty parent.
[[nodiscard]] inline std::string linux_x11_portal_parent_window(
    std::uintptr_t native_window) {
    if (native_window == 0) return {};

    std::array<char, sizeof(std::uintptr_t) * 2> digits{};
    const auto converted = std::to_chars(
        digits.data(), digits.data() + digits.size(), native_window, 16);
    if (converted.ec != std::errc{}) return {};

    std::string parent{"x11:"};
    parent.append(digits.data(), converted.ptr);
    return parent;
}

/// Protocol-level factory used by deterministic tests and the production T072
/// adapter. The namespace ID must be one non-zero transport-local T072 client
/// ID so request handle tokens stay unique across windows sharing an Application.
[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_linux_portal_desktop_services_backend(std::shared_ptr<LinuxPortalBus> bus,
                                           Dispatcher dispatcher,
                                           LinuxDbusClientId token_namespace,
                                           std::string parent_window);

#if defined(__linux__)
/// Production standalone factory. It registers exactly one logical client on
/// the Application-owned T072 transport and releases it with backend lifetime.
/// `native_window` is the requesting X11 window XID and is used only to derive
/// the portal parent identifier; transport ownership remains Application-scoped.
[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_linux_desktop_services_backend(Application& application,
                                    Dispatcher dispatcher,
                                    std::uintptr_t native_window);
#endif

} // namespace ui::detail
