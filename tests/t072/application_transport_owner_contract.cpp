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
    if (owner.operations_if_started() != nullptr || owner.transport_if_started() != nullptr ||
        owner.client_count() != 0) {
        return EXIT_FAILURE;
    }

    const auto client_a = owner.register_client();
    auto* const operations = owner.operations_if_started();
    auto* const transport = owner.transport_if_started();
    if (client_a == kInvalidLinuxDbusClientId || operations == nullptr || transport == nullptr ||
        &operations->transport_for_testing() != transport || !operations->running() ||
        transport->client_count() != 1 || owner.client_count() != 1) {
        return EXIT_FAILURE;
    }
    const std::string first_unique_name = operations->unique_name();
    if (first_unique_name.empty()) {
        return EXIT_FAILURE;
    }

    const auto client_b = owner.register_client();
    if (client_b == kInvalidLinuxDbusClientId || client_b == client_a ||
        owner.operations_if_started() != operations || owner.transport_if_started() != transport ||
        owner.client_count() != 2 || operations->client_count() != 2 ||
        operations->unique_name() != first_unique_name) {
        return EXIT_FAILURE;
    }

    // Application clients must observe the typed transport-wide quota instead
    // of silently creating an overflow connection or collapsing the failure to
    // an invalid ID with no ResourceLimit reason.
    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto result = operations->register_object_path(
            client_a,
            "/org/nativeui/T072/ApplicationCapacity/Path" + std::to_string(i),
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            });
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
    }
    const auto overflow = operations->register_object_path(
        client_b,
        "/org/nativeui/T072/ApplicationCapacity/Overflow",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (operations->object_path_count() != kLinuxDbusMaxObjectPaths ||
        overflow.code != LinuxDbusErrorCode::ResourceLimit ||
        overflow.id != kInvalidLinuxDbusObjectRegistrationId ||
        owner.operations_if_started() != operations || owner.transport_if_started() != transport ||
        operations->unique_name() != first_unique_name || owner.client_count() != 2) {
        return EXIT_FAILURE;
    }

    owner.release_client(client_a);
    owner.release_client(client_a);
    if (owner.operations_if_started() != operations || owner.transport_if_started() != transport ||
        !operations->running() || owner.client_count() != 1 || operations->client_count() != 1 ||
        operations->object_path_count() != 0 || operations->unique_name() != first_unique_name) {
        return EXIT_FAILURE;
    }

    LinuxDbusApplicationTransportOwner independent_owner;
    const auto independent_client = independent_owner.register_client();
    auto* const independent_operations = independent_owner.operations_if_started();
    auto* const independent_transport = independent_owner.transport_if_started();
    if (independent_client == kInvalidLinuxDbusClientId || independent_operations == nullptr ||
        independent_transport == nullptr || independent_operations == operations ||
        independent_transport == transport || !independent_operations->running() ||
        independent_operations->unique_name().empty() ||
        independent_operations->unique_name() == first_unique_name) {
        return EXIT_FAILURE;
    }

    const auto slow_path = independent_operations->register_object_path(
        independent_client,
        "/org/nativeui/T072/ApplicationShutdown/Slow",
        [](const LinuxDbusMethodRequest&) {
            std::this_thread::sleep_for(100ms);
            return LinuxDbusMethodReply::method_return({LinuxDbusValue::string("late")});
        });
    if (!slow_path.ok()) {
        return EXIT_FAILURE;
    }

    DispatcherOwner dispatcher_owner;
    std::size_t late_callbacks = 0;
    const auto request = operations->call_method(
        client_b,
        dispatcher_owner.dispatcher(),
        LinuxDbusMethodCall{
            independent_operations->unique_name(),
            "/org/nativeui/T072/ApplicationShutdown/Slow",
            "org.nativeui.T072.Test",
            "Wait",
            2s,
        },
        [&](LinuxDbusCompletion) { ++late_callbacks; });
    if (!request.ok()) {
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
    if (late_callbacks != 0 || owner.operations_if_started() != nullptr ||
        owner.transport_if_started() != nullptr || owner.client_count() != 0 ||
        owner.register_client() != kInvalidLinuxDbusClientId ||
        !independent_operations->running()) {
        return EXIT_FAILURE;
    }

    if (!independent_operations->unregister_object_path(independent_client, slow_path.id)) {
        return EXIT_FAILURE;
    }
    independent_owner.release_client(independent_client);
    independent_owner.shutdown();
    return EXIT_SUCCESS;
}
