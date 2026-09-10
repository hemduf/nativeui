#include "detail/application_platform_state.hpp"

#include <nativeui/window.hpp>

#include <memory>

namespace ui::detail {

LinuxDbusClientId ApplicationBackendAccess::register_linux_dbus_client(
    Application& application) {
    if (!application.valid()) {
        return kInvalidLinuxDbusClientId;
    }

    try {
        if (!application.platform_state_) {
            application.platform_state_ = std::make_unique<ApplicationPlatformState>();
        }
        return application.platform_state_->linux_dbus_transport.register_client();
    } catch (...) {
        return kInvalidLinuxDbusClientId;
    }
}

void ApplicationBackendAccess::release_linux_dbus_client(
    Application& application,
    LinuxDbusClientId client) noexcept {
    if (!application.platform_state_ || client == kInvalidLinuxDbusClientId) {
        return;
    }
    application.platform_state_->linux_dbus_transport.release_client(client);
}

LinuxDbusTransport* ApplicationBackendAccess::linux_dbus_transport_if_started(
    Application& application) noexcept {
    if (!application.platform_state_) {
        return nullptr;
    }
    return application.platform_state_->linux_dbus_transport.transport_if_started();
}

} // namespace ui::detail
