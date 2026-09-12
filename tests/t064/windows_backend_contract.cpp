#include "detail/windows_desktop_services.hpp"

#include <cstdlib>

int main() {
    auto backend = ui::detail::make_windows_desktop_services_backend(std::uintptr_t{0});
    if (!backend) return EXIT_FAILURE;

    bool completion_called = false;
    const auto status = backend->start_open_file(
        ui::kInvalidDesktopRequestId,
        {},
        [&completion_called](ui::FileDialogResult) { completion_called = true; });
    if (status != ui::DesktopServiceStatus::InvalidArgument || completion_called) {
        return EXIT_FAILURE;
    }
    if (backend->cancel(ui::kInvalidDesktopRequestId)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
