#include "detail/linux_dbus.hpp"

#include <chrono>
#include <cstdlib>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    if (kLinuxDbusMaxPendingCalls != 1'024 ||
        kLinuxDbusMaxSubscriptions != 256 ||
        kLinuxDbusMaxObjectPaths != 256) {
        return EXIT_FAILURE;
    }
    if (kLinuxDbusDefaultTimeout != 30s ||
        kLinuxDbusMinTimeout != 1ms ||
        kLinuxDbusMaxTimeout != 300s) {
        return EXIT_FAILURE;
    }
    if (!linux_dbus_library_probe()) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
