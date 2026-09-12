#include "detail/windows_desktop_services.hpp"

#include <cstdlib>

int main() {
    using namespace ui::detail::windows_desktop_services_detail;

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

    RequestState cancel_wins;
    if (!claim_cancellation(cancel_wins) || claim_cancellation(cancel_wins)) {
        return EXIT_FAILURE;
    }
    ui::FileDialogResult accepted_after_cancel{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {"ignored.wav"},
        .error = {},
    };
    arbitrate_worker_result(cancel_wins, accepted_after_cancel);
    if (accepted_after_cancel.status != ui::DesktopServiceStatus::Cancelled ||
        !accepted_after_cancel.paths.empty() ||
        !cancel_wins.worker_finished.load(std::memory_order_acquire)) {
        return EXIT_FAILURE;
    }

    RequestState worker_wins;
    ui::FileDialogResult accepted{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {"selected.wav"},
        .error = {},
    };
    arbitrate_worker_result(worker_wins, accepted);
    if (accepted.status != ui::DesktopServiceStatus::Accepted ||
        accepted.paths.size() != 1 ||
        claim_cancellation(worker_wins) ||
        !worker_wins.worker_finished.load(std::memory_order_acquire)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
