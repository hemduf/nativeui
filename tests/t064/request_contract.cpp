#include <nativeui/desktop_services.hpp>
#include <nativeui/dispatcher.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void drain(ui::detail::DispatcherOwner& owner) {
    while (owner.pending_task_count() != 0) {
        (void)owner.checkpoint();
    }
}

int fail() { return EXIT_FAILURE; }

class FakeBackend final : public ui::DesktopServicesBackend {
public:
    ui::DesktopServiceStatus next_start_status{ui::DesktopServiceStatus::Accepted};
    bool complete_inline{};
    std::size_t file_starts{};
    std::size_t url_starts{};
    std::unordered_map<ui::DesktopRequestId, ui::FileDialogCallback> files;
    std::unordered_map<ui::DesktopRequestId, ui::StatusCallback> urls;

    ui::DesktopServiceStatus start_open_file(ui::DesktopRequestId id,
                                             const ui::OpenFileOptions&,
                                             ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback), false);
    }
    ui::DesktopServiceStatus start_open_files(ui::DesktopRequestId id,
                                              const ui::OpenFileOptions&,
                                              ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback), true);
    }
    ui::DesktopServiceStatus start_save_file(ui::DesktopRequestId id,
                                             const ui::SaveFileOptions&,
                                             ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback), false);
    }
    ui::DesktopServiceStatus start_select_directory(ui::DesktopRequestId id,
                                                    const ui::DirectoryOptions&,
                                                    ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback), false);
    }
    ui::DesktopServiceStatus start_open_url(ui::DesktopRequestId id,
                                            std::string,
                                            ui::StatusCallback callback) override {
        ++url_starts;
        const auto status = next_start_status;
        next_start_status = ui::DesktopServiceStatus::Accepted;
        if (status != ui::DesktopServiceStatus::Accepted) return status;
        if (complete_inline) {
            complete_inline = false;
            callback(ui::DesktopServiceStatus::Accepted);
        } else {
            urls.emplace(id, std::move(callback));
        }
        return status;
    }
    bool cancel(ui::DesktopRequestId id) override {
        if (const auto it = files.find(id); it != files.end()) {
            auto callback = std::move(it->second);
            files.erase(it);
            callback(ui::FileDialogResult{.status = ui::DesktopServiceStatus::Cancelled, .paths = {}, .error = {}});
            return true;
        }
        if (const auto it = urls.find(id); it != urls.end()) {
            auto callback = std::move(it->second);
            urls.erase(it);
            callback(ui::DesktopServiceStatus::Cancelled);
            return true;
        }
        return false;
    }

    void complete_file(ui::DesktopRequestId id, ui::FileDialogResult result) {
        auto it = files.find(id);
        if (it == files.end()) return;
        auto callback = std::move(it->second);
        files.erase(it);
        callback(std::move(result));
    }
    void complete_url(ui::DesktopRequestId id, ui::DesktopServiceStatus status) {
        auto it = urls.find(id);
        if (it == urls.end()) return;
        auto callback = std::move(it->second);
        urls.erase(it);
        callback(status);
    }

private:
    ui::DesktopServiceStatus start_file(ui::DesktopRequestId id,
                                        ui::FileDialogCallback callback,
                                        bool multiple) {
        ++file_starts;
        const auto status = next_start_status;
        next_start_status = ui::DesktopServiceStatus::Accepted;
        if (status != ui::DesktopServiceStatus::Accepted) return status;
        if (complete_inline) {
            complete_inline = false;
            callback(ui::FileDialogResult{
                .status = ui::DesktopServiceStatus::Accepted,
                .paths = multiple
                    ? std::vector<std::filesystem::path>{"a", "b"}
                    : std::vector<std::filesystem::path>{"a"},
                .error = {},
            });
        } else {
            files.emplace(id, std::move(callback));
        }
        return status;
    }
};
} // namespace


int main() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<FakeBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    bool invalid_called = false;
    ui::OpenFileOptions invalid;
    invalid.filters.push_back({"bad", {"wav"}});
    if (services.open_file(std::move(invalid), [&](ui::FileDialogResult result) {
            invalid_called = result.status == ui::DesktopServiceStatus::InvalidArgument;
        }) != ui::kInvalidDesktopRequestId || invalid_called || backend->file_starts != 0) {
        return fail();
    }
    drain(owner);
    if (!invalid_called) return fail();

    bool bad_save_called = false;
    ui::SaveFileOptions bad_save;
    bad_save.suggested_filename = "folder/file.wav";
    if (services.save_file(std::move(bad_save), [&](ui::FileDialogResult result) {
            bad_save_called = result.status == ui::DesktopServiceStatus::InvalidArgument;
        }) != ui::kInvalidDesktopRequestId || bad_save_called) {
        return fail();
    }
    drain(owner);
    if (!bad_save_called) return fail();

    bool bad_url_called = false;
    if (services.open_url("mailto:test@example.com", [&](ui::DesktopServiceStatus status) {
            bad_url_called = status == ui::DesktopServiceStatus::InvalidArgument;
        }) != ui::kInvalidDesktopRequestId || bad_url_called) {
        return fail();
    }
    drain(owner);
    if (!bad_url_called) return fail();

    bool first_done = false;
    ui::DesktopRequestId replacement = ui::kInvalidDesktopRequestId;
    const auto first = services.open_file({}, [&](ui::FileDialogResult result) {
        first_done = result.status == ui::DesktopServiceStatus::Accepted;
        replacement = services.open_file({}, [](ui::FileDialogResult) {});
    });
    if (first == ui::kInvalidDesktopRequestId) return fail();

    bool busy_done = false;
    if (services.open_files({}, [&](ui::FileDialogResult result) {
            busy_done = result.status == ui::DesktopServiceStatus::Busy;
        }) != ui::kInvalidDesktopRequestId || busy_done) {
        return fail();
    }
    backend->complete_file(first, {
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {"selected.wav"},
        .error = {},
    });
    if (first_done) return fail();
    drain(owner);
    if (!first_done || !busy_done || replacement == ui::kInvalidDesktopRequestId) return fail();
    if (!services.cancel(replacement)) return fail();
    drain(owner);

    std::vector<ui::DesktopRequestId> urls;
    urls.reserve(ui::kDesktopServicesMaxActiveUrls);
    ui::DesktopRequestId url_replacement = ui::kInvalidDesktopRequestId;
    for (std::size_t i = 0; i < ui::kDesktopServicesMaxActiveUrls; ++i) {
        ui::StatusCallback callback = [](ui::DesktopServiceStatus) {};
        if (i == 0) {
            callback = [&](ui::DesktopServiceStatus status) {
                if (status == ui::DesktopServiceStatus::Accepted) {
                    url_replacement = services.open_url(
                        "https://replacement.example", [](ui::DesktopServiceStatus) {});
                }
            };
        }
        const auto id = services.open_url("HTTPS://example.com/path", std::move(callback));
        if (id == ui::kInvalidDesktopRequestId) return fail();
        urls.push_back(id);
    }
    bool url_busy = false;
    if (services.open_url("http://example.com", [&](ui::DesktopServiceStatus status) {
            url_busy = status == ui::DesktopServiceStatus::Busy;
        }) != ui::kInvalidDesktopRequestId || url_busy) {
        return fail();
    }
    drain(owner);
    if (!url_busy) return fail();

    backend->complete_url(urls.front(), ui::DesktopServiceStatus::Accepted);
    if (url_replacement != ui::kInvalidDesktopRequestId) return fail();
    drain(owner);
    if (url_replacement == ui::kInvalidDesktopRequestId) return fail();

    std::size_t cancelled_callbacks = 0;
    // Capacity is full again after the replacement request, so free one older
    // URL before establishing the explicit cancellation contract.
    if (!services.cancel(urls[1])) return fail();
    drain(owner);
    const auto active_cancel_id = services.open_url(
        "https://cancel.example",
        [&](ui::DesktopServiceStatus status) {
            if (status == ui::DesktopServiceStatus::Cancelled) ++cancelled_callbacks;
        });
    if (active_cancel_id == ui::kInvalidDesktopRequestId ||
        !services.cancel(active_cancel_id) || services.cancel(active_cancel_id)) {
        return fail();
    }
    drain(owner);
    if (cancelled_callbacks != 1) return fail();

    backend->complete_inline = true;
    bool inline_result = false;
    const auto inline_id = services.open_files({}, [&](ui::FileDialogResult result) {
        inline_result = result.status == ui::DesktopServiceStatus::Accepted && result.paths.size() == 2;
    });
    if (inline_id != ui::kInvalidDesktopRequestId || inline_result) return fail();
    drain(owner);
    if (!inline_result) return fail();

    bool unsupported = false;
    {
        ui::DesktopServices no_backend{owner.dispatcher()};
        if (no_backend.open_file({}, [&](ui::FileDialogResult result) {
                unsupported = result.status == ui::DesktopServiceStatus::Unsupported;
            }) != ui::kInvalidDesktopRequestId || unsupported) {
            return fail();
        }
        drain(owner);
        if (!unsupported) return fail();
    }

    bool suppressed_immediate = false;
    {
        ui::DesktopServices no_backend{owner.dispatcher()};
        (void)no_backend.open_file({}, [&](ui::FileDialogResult) {
            suppressed_immediate = true;
        });
    }
    drain(owner);
    // Owner destruction suppresses work already accepted by the Dispatcher.
    if (suppressed_immediate) return fail();

    bool post_destroy = false;
    {
        auto transient_backend = std::make_shared<FakeBackend>();
        auto transient = std::make_unique<ui::DesktopServices>(owner.dispatcher(), transient_backend);
        const auto id = transient->open_file({}, [&](ui::FileDialogResult) { post_destroy = true; });
        if (id == ui::kInvalidDesktopRequestId) return fail();
        transient_backend->complete_file(id, {
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = {"done"},
            .error = {},
        });
        transient.reset();
    }
    drain(owner);
    if (post_destroy) return fail();

    return EXIT_SUCCESS;
}
