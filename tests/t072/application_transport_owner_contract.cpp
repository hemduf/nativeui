#include "detail/linux_dbus_application_transport_owner.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    LinuxDbusApplicationTransportOwner owner;
    if (owner.transport_if_started() != nullptr || owner.client_count() != 0) {
        return EXIT_FAILURE;
    }

    const auto client_a = owner.register_client();
    auto* const transport = owner.transport_if_started();
    if (client_a == kInvalidLinuxDbusClientId || transport == nullptr ||
        !transport->running() || transport->client_count() != 1 ||
        owner.client_count() != 1) {
        return EXIT_FAILURE;
    }
    const std::string first_unique_name = transport->unique_name();
    if (first_unique_name.empty()) {
        return EXIT_FAILURE;
    }

    const auto client_b = owner.register_client();
    if (client_b == kInvalidLinuxDbusClientId || client_b == client_a ||
        owner.transport_if_started() != transport || owner.client_count() != 2 ||
        transport->client_count() != 2 || transport->unique_name() != first_unique_name) {
        return EXIT_FAILURE;
    }

    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto id = transport->register_object_path(
            client_a,
            "/org/nativeui/T072/ApplicationCapacity/Path" + std::to_string(i),
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            });
        if (id == kInvalidLinuxDbusObjectRegistrationId) {
            return EXIT_FAILURE;
        }
    }
    if (transport->object_path_count() != kLinuxDbusMaxObjectPaths ||
        transport->register_object_path(
            client_b,
            "/org/nativeui/T072/ApplicationCapacity/Overflow",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId ||
        owner.transport_if_started() != transport || transport->unique_name() != first_unique_name ||
        owner.client_count() != 2) {
        return EXIT_FAILURE;
    }

    owner.release_client(client_a);
    owner.release_client(client_a);
    if (owner.transport_if_started() != transport || !transport->running() ||
        owner.client_count() != 1 || transport->client_count() != 1 ||
        transport->object_path_count() != 0 || transport->unique_name() != first_unique_name) {
        return EXIT_FAILURE;
    }

    LinuxDbusApplicationTransportOwner independent_owner;
    const auto independent_client = independent_owner.register_client();
    auto* const independent_transport = independent_owner.transport_if_started();
    if (independent_client == kInvalidLinuxDbusClientId || independent_transport == nullptr ||
        independent_transport == transport || !independent_transport->running() ||
        independent_transport->unique_name().empty() ||
        independent_transport->unique_name() == first_unique_name) {
        return EXIT_FAILURE;
    }

    const auto slow_path = independent_transport->register_object_path(
        independent_client,
        "/org/nativeui/T072/ApplicationShutdown/Slow",
        [](const LinuxDbusMethodRequest&) {
            std::this_thread::sleep_for(100ms);
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("late")});
        });
    if (slow_path == kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    DispatcherOwner dispatcher_owner;
    std::size_t late_callbacks = 0;
    const auto request = transport->call_method(
        client_b,
        dispatcher_owner.dispatcher(),
        LinuxDbusMethodCall{
            independent_transport->unique_name(),
            "/org/nativeui/T072/ApplicationShutdown/Slow",
            "org.nativeui.T072.Test",
            "Wait",
            2s,
        },
        [&](LinuxDbusCompletion) { ++late_callbacks; });
    if (request == kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    // Application shutdown must tear logical clients down before transport
    // stop/join. A pending request therefore cannot post a Shutdown completion
    // into an owner that is already closing.
    owner.shutdown();
    owner.shutdown();
    (void)dispatcher_owner.checkpoint();
    std::this_thread::sleep_for(150ms);
    (void)dispatcher_owner.checkpoint();
    if (late_callbacks != 0 || owner.transport_if_started() != nullptr ||
        owner.client_count() != 0 ||
        owner.register_client() != kInvalidLinuxDbusClientId ||
        !independent_transport->running()) {
        return EXIT_FAILURE;
    }

    if (!independent_transport->unregister_object_path(independent_client, slow_path)) {
        return EXIT_FAILURE;
    }
    independent_owner.release_client(independent_client);
    independent_owner.shutdown();
    return EXIT_SUCCESS;
}
