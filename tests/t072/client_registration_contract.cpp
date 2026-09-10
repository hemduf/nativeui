#include "detail/linux_dbus.hpp"

#include <cstdlib>
#include <limits>

int main() {
    using namespace ui::detail;

    LinuxDbusTransport transport;
    if (transport.register_client() != kInvalidLinuxDbusClientId) {
        return EXIT_FAILURE;
    }
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    const auto client_a = transport.register_client();
    const auto client_b = transport.register_client();
    if (client_a == kInvalidLinuxDbusClientId ||
        client_b == kInvalidLinuxDbusClientId ||
        client_a == client_b ||
        transport.client_count() != 2) {
        return EXIT_FAILURE;
    }

    constexpr LinuxDbusClientId unregistered =
        std::numeric_limits<LinuxDbusClientId>::max();
    if (transport.register_object_path(
            unregistered,
            "/org/nativeui/T072/Unregistered",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    transport.release_client(client_a);
    transport.release_client(client_a);
    if (transport.client_count() != 1 ||
        transport.register_object_path(
            client_a,
            "/org/nativeui/T072/Released",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    const auto client_c = transport.register_client();
    if (client_c == kInvalidLinuxDbusClientId ||
        client_c == client_a || client_c == client_b ||
        transport.client_count() != 2) {
        return EXIT_FAILURE;
    }

    transport.release_client(client_b);
    transport.release_client(client_c);
    if (transport.client_count() != 0) {
        return EXIT_FAILURE;
    }

    transport.stop();
    return EXIT_SUCCESS;
}
