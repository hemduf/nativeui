#include "detail/linux_dbus.hpp"

#include <chrono>
#include <cstdlib>
#include <string>

namespace {

/// The test bus is the private bus provided by the harness (`dbus-run-session`
/// on CI, the local probe daemon during macOS development). Its address is a
/// real, private session bus address and is used as the explicit-address mode
/// target.
[[nodiscard]] const char* environment_bus_address() {
    const char* address = std::getenv("DBUS_SESSION_BUS_ADDRESS");
    if (address == nullptr || *address == '\0') {
        return nullptr;
    }
    return address;
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    const char* env_address = environment_bus_address();
    if (env_address == nullptr) {
        return EXIT_FAILURE;
    }
    const std::string address{env_address};
    if (!linux_dbus_valid_bus_address(address)) {
        return EXIT_FAILURE;
    }

    // Mirrors lifecycle_contract.cpp's bounded stop-time assertion: every
    // start/stop pair below must complete without waiting on idle timeouts.
    const auto begin = std::chrono::steady_clock::now();

    // Mirrors lifecycle_contract.cpp for the explicit-address mode: start,
    // idempotent double start, stop, restart, stop-then-switch-mode and two
    // independent transports all behave like the frozen session path.
    {
        LinuxDbusTransport transport;
        if (transport.start(address) != LinuxDbusErrorCode::None || !transport.running()) {
            return EXIT_FAILURE;
        }
        const std::string first_name = transport.unique_name();
        if (first_name.empty()) {
            return EXIT_FAILURE;
        }

        if (transport.start(address) != LinuxDbusErrorCode::None ||
            transport.unique_name() != first_name || !transport.running()) {
            return EXIT_FAILURE;
        }

        if (transport.register_client() == kInvalidLinuxDbusClientId ||
            transport.client_count() != 1) {
            return EXIT_FAILURE;
        }

        transport.stop();
        if (transport.running() || !transport.unique_name().empty() ||
            transport.client_count() != 0) {
            return EXIT_FAILURE;
        }
        transport.stop(); // idempotent after a successful start
        if (transport.running()) {
            return EXIT_FAILURE;
        }

        // Stop-then-start is a fresh start in the same explicit mode.
        if (transport.start(address) != LinuxDbusErrorCode::None ||
            !transport.running() || transport.unique_name().empty()) {
            return EXIT_FAILURE;
        }
        transport.stop();

        // The explicit mode does not latch: the same object can still start on
        // the default session path afterwards.
        if (transport.start() != LinuxDbusErrorCode::None || !transport.running() ||
            transport.unique_name().empty()) {
            return EXIT_FAILURE;
        }
        transport.stop();

        // And the explicit mode works again after the session mode.
        if (transport.start(address) != LinuxDbusErrorCode::None ||
            !transport.running()) {
            return EXIT_FAILURE;
        }
        transport.stop();
        if (transport.running()) {
            return EXIT_FAILURE;
        }
    }

    // Two independent explicit transports own independent private connections.
    {
        LinuxDbusTransport first;
        LinuxDbusTransport second;
        if (first.start(address) != LinuxDbusErrorCode::None ||
            second.start(address) != LinuxDbusErrorCode::None) {
            return EXIT_FAILURE;
        }

        const std::string first_name = first.unique_name();
        const std::string second_name = second.unique_name();
        if (first_name.empty() || second_name.empty() || first_name == second_name) {
            return EXIT_FAILURE;
        }

        first.stop();
        if (first.running() || !second.running()) {
            return EXIT_FAILURE;
        }
        second.stop();
    }

    if (std::chrono::steady_clock::now() - begin >= 2s) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
