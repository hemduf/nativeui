#include <nativeui/desktop_services.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <cstdlib>
#include <memory>
#include <utility>

namespace {

void drain(ui::detail::DispatcherOwner& owner) {
    while (owner.pending_task_count() != 0) {
        (void)owner.checkpoint();
    }
}

class InlineBackend final : public ui::DesktopServicesBackend {
public:
    ui::DesktopServiceStatus start_open_file(ui::DesktopRequestId,
                                             const ui::OpenFileOptions&,
                                             ui::FileDialogCallback completion) override {
        completion(ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {"selected.wav"},
            .error = {},
        });
        return ui::DesktopServiceStatus::Accepted;
    }

    ui::DesktopServiceStatus start_open_files(ui::DesktopRequestId id,
                                              const ui::OpenFileOptions& options,
                                              ui::FileDialogCallback completion) override {
        return start_open_file(id, options, std::move(completion));
    }

    ui::DesktopServiceStatus start_save_file(ui::DesktopRequestId,
                                             const ui::SaveFileOptions&,
                                             ui::FileDialogCallback) override {
        return ui::DesktopServiceStatus::Unsupported;
    }

    ui::DesktopServiceStatus start_select_directory(ui::DesktopRequestId,
                                                    const ui::DirectoryOptions&,
                                                    ui::FileDialogCallback) override {
        return ui::DesktopServiceStatus::Unsupported;
    }

    ui::DesktopServiceStatus start_open_url(ui::DesktopRequestId,
                                            std::string,
                                            ui::StatusCallback completion) override {
        completion(ui::DesktopServiceStatus::Accepted);
        return ui::DesktopServiceStatus::Accepted;
    }

    bool cancel(ui::DesktopRequestId) override { return false; }
};

} // namespace

int main() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<InlineBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    bool file_called = false;
    const auto file_id = services.open_file({}, [&](ui::FileDialogResult result) {
        file_called = result.status == ui::DesktopServiceStatus::Accepted &&
            result.paths.size() == 1;
    });
    // Accepted backend work must retain a live non-zero request ID until the
    // T065 completion checkpoint, even when a backend reports completion from
    // inside start_*().  Returning 0 would incorrectly mean no backend request
    // started according to the public T064 request contract.
    if (file_id == ui::kInvalidDesktopRequestId || file_called) {
        return EXIT_FAILURE;
    }
    drain(owner);
    if (!file_called || services.cancel(file_id)) {
        return EXIT_FAILURE;
    }

    bool url_called = false;
    const auto url_id = services.open_url(
        "https://example.com",
        [&](ui::DesktopServiceStatus status) {
            url_called = status == ui::DesktopServiceStatus::Accepted;
        });
    if (url_id == ui::kInvalidDesktopRequestId || url_called) {
        return EXIT_FAILURE;
    }
    drain(owner);
    if (!url_called || services.cancel(url_id)) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
