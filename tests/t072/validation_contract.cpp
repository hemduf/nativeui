#include "detail/linux_dbus.hpp"

#include <chrono>
#include <cstdlib>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    if (linux_dbus_valid_timeout(0ms) ||
        !linux_dbus_valid_timeout(1ms) ||
        !linux_dbus_valid_timeout(30s) ||
        !linux_dbus_valid_timeout(300s) ||
        linux_dbus_valid_timeout(300s + 1ms)) {
        return EXIT_FAILURE;
    }

    if (!linux_dbus_valid_object_path("/") ||
        !linux_dbus_valid_object_path("/org/nativeui/Test_1") ||
        linux_dbus_valid_object_path("") ||
        linux_dbus_valid_object_path("relative/path") ||
        linux_dbus_valid_object_path("/trailing/") ||
        linux_dbus_valid_object_path("/bad-dash")) {
        return EXIT_FAILURE;
    }

    if (!linux_dbus_initialize_threads() || !linux_dbus_initialize_threads()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
