#include "detail/linux_dbus.hpp"

#include <cstdlib>
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

    return EXIT_SUCCESS;
}
