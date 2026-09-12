#include "detail/macos_desktop_services.hpp"

#include <nativeui/window.hpp>

#include <cstdlib>

int main() {
    auto backend = ui::detail::make_macos_desktop_services_backend(
        static_cast<ui::NativeViewHandle>(0));
    return backend ? EXIT_SUCCESS : EXIT_FAILURE;
}
