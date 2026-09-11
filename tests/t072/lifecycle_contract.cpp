#include "detail/linux_dbus.hpp"

#include <chrono>
#include <cstdlib>
#include <string>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    const auto begin = std::chrono::steady_clock::now();
    {
        LinuxDbusTransport first;
        LinuxDbusTransport second;

        if (first.start() != LinuxDbusErrorCode::None ||
            second.start() != LinuxDbusErrorCode::None ||
            !first.running() || !second.running()) {
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
