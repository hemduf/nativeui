#include "detail/linux_dbus_client_operations.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    LinuxDbusClientOperations operations;
    const auto before_start = operations.register_client();
    if (before_start.code != LinuxDbusErrorCode::Shutdown ||
        before_start.id != kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }
    if (operations.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    const auto registration = operations.register_client();
    if (!registration.ok() || registration.code != LinuxDbusErrorCode::None ||
        operations.client_count() != 1) {
        return EXIT_FAILURE;
    }
    const auto client = registration.id;

    DispatcherOwner dispatcher_owner;
    LinuxDbusMethodCall invalid_call{
        "not a bus name",
        "/org/nativeui/T072/TypedResult",
        "org.nativeui.T072.TypedResult",
        "Ping",
        2s,
    };
    const auto invalid_request = operations.call_method(
        client, dispatcher_owner.dispatcher(), invalid_call,
        [](LinuxDbusCompletion) {});
    if (invalid_request.code != LinuxDbusErrorCode::InvalidArgument ||
        invalid_request.id != kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    LinuxDbusSignalMatch invalid_match;
    invalid_match.path = "relative/path";
    invalid_match.interface = "org.nativeui.T072.TypedResult";
    invalid_match.member = "Changed";
    const auto invalid_subscription = operations.subscribe_signal(
        client, dispatcher_owner.dispatcher(), invalid_match,
        [](LinuxDbusSignal) {});
    if (invalid_subscription.code != LinuxDbusErrorCode::InvalidArgument ||
        invalid_subscription.id != kInvalidLinuxDbusSubscriptionId) {
        return EXIT_FAILURE;
    }

    const auto invalid_path = operations.register_object_path(
        client, "relative/path", [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (invalid_path.code != LinuxDbusErrorCode::InvalidArgument ||
        invalid_path.id != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    const auto duplicate_seed = operations.register_object_path(
        client, "/org/nativeui/T072/TypedResult/Duplicate",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (!duplicate_seed.ok()) {
        return EXIT_FAILURE;
    }
    const auto duplicate = operations.register_object_path(
        client, "/org/nativeui/T072/TypedResult/Duplicate",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (duplicate.code != LinuxDbusErrorCode::InvalidArgument ||
        duplicate.id != kInvalidLinuxDbusObjectRegistrationId ||
        !operations.unregister_object_path(client, duplicate_seed.id)) {
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusObjectRegistrationId> object_paths;
    object_paths.reserve(kLinuxDbusMaxObjectPaths);
    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto result = operations.register_object_path(
            client,
            "/org/nativeui/T072/TypedResult/Path" + std::to_string(i),
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            });
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
        object_paths.push_back(result.id);
    }
    const auto object_overflow = operations.register_object_path(
        client, "/org/nativeui/T072/TypedResult/Overflow",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (object_overflow.code != LinuxDbusErrorCode::ResourceLimit ||
        object_overflow.id != kInvalidLinuxDbusObjectRegistrationId ||
        operations.object_path_count() != kLinuxDbusMaxObjectPaths) {
        return EXIT_FAILURE;
    }
    for (const auto id : object_paths) {
        if (!operations.unregister_object_path(client, id)) {
            return EXIT_FAILURE;
        }
    }

    LinuxDbusSignalMatch capacity_match;
    capacity_match.interface = "org.nativeui.T072.TypedResult";
    capacity_match.member = "NeverEmitted";
    std::vector<LinuxDbusSubscriptionId> subscriptions;
    subscriptions.reserve(kLinuxDbusMaxSubscriptions);
    for (std::size_t i = 0; i < kLinuxDbusMaxSubscriptions; ++i) {
        const auto result = operations.subscribe_signal(
            client, dispatcher_owner.dispatcher(), capacity_match,
            [](LinuxDbusSignal) {});
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
        subscriptions.push_back(result.id);
    }
    const auto subscription_overflow = operations.subscribe_signal(
        client, dispatcher_owner.dispatcher(), capacity_match,
        [](LinuxDbusSignal) {});
    if (subscription_overflow.code != LinuxDbusErrorCode::ResourceLimit ||
        subscription_overflow.id != kInvalidLinuxDbusSubscriptionId ||
        operations.subscription_count() != kLinuxDbusMaxSubscriptions) {
        return EXIT_FAILURE;
    }
    for (const auto id : subscriptions) {
        if (!operations.unsubscribe_signal(client, id)) {
            return EXIT_FAILURE;
        }
    }

    if (operations.send_signal(client, "relative/path",
                               "org.nativeui.T072.TypedResult", "Changed", {}) !=
            LinuxDbusErrorCode::InvalidArgument ||
        operations.send_signal(client, "/org/nativeui/T072/TypedResult",
                               "org.nativeui.T072.TypedResult", "Changed", {}) !=
            LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    // Hold one peer method handler open so all 1,024 caller slots can be filled
    // deterministically before any reply releases request capacity.
    LinuxDbusClientOperations peer;
    if (peer.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }
    const auto peer_registration = peer.register_client();
    if (!peer_registration.ok()) {
        return EXIT_FAILURE;
    }
    std::atomic<bool> release_peer{false};
    const auto peer_path = peer.register_object_path(
        peer_registration.id,
        "/org/nativeui/T072/TypedResult/Blocked",
        [&](const LinuxDbusMethodRequest&) {
            while (!release_peer.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(1ms);
            }
            return LinuxDbusMethodReply::method_return({});
        });
    if (!peer_path.ok()) {
        return EXIT_FAILURE;
    }

    LinuxDbusMethodCall blocked_call{
        peer.unique_name(),
        "/org/nativeui/T072/TypedResult/Blocked",
        "org.nativeui.T072.TypedResult",
        "Wait",
        kLinuxDbusMaxTimeout,
    };
    std::vector<LinuxDbusRequestId> requests;
    requests.reserve(kLinuxDbusMaxPendingCalls);
    for (std::size_t i = 0; i < kLinuxDbusMaxPendingCalls; ++i) {
        const auto result = operations.call_method(
            client, dispatcher_owner.dispatcher(), blocked_call,
            [](LinuxDbusCompletion) {});
        if (!result.ok()) {
            release_peer.store(true, std::memory_order_release);
            return EXIT_FAILURE;
        }
        requests.push_back(result.id);
    }
    const auto request_overflow = operations.call_method(
        client, dispatcher_owner.dispatcher(), blocked_call,
        [](LinuxDbusCompletion) {});
    if (request_overflow.code != LinuxDbusErrorCode::ResourceLimit ||
        request_overflow.id != kInvalidLinuxDbusRequestId ||
        operations.pending_request_count() != kLinuxDbusMaxPendingCalls) {
        release_peer.store(true, std::memory_order_release);
        return EXIT_FAILURE;
    }

    if (!operations.release_client(client) || operations.client_count() != 0 ||
        operations.pending_request_count() != 0) {
        release_peer.store(true, std::memory_order_release);
        return EXIT_FAILURE;
    }
    release_peer.store(true, std::memory_order_release);
    if (!peer.unregister_object_path(peer_registration.id, peer_path.id) ||
        !peer.release_client(peer_registration.id)) {
        return EXIT_FAILURE;
    }
    peer.stop();

    const auto stale_client_call = operations.call_method(
        client, dispatcher_owner.dispatcher(), blocked_call,
        [](LinuxDbusCompletion) {});
    if (stale_client_call.code != LinuxDbusErrorCode::InvalidArgument ||
        stale_client_call.id != kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    operations.stop();
    const auto after_stop = operations.register_client();
    if (after_stop.code != LinuxDbusErrorCode::Shutdown ||
        after_stop.id != kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
