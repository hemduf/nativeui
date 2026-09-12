#pragma once

#include "linux_dbus_client_operations.hpp"

#include <nativeui/desktop_services.hpp>

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

/// Protocol-level factory used by deterministic tests and the production T072
/// adapter. The namespace ID must be one non-zero transport-local T072 client
/// ID so request handle tokens stay unique across windows sharing an Application.
[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_linux_portal_desktop_services_backend(std::shared_ptr<LinuxPortalBus> bus,
                                           Dispatcher dispatcher,
                                           LinuxDbusClientId token_namespace);

#if defined(__linux__)
/// Production standalone factory. It registers exactly one logical client on
/// the Application-owned T072 transport and releases it with backend lifetime.
[[nodiscard]] std::shared_ptr<DesktopServicesBackend>
make_linux_desktop_services_backend(Application& application,
                                    Dispatcher dispatcher);
#endif

} // namespace ui::detail
