#include "detail/linux_dbus.hpp"
#include "detail/linux_dbus_client_operations.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

int main() {
    using namespace ui::detail;

    constexpr LinuxDbusClientId client_a = 1;
    constexpr LinuxDbusClientId client_b = 2;

    LinuxDbusResourceLedger ledger;
    std::vector<LinuxDbusRequestId> requests;
    requests.reserve(kLinuxDbusMaxPendingCalls);
    for (std::size_t i = 0; i < kLinuxDbusMaxPendingCalls; ++i) {
        const auto id = ledger.acquire_request(client_a);
        if (id == kInvalidLinuxDbusRequestId) {
            return EXIT_FAILURE;
        }
        requests.push_back(id);
    }
    if (ledger.pending_request_count() != kLinuxDbusMaxPendingCalls ||
        ledger.acquire_request(client_b) != kInvalidLinuxDbusRequestId ||
        ledger.release_request(client_b, requests.front()) ||
        !ledger.release_request(client_a, requests.front()) ||
        ledger.pending_request_count() != kLinuxDbusMaxPendingCalls - 1 ||
        ledger.acquire_request(client_b) == kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusSubscriptionId> subscriptions;
    subscriptions.reserve(kLinuxDbusMaxSubscriptions);
    for (std::size_t i = 0; i < kLinuxDbusMaxSubscriptions; ++i) {
        const auto id = ledger.acquire_subscription(client_a);
        if (id == kInvalidLinuxDbusSubscriptionId) {
            return EXIT_FAILURE;
        }
        subscriptions.push_back(id);
    }
    if (ledger.acquire_subscription(client_b) != kInvalidLinuxDbusSubscriptionId ||
        ledger.release_subscription(client_b, subscriptions.front()) ||
        !ledger.release_subscription(client_a, subscriptions.front()) ||
        ledger.acquire_subscription(client_b) == kInvalidLinuxDbusSubscriptionId) {
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusObjectRegistrationId> paths;
    paths.reserve(kLinuxDbusMaxObjectPaths);
    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto id = ledger.acquire_object_path(client_a);
        if (id == kInvalidLinuxDbusObjectRegistrationId) {
            return EXIT_FAILURE;
        }
        paths.push_back(id);
    }
    if (ledger.acquire_object_path(client_b) != kInvalidLinuxDbusObjectRegistrationId ||
        ledger.release_object_path(client_b, paths.front()) ||
        !ledger.release_object_path(client_a, paths.front()) ||
        ledger.acquire_object_path(client_b) == kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    LinuxDbusResourceLedger independent;
    if (independent.acquire_request(client_a) == kInvalidLinuxDbusRequestId ||
        independent.pending_request_count() != 1 ||
        ledger.pending_request_count() != kLinuxDbusMaxPendingCalls) {
        return EXIT_FAILURE;
    }

    // T181 extension: the typed explicit-address mode enforces exactly the same
    // frozen limits and timeout validation as the session path. The compile-time
    // constants themselves stay asserted in build_contract.cpp; the shared
    // ledger assertions above are unchanged.
    using namespace std::chrono_literals;

    const char* env_address = std::getenv("DBUS_SESSION_BUS_ADDRESS");
    if (env_address == nullptr || *env_address == '\0' ||
        !linux_dbus_valid_bus_address(env_address)) {
        return EXIT_FAILURE;
    }
    const std::string address{env_address};

    LinuxDbusClientOperations operations;
    if (operations.start(address) != LinuxDbusErrorCode::None || !operations.running()) {
        return EXIT_FAILURE;
    }
    DispatcherOwner owner;
    const auto registration = operations.register_client();
    if (!registration.ok()) {
        return EXIT_FAILURE;
    }
    const auto client = registration.id;

    LinuxDbusMethodCall get_id{
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "GetId",
        kLinuxDbusDefaultTimeout,
    };

    // Invalid timeouts are rejected before any request slot is acquired.
    const std::chrono::milliseconds invalid_timeouts[] = {0ms, 301s, 300s + 1ms};
    for (const auto invalid : invalid_timeouts) {
        get_id.timeout = invalid;
        const auto result = operations.call_method(
            client, owner.dispatcher(), get_id, [](LinuxDbusCompletion) {});
        if (result.code != LinuxDbusErrorCode::InvalidArgument ||
            result.id != kInvalidLinuxDbusRequestId ||
            operations.pending_request_count() != 0) {
            return EXIT_FAILURE;
        }
    }

    // The frozen minimum and maximum timeouts remain valid.
    const std::chrono::milliseconds valid_timeouts[] = {kLinuxDbusMinTimeout,
                                                       kLinuxDbusMaxTimeout};
    for (const auto valid : valid_timeouts) {
        get_id.timeout = valid;
        const auto result = operations.call_method(
            client, owner.dispatcher(), get_id, [](LinuxDbusCompletion) {});
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
        (void)operations.cancel_request(client, result.id);
    }
    for (int spins = 0; spins < 10'000 && operations.pending_request_count() != 0; ++spins) {
        (void)owner.checkpoint();
        std::this_thread::sleep_for(1ms);
    }
    if (operations.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    // Object-path cap identical to the session mode.
    std::vector<LinuxDbusObjectRegistrationId> object_paths;
    object_paths.reserve(kLinuxDbusMaxObjectPaths);
    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto result = operations.register_object_path(
            client, "/org/nativeui/T072/ExplicitLimits/Path" + std::to_string(i),
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            });
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
        object_paths.push_back(result.id);
    }
    const auto object_overflow = operations.register_object_path(
        client, "/org/nativeui/T072/ExplicitLimits/Overflow",
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
    if (operations.object_path_count() != 0) {
        return EXIT_FAILURE;
    }

    // Subscription cap identical to the session mode.
    LinuxDbusSignalMatch unlimited_match;
    unlimited_match.interface = "org.nativeui.T072.ExplicitLimits";
    unlimited_match.member = "NeverEmitted";
    std::vector<LinuxDbusSubscriptionId> explicit_subscriptions;
    explicit_subscriptions.reserve(kLinuxDbusMaxSubscriptions);
    for (std::size_t i = 0; i < kLinuxDbusMaxSubscriptions; ++i) {
        const auto result = operations.subscribe_signal(
            client, owner.dispatcher(), unlimited_match, [](LinuxDbusSignal) {});
        if (!result.ok()) {
            return EXIT_FAILURE;
        }
        explicit_subscriptions.push_back(result.id);
    }
    const auto subscription_overflow = operations.subscribe_signal(
        client, owner.dispatcher(), unlimited_match, [](LinuxDbusSignal) {});
    if (subscription_overflow.code != LinuxDbusErrorCode::ResourceLimit ||
        subscription_overflow.id != kInvalidLinuxDbusSubscriptionId ||
        operations.subscription_count() != kLinuxDbusMaxSubscriptions) {
        return EXIT_FAILURE;
    }
    for (const auto id : explicit_subscriptions) {
        if (!operations.unsubscribe_signal(client, id)) {
            return EXIT_FAILURE;
        }
    }
    if (operations.subscription_count() != 0) {
        return EXIT_FAILURE;
    }

    // Pending-call cap identical to the session mode, using the same blocked
    // peer-handler fault injection as immediate_result_contract.cpp.
    LinuxDbusClientOperations peer;
    if (peer.start(address) != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }
    const auto peer_registration = peer.register_client();
    if (!peer_registration.ok()) {
        return EXIT_FAILURE;
    }
    std::atomic<bool> release_peer{false};
    const auto peer_path = peer.register_object_path(
        peer_registration.id,
        "/org/nativeui/T072/ExplicitLimits/Blocked",
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
        "/org/nativeui/T072/ExplicitLimits/Blocked",
        "org.nativeui.T072.ExplicitLimits",
        "Wait",
        kLinuxDbusMaxTimeout,
    };
    std::vector<LinuxDbusRequestId> explicit_requests;
    explicit_requests.reserve(kLinuxDbusMaxPendingCalls);
    for (std::size_t i = 0; i < kLinuxDbusMaxPendingCalls; ++i) {
        const auto result = operations.call_method(
            client, owner.dispatcher(), blocked_call, [](LinuxDbusCompletion) {});
        if (!result.ok()) {
            release_peer.store(true, std::memory_order_release);
            return EXIT_FAILURE;
        }
        explicit_requests.push_back(result.id);
    }
    const auto request_overflow = operations.call_method(
        client, owner.dispatcher(), blocked_call, [](LinuxDbusCompletion) {});
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
    operations.stop();

    return EXIT_SUCCESS;
}
