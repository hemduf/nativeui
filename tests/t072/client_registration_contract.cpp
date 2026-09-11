#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

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

    DispatcherOwner owner;
    constexpr LinuxDbusClientId unregistered =
        std::numeric_limits<LinuxDbusClientId>::max();
    const LinuxDbusSignalMatch match{
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus",
        "NameOwnerChanged",
    };

    bool unregistered_rejected = true;
    if (transport.call_method(
            unregistered,
            owner.dispatcher(),
            LinuxDbusMethodCall{
                "org.freedesktop.DBus",
                "/org/freedesktop/DBus",
                "org.freedesktop.DBus",
                "GetId",
            },
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId) {
        unregistered_rejected = false;
    }
    if (transport.subscribe_signal(
            unregistered, owner.dispatcher(), match,
            [](LinuxDbusSignal) {}) != kInvalidLinuxDbusSubscriptionId) {
        unregistered_rejected = false;
    }
    if (transport.register_object_path(
            unregistered,
            "/org/nativeui/T072/Unregistered",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId) {
        unregistered_rejected = false;
    }
    if (!unregistered_rejected) {
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

    transport.release_client(client_a);
    transport.release_client(client_a);

    bool released_rejected = true;
    if (transport.call_method(
            client_a,
            owner.dispatcher(),
            LinuxDbusMethodCall{
                "org.freedesktop.DBus",
                "/org/freedesktop/DBus",
                "org.freedesktop.DBus",
                "GetId",
            },
            [](LinuxDbusCompletion) {}) != kInvalidLinuxDbusRequestId) {
        released_rejected = false;
    }
    if (transport.subscribe_signal(
            client_a, owner.dispatcher(), match,
            [](LinuxDbusSignal) {}) != kInvalidLinuxDbusSubscriptionId) {
        released_rejected = false;
    }
    if (transport.register_object_path(
            client_a,
            "/org/nativeui/T072/Released",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId) {
        released_rejected = false;
    }
    if (!released_rejected || transport.client_count() != 1) {
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
