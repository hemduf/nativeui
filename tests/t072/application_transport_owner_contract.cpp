#include "detail/linux_dbus_application_transport_owner.hpp"

#include <cstdlib>
#include <string>

int main() {
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

    owner.release_client(client_a);
    owner.release_client(client_a);
    if (owner.transport_if_started() != transport || !transport->running() ||
        owner.client_count() != 1 || transport->client_count() != 1) {
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

    owner.release_client(client_b);
    if (owner.client_count() != 0 || owner.transport_if_started() != transport ||
        !transport->running()) {
        return EXIT_FAILURE;
    }

    owner.shutdown();
    owner.shutdown();
    if (owner.transport_if_started() != nullptr || owner.client_count() != 0 ||
        owner.register_client() != kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }

    independent_owner.release_client(independent_client);
    independent_owner.shutdown();
    return EXIT_SUCCESS;
}
