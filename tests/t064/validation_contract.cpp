#include <nativeui/desktop_services.hpp>
#include <nativeui/detail/dispatcher_owner.hpp>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void drain(ui::detail::DispatcherOwner& owner) {
    while (owner.pending_task_count() != 0) {
        (void)owner.checkpoint();
    }
}

class RecordingBackend final : public ui::DesktopServicesBackend {
public:
    std::size_t file_starts{};
    std::size_t url_starts{};
    std::unordered_map<ui::DesktopRequestId, ui::FileDialogCallback> files;
    std::unordered_map<ui::DesktopRequestId, ui::StatusCallback> urls;

    ui::DesktopServiceStatus start_open_file(
        ui::DesktopRequestId id,
        const ui::OpenFileOptions&,
        ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback));
    }

    ui::DesktopServiceStatus start_open_files(
        ui::DesktopRequestId id,
        const ui::OpenFileOptions&,
        ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback));
    }

    ui::DesktopServiceStatus start_save_file(
        ui::DesktopRequestId id,
        const ui::SaveFileOptions&,
        ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback));
    }

    ui::DesktopServiceStatus start_select_directory(
        ui::DesktopRequestId id,
        const ui::DirectoryOptions&,
        ui::FileDialogCallback callback) override {
        return start_file(id, std::move(callback));
    }

    ui::DesktopServiceStatus start_open_url(
        ui::DesktopRequestId id,
        std::string,
        ui::StatusCallback callback) override {
        ++url_starts;
        urls.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }

    bool cancel(ui::DesktopRequestId id) override {
        if (const auto found = files.find(id); found != files.end()) {
            auto callback = std::move(found->second);
            files.erase(found);
            callback(ui::FileDialogResult{
                .status = ui::DesktopServiceStatus::Cancelled,
                .paths = {},
                .error = {},
            });
            return true;
        }
        if (const auto found = urls.find(id); found != urls.end()) {
            auto callback = std::move(found->second);
            urls.erase(found);
            callback(ui::DesktopServiceStatus::Cancelled);
            return true;
        }
        return false;
    }

    void complete_file(ui::DesktopRequestId id, ui::FileDialogResult result) {
        const auto found = files.find(id);
        if (found == files.end()) return;
        auto callback = std::move(found->second);
        files.erase(found);
        callback(std::move(result));
    }

private:
    ui::DesktopServiceStatus start_file(
        ui::DesktopRequestId id,
        ui::FileDialogCallback callback) {
        ++file_starts;
        files.emplace(id, std::move(callback));
        return ui::DesktopServiceStatus::Accepted;
    }
};

bool expect_invalid_filter(ui::DesktopServices& services,
                           ui::detail::DispatcherOwner& owner,
                           RecordingBackend& backend,
                           std::string extension) {
    const auto starts_before = backend.file_starts;
    bool called = false;
    ui::OpenFileOptions options;
    options.filters = {{"", {std::move(extension)}}};
    const auto id = services.open_file(
        std::move(options),
        [&](ui::FileDialogResult result) {
            called = result.status == ui::DesktopServiceStatus::InvalidArgument &&
                     result.paths.empty() && result.error.empty();
        });
    if (id != ui::kInvalidDesktopRequestId || called ||
        backend.file_starts != starts_before) {
        return false;
    }
    drain(owner);
    return called && backend.file_starts == starts_before;
}

bool expect_invalid_url(ui::DesktopServices& services,
                        ui::detail::DispatcherOwner& owner,
                        RecordingBackend& backend,
                        std::string url) {
    const auto starts_before = backend.url_starts;
    bool called = false;
    const auto id = services.open_url(
        std::move(url),
        [&](ui::DesktopServiceStatus status) {
            called = status == ui::DesktopServiceStatus::InvalidArgument;
        });
    if (id != ui::kInvalidDesktopRequestId || called ||
        backend.url_starts != starts_before) {
        return false;
    }
    drain(owner);
    return called && backend.url_starts == starts_before;
}

} // namespace

int main() {
    ui::detail::DispatcherOwner owner;
    auto backend = std::make_shared<RecordingBackend>();
    ui::DesktopServices services{owner.dispatcher(), backend};

    // Exact extension grammar: ^\.[A-Za-z0-9][A-Za-z0-9._+-]*$.
    for (const std::string_view extension : {
             "", ".", "wav", "._bad", ".-bad", ".+bad", ".é"}) {
        if (!expect_invalid_filter(services, owner, *backend, std::string{extension})) {
            return EXIT_FAILURE;
        }
    }
    for (const std::string_view extension : {
             ".a", ".A0", ".wav", ".a.b_c+d-e"}) {
        ui::OpenFileOptions options;
        options.filters = {{"", {std::string{extension}}}};
        const auto id = services.open_file(std::move(options), [](ui::FileDialogResult) {});
        if (id == ui::kInvalidDesktopRequestId || !services.cancel(id)) {
            return EXIT_FAILURE;
        }
        drain(owner);
    }

    // Suggested filename is filename-only: both platform separator forms are
    // rejected, while UTF-8 filename data without separators remains valid.
    for (const std::string_view filename : {"dir/file.wav", "dir\\file.wav"}) {
        const auto starts_before = backend->file_starts;
        bool called = false;
        ui::SaveFileOptions options;
        options.suggested_filename = std::string{filename};
        const auto id = services.save_file(
            std::move(options),
            [&](ui::FileDialogResult result) {
                called = result.status == ui::DesktopServiceStatus::InvalidArgument;
            });
        if (id != ui::kInvalidDesktopRequestId || called ||
            backend->file_starts != starts_before) {
            return EXIT_FAILURE;
        }
        drain(owner);
        if (!called) return EXIT_FAILURE;
    }
    {
        ui::SaveFileOptions options;
        options.suggested_filename = std::string{"nativeui-\xC3\xA9.wav"};
        const auto id = services.save_file(std::move(options), [](ui::FileDialogResult) {});
        if (id == ui::kInvalidDesktopRequestId || !services.cancel(id)) {
            return EXIT_FAILURE;
        }
        drain(owner);
    }

    // URL validation is deliberately bounded to an ASCII-case-insensitive
    // HTTP(S) scheme and one non-empty authority before / ? or #.
    for (const std::string_view url : {
             "", "example.com", "file://example.com", "mailto:x@example.com",
             "http://", "https://", "http:///path", "https://?query",
             "https://#fragment"}) {
        if (!expect_invalid_url(services, owner, *backend, std::string{url})) {
            return EXIT_FAILURE;
        }
    }
    for (const std::string_view url : {
             "HTTP://example.com", "Https://example.com/path",
             "https://example.com?query", "https://example.com#fragment"}) {
        const auto id = services.open_url(std::string{url}, [](ui::DesktopServiceStatus) {});
        if (id == ui::kInvalidDesktopRequestId || !services.cancel(id)) {
            return EXIT_FAILURE;
        }
        drain(owner);
    }

    // Accepted selection cardinality is normalized at the public boundary.
    auto expect_cardinality_error = [&](auto start, std::vector<std::filesystem::path> paths) {
        bool called = false;
        const auto id = start([&](ui::FileDialogResult result) {
            called = result.status == ui::DesktopServiceStatus::Error &&
                     result.paths.empty() && !result.error.empty();
        });
        if (id == ui::kInvalidDesktopRequestId) return false;
        backend->complete_file(id, ui::FileDialogResult{
            .status = ui::DesktopServiceStatus::Accepted,
            .paths = std::move(paths),
            .error = {},
        });
        if (called) return false;
        drain(owner);
        return called && !services.cancel(id);
    };

    if (!expect_cardinality_error(
            [&](ui::FileDialogCallback callback) {
                return services.open_file({}, std::move(callback));
            },
            {}) ||
        !expect_cardinality_error(
            [&](ui::FileDialogCallback callback) {
                return services.open_file({}, std::move(callback));
            },
            {"a", "b"}) ||
        !expect_cardinality_error(
            [&](ui::FileDialogCallback callback) {
                return services.open_files({}, std::move(callback));
            },
            {}) ||
        !expect_cardinality_error(
            [&](ui::FileDialogCallback callback) {
                return services.save_file({}, std::move(callback));
            },
            {"a", "b"}) ||
        !expect_cardinality_error(
            [&](ui::FileDialogCallback callback) {
                return services.select_directory({}, std::move(callback));
            },
            {"a", "b"})) {
        return EXIT_FAILURE;
    }

    // Filesystem paths are values at the public boundary. Preserve one
    // non-ASCII path exactly across backend -> Dispatcher -> application.
    const std::filesystem::path unicode_path{
        std::u8string{u8"nativeui-s\u00E9lection-\u00E9.wav"}};
    bool unicode_called = false;
    const auto unicode_id = services.open_file({}, [&](ui::FileDialogResult result) {
        unicode_called = result.status == ui::DesktopServiceStatus::Accepted &&
                         result.paths == std::vector<std::filesystem::path>{unicode_path};
    });
    if (unicode_id == ui::kInvalidDesktopRequestId) return EXIT_FAILURE;
    backend->complete_file(unicode_id, ui::FileDialogResult{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {unicode_path},
        .error = {},
    });
    if (unicode_called) return EXIT_FAILURE;
    drain(owner);
    if (!unicode_called) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
