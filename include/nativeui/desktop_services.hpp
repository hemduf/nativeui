#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ui {

using DesktopRequestId = std::uint64_t;
inline constexpr DesktopRequestId kInvalidDesktopRequestId = 0;

enum class DesktopServiceStatus {
    Accepted,
    Cancelled,
    Busy,
    Unsupported,
    InvalidArgument,
    Error,
};

struct FileFilter final {
    std::string description;
    std::vector<std::string> extensions;
};

struct OpenFileOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
    std::vector<FileFilter> filters;
};

struct SaveFileOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
    std::optional<std::string> suggested_filename;
    std::vector<FileFilter> filters;
};

struct DirectoryOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
};

struct FileDialogResult final {
    DesktopServiceStatus status{DesktopServiceStatus::Error};
    std::vector<std::filesystem::path> paths;
    std::string error;
};

using FileDialogCallback = std::function<void(FileDialogResult)>;
using StatusCallback = std::function<void(DesktopServiceStatus)>;

} // namespace ui
