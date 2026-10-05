#include "detail/linux_dbus.hpp"
#include "fd_probe.hpp"

#include <chrono>
#include <cstdlib>
#include <string>

namespace {

constexpr const char* kUnreachableSessionAddress =
    "unix:path=/nonexistent/nativeui-t072-no-session";
constexpr const char* kOverrideAddress = "unix:path=/tmp/nativeui-t072-a11y-override";

} // namespace

int main() {
    using namespace ui::detail;

    // This process deliberately points the session bus at an unreachable
    // address before it touches libdbus, so the one-shot temporary discovery
    // connection must fail with a bounded BusUnavailable error.
    const std::string unreachable{kUnreachableSessionAddress};
    if (setenv("DBUS_SESSION_BUS_ADDRESS", unreachable.c_str(), 1) != 0 ||
        unsetenv("AT_SPI_BUS_ADDRESS") != 0) {
        return EXIT_FAILURE;
    }

    const std::size_t fd_before = t072_test::open_file_descriptor_count();
    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto discovery = LinuxDbusTransport::discover_accessibility_bus_address(
            nullptr, std::chrono::milliseconds{1});
        if (discovery.code != LinuxDbusErrorCode::BusUnavailable || discovery.available() ||
            !discovery.address.empty()) {
            return EXIT_FAILURE;
        }
    }
    const std::size_t fd_after = t072_test::open_file_descriptor_count();
    if (fd_before != static_cast<std::size_t>(-1) && fd_after != fd_before) {
        return EXIT_FAILURE;
    }

    // An unavailable session bus never disables the environment override.
    if (setenv("AT_SPI_BUS_ADDRESS", kOverrideAddress, 1) != 0) {
        return EXIT_FAILURE;
    }
    const auto override_discovery = LinuxDbusTransport::discover_accessibility_bus_address(
        nullptr, std::chrono::milliseconds{1});
    if (!override_discovery.available() ||
        override_discovery.address != kOverrideAddress) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
