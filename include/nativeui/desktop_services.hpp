#pragma once

#include <nativeui/dispatcher.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

inline constexpr std::size_t kDesktopServicesMaxActiveFileChoosers = 1;
inline constexpr std::size_t kDesktopServicesMaxActiveUrls = 16;

class DesktopServicesBackend {
public:
    virtual ~DesktopServicesBackend() = default;

    virtual DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                                  const OpenFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    virtual DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                                   const OpenFileOptions& options,
                                                   FileDialogCallback completion) = 0;
    virtual DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                                  const SaveFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    virtual DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                        const DirectoryOptions& options,
                                                        FileDialogCallback completion) = 0;
    virtual DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                                 std::string url,
                                                 StatusCallback completion) = 0;
    virtual bool cancel(DesktopRequestId request_id) = 0;
};

class DesktopServices final {
public:
    explicit DesktopServices(Dispatcher dispatcher,
                             std::shared_ptr<DesktopServicesBackend> backend = {});
    ~DesktopServices();

    DesktopServices(const DesktopServices&) = delete;
    DesktopServices& operator=(const DesktopServices&) = delete;
    DesktopServices(DesktopServices&&) = delete;
    DesktopServices& operator=(DesktopServices&&) = delete;

    [[nodiscard]] DesktopRequestId open_file(OpenFileOptions options,
                                             FileDialogCallback callback);
    [[nodiscard]] DesktopRequestId open_files(OpenFileOptions options,
                                              FileDialogCallback callback);
    [[nodiscard]] DesktopRequestId save_file(SaveFileOptions options,
                                             FileDialogCallback callback);
    [[nodiscard]] DesktopRequestId select_directory(DirectoryOptions options,
                                                    FileDialogCallback callback);
    [[nodiscard]] DesktopRequestId open_url(std::string url,
                                            StatusCallback callback);
    [[nodiscard]] bool cancel(DesktopRequestId request_id);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace ui
