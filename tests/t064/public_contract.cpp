#include <nativeui/desktop_services.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

static_assert(std::is_same_v<ui::DesktopRequestId, std::uint64_t>);
static_assert(ui::kInvalidDesktopRequestId == 0);
static_assert(std::is_same_v<ui::FileDialogCallback,
                             std::function<void(ui::FileDialogResult)>>);
static_assert(std::is_same_v<ui::StatusCallback,
                             std::function<void(ui::DesktopServiceStatus)>>);

int main() {
    ui::FileFilter filter{
        .description = "Audio",
        .extensions = {".wav", ".aiff"},
    };
    ui::OpenFileOptions open{
        .title = "Open",
        .initial_directory = std::filesystem::path{"missing-is-valid"},
        .filters = {filter},
    };
    ui::SaveFileOptions save{
        .title = "Save",
        .initial_directory = std::nullopt,
        .suggested_filename = std::string{"example.wav"},
        .filters = {filter},
    };
    ui::DirectoryOptions directory{
        .title = "Directory",
        .initial_directory = std::filesystem::path{"not-required-to-exist"},
    };
    ui::FileDialogResult result{
        .status = ui::DesktopServiceStatus::Accepted,
        .paths = {std::filesystem::path{"unicode-é.wav"}},
        .error = {},
    };

    if (open.title != "Open" || open.filters.size() != 1 ||
        save.suggested_filename != std::optional<std::string>{"example.wav"} ||
        directory.title != "Directory" || result.paths.size() != 1) {
        return EXIT_FAILURE;
    }

    for (const auto status : {
             ui::DesktopServiceStatus::Accepted,
             ui::DesktopServiceStatus::Cancelled,
             ui::DesktopServiceStatus::Busy,
             ui::DesktopServiceStatus::Unsupported,
             ui::DesktopServiceStatus::InvalidArgument,
             ui::DesktopServiceStatus::Error,
         }) {
        (void)status;
    }

    ui::FileDialogCallback file_callback = [](ui::FileDialogResult) {};
    ui::StatusCallback status_callback = [](ui::DesktopServiceStatus) {};
    file_callback(std::move(result));
    status_callback(ui::DesktopServiceStatus::Cancelled);
    return EXIT_SUCCESS;
}
