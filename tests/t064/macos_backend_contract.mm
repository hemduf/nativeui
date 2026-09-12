#include "detail/macos_desktop_services.hpp"

#include <cstdint>
#include <cstdlib>

int main() {
    auto backend = ui::detail::make_macos_desktop_services_backend(std::uintptr_t{0});
    return backend ? EXIT_SUCCESS : EXIT_FAILURE;
}
