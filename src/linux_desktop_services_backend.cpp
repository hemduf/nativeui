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

    std::shared_ptr<T072LinuxPortalBus> bus;
    try {
        bus = std::make_shared<T072LinuxPortalBus>(application, *operations, client);
    } catch (...) {
        ApplicationBackendAccess::release_linux_dbus_client(application, client);
        return {};
    }

    // From this point the bus owns the T072 client lifetime. If the portal
    // backend cannot be created, destroying `bus` releases the client exactly
    // once; no second cleanup path is required here.
    return make_linux_portal_desktop_services_backend(
        std::move(bus), std::move(dispatcher), client, std::move(parent_window));
}

} // namespace ui::detail

#endif
