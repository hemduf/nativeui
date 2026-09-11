#include "detail/linux_dbus_client_operations.hpp"

#include <cstdlib>

int main() {
    using namespace ui::detail;

    // T072 does not enable accessibility itself (T068 does), but it must make
    // the normative embedded ownership shape possible without a process-global
    // transport: each accessibility-enabled EmbeddedView can own one independent
    // client-operation surface, and therefore one independent private transport.
    LinuxDbusClientOperations embedded_a;
    LinuxDbusClientOperations embedded_b;
    if (embedded_a.start() != LinuxDbusErrorCode::None ||
        embedded_b.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    const auto client_a = embedded_a.register_client();
    const auto client_b = embedded_b.register_client();
    if (!client_a.ok() || !client_b.ok() ||
        embedded_a.unique_name().empty() || embedded_b.unique_name().empty() ||
        embedded_a.unique_name() == embedded_b.unique_name() ||
        &embedded_a.transport_for_testing() == &embedded_b.transport_for_testing()) {
        return EXIT_FAILURE;
    }

    // Equal object paths are valid across independent EmbeddedView transports;
    // collision scope is transport-local, not process-global.
    const auto path_a = embedded_a.register_object_path(
        client_a.id,
        "/org/nativeui/T072/Embedded/Root",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    const auto path_b = embedded_b.register_object_path(
        client_b.id,
        "/org/nativeui/T072/Embedded/Root",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::method_return({});
        });
    if (!path_a.ok() || !path_b.ok() ||
        embedded_a.object_path_count() != 1 || embedded_b.object_path_count() != 1) {
        return EXIT_FAILURE;
    }

    if (!embedded_a.release_client(client_a.id)) {
        return EXIT_FAILURE;
    }
    embedded_a.stop();
    if (embedded_a.running() || !embedded_b.running() || embedded_b.client_count() != 1 ||
        embedded_b.object_path_count() != 1) {
        return EXIT_FAILURE;
    }

    if (!embedded_b.unregister_object_path(client_b.id, path_b.id) ||
        !embedded_b.release_client(client_b.id)) {
        return EXIT_FAILURE;
    }
    embedded_b.stop();
    return EXIT_SUCCESS;
}
