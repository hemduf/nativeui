#include "detail/linux_desktop_services.hpp"

#if defined(__linux__)

#include "detail/application_platform_state.hpp"

#include <utility>

namespace ui::detail {
namespace {

class T072LinuxPortalBus final : public LinuxPortalBus {
public:
    T072LinuxPortalBus(Application& application,
                       LinuxDbusClientOperations& operations,
                       LinuxDbusClientId client)
        : application_(&application), operations_(&operations), client_(client) {}

    ~T072LinuxPortalBus() override {
        if (application_ && client_ != kInvalidLinuxDbusClientId) {
            ApplicationBackendAccess::release_linux_dbus_client(*application_, client_);
        }
    }

    [[nodiscard]] std::string unique_name() const override {
        return operations_ ? operations_->unique_name() : std::string{};
    }

    [[nodiscard]] LinuxDbusSubscriptionResult subscribe_signal(
        Dispatcher dispatcher,
        const LinuxDbusSignalMatch& match,
        LinuxDbusSignalCallback callback) override {
        if (!operations_) return {LinuxDbusErrorCode::Shutdown, 0};
        return operations_->subscribe_signal(
            client_, std::move(dispatcher), match, std::move(callback));
    }

    [[nodiscard]] bool unsubscribe_signal(LinuxDbusSubscriptionId id) override {
        return operations_ && operations_->unsubscribe_signal(client_, id);
    }

    [[nodiscard]] LinuxDbusRequestStartResult call_method(
        Dispatcher dispatcher,
        const LinuxDbusMethodCall& call,
        LinuxDbusCompletionCallback callback) override {
        if (!operations_) return {LinuxDbusErrorCode::Shutdown, 0};
        return operations_->call_method(
            client_, std::move(dispatcher), call, std::move(callback));
    }

    [[nodiscard]] bool cancel_request(LinuxDbusRequestId id) override {
        return operations_ && operations_->cancel_request(client_, id);
    }

private:
    Application* application_{};
    LinuxDbusClientOperations* operations_{};
    LinuxDbusClientId client_{kInvalidLinuxDbusClientId};
};

} // namespace

std::shared_ptr<DesktopServicesBackend> make_linux_desktop_services_backend(
    Application& application,
    Dispatcher dispatcher,
    std::uintptr_t native_window) {
    std::string parent_window;
    try {
        parent_window = linux_x11_portal_parent_window(native_window);
    } catch (...) {
        return {};
    }

    const auto client = ApplicationBackendAccess::register_linux_dbus_client(application);
    if (client == kInvalidLinuxDbusClientId) return {};

    auto* operations = ApplicationBackendAccess::linux_dbus_operations_if_started(application);
    if (!operations) {
        ApplicationBackendAccess::release_linux_dbus_client(application, client);
        return {};
    }

    std::shared_ptr<T072LinuxPortalBus> transport_bus;
    try {
        transport_bus =
            std::make_shared<T072LinuxPortalBus>(application, *operations, client);
    } catch (...) {
        // No RAII owner exists yet.
        ApplicationBackendAccess::release_linux_dbus_client(application, client);
        return {};
    }

    std::shared_ptr<LinuxPortalBus> portal_bus;
    try {
        portal_bus = std::make_shared<LinuxPortalParentBus>(
            transport_bus, std::move(parent_window));
    } catch (...) {
        // transport_bus is now the sole client owner and releases exactly once.
        return {};
    }

    return make_linux_portal_desktop_services_backend(
        std::move(portal_bus), std::move(dispatcher), client);
}

} // namespace ui::detail

#endif
