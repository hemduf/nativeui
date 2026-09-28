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

/// Owner-local identity for one accepted DesktopServices request.
///
/// Zero is reserved as invalid. A request ID does not keep the service/backend
/// alive and must not be reused as a process-global identity.
using DesktopRequestId = std::uint64_t;
inline constexpr DesktopRequestId kInvalidDesktopRequestId = 0;

/// Result/status vocabulary shared by desktop-service requests.
///
/// Immediate validation/capacity/unsupported statuses are normally delivered
/// asynchronously through the owning Dispatcher when a valid callback and
/// Dispatcher are available.
enum class DesktopServiceStatus {
    Accepted,
    Cancelled,
    Busy,
    ResourceLimit,
    Unsupported,
    InvalidArgument,
    Error,
};

/// One user-facing file-dialog filter.
///
/// Extensions use a leading dot (for example ".wav"). NativeUI validation
/// accepts ASCII alphanumeric characters after the dot plus '.', '_', '+', '-'.
struct FileFilter final {
    std::string description;
    std::vector<std::string> extensions;
};

/// Options shared by single- and multiple-file open requests.
struct OpenFileOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
    std::vector<FileFilter> filters;
};

/// Options for one save-file request.
///
/// suggested_filename is a filename, not a path; values containing '/' or '\\'
/// are rejected as InvalidArgument.
struct SaveFileOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
    std::optional<std::string> suggested_filename;
    std::vector<FileFilter> filters;
};

/// Options for one directory-selection request.
struct DirectoryOptions final {
    std::string title;
    std::optional<std::filesystem::path> initial_directory;
};

/// Normalized result delivered to a file/directory callback.
///
/// Accepted single-file/save/directory requests contain exactly one path;
/// Accepted open-files requests contain at least one path. Backend results that
/// violate that cardinality are normalized to Error. Non-Accepted results expose
/// no paths. error is populated only for Error outcomes.
struct FileDialogResult final {
    DesktopServiceStatus status{DesktopServiceStatus::Error};
    std::vector<std::filesystem::path> paths;
    std::string error;
};

/// Completion for file/directory requests. The facade owns the callback until
/// the request becomes terminal or DesktopServices is destroyed.
using FileDialogCallback = std::function<void(FileDialogResult)>;
/// Completion for status-only requests such as open_url().
using StatusCallback = std::function<void(DesktopServiceStatus)>;

/// Per-DesktopServices bound: all file/open/save/directory requests share one
/// active chooser slot. There is no hidden overflow queue.
inline constexpr std::size_t kDesktopServicesMaxActiveFileChoosers = 1;
/// Per-DesktopServices bound for concurrently active URL requests.
inline constexpr std::size_t kDesktopServicesMaxActiveUrls = 16;

/// Platform/application backend seam used by DesktopServices.
///
/// The facade supplies a non-zero owner-local request_id and an owned completion
/// callback. A backend returns Accepted only when it accepted responsibility for
/// the request; any other status is treated as a start failure. A backend may
/// invoke completion synchronously, but DesktopServices still marshals the
/// application callback through its Dispatcher.
///
/// Backends may complete from native/worker threads. They must not let exceptions
/// cross native/foreign callback boundaries. Backend objects are shared-owned by
/// DesktopServices for at least the service lifetime.
class DesktopServicesBackend {
public:
    virtual ~DesktopServicesBackend() = default;

    /// Start one single-file chooser.
    virtual DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                                  const OpenFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    /// Start one multiple-file chooser.
    virtual DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                                   const OpenFileOptions& options,
                                                   FileDialogCallback completion) = 0;
    /// Start one save-file chooser.
    virtual DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                                  const SaveFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    /// Start one directory chooser.
    virtual DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                        const DirectoryOptions& options,
                                                        FileDialogCallback completion) = 0;
    /// Start opening one validated absolute HTTP(S) URL.
    virtual DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                                 std::string url,
                                                 StatusCallback completion) = 0;
    /// Request cancellation for an active backend request. Returning true means
    /// the backend accepted the cancellation request; terminal completion may
    /// still arrive through the supplied completion callback.
    virtual bool cancel(DesktopRequestId request_id) = 0;
};

/// Bounded asynchronous desktop integration tied to one Dispatcher owner.
///
/// Request initiation/cancellation is application/UI-side work and is not
/// real-time safe. Backend/native completion may occur on another thread, but
/// application callbacks are marshalled onto the owning Dispatcher and are
/// never intentionally invoked from the backend completion thread.
///
/// The service owns shared backend lifetime and pending callbacks. Destruction
/// gates all later application callback delivery and best-effort cancels active
/// backend requests without invoking user callbacks from the destructor.
class DesktopServices final {
public:
    /// Bind to one Dispatcher and optional shared backend.
    ///
    /// A missing backend is a supported configuration: requests with otherwise
    /// valid callback/Dispatcher inputs complete asynchronously as Unsupported.
    explicit DesktopServices(Dispatcher dispatcher,
                             std::shared_ptr<DesktopServicesBackend> backend = {});
    /// Best-effort no-throw-style shutdown: suppress pending application
    /// callbacks and request backend cancellation for active operations.
    ~DesktopServices();

    DesktopServices(const DesktopServices&) = delete;
    DesktopServices& operator=(const DesktopServices&) = delete;
    DesktopServices(DesktopServices&&) = delete;
    DesktopServices& operator=(DesktopServices&&) = delete;

    /// Request one file. Returns a non-zero ID only when the backend accepted
    /// the request. Invalid options, Unsupported/Busy/start failure return zero;
    /// with a valid callback/Dispatcher their status is posted asynchronously.
    [[nodiscard]] DesktopRequestId open_file(OpenFileOptions options,
                                             FileDialogCallback callback);
    /// Request one or more files. Accepted completion contains at least one path.
    [[nodiscard]] DesktopRequestId open_files(OpenFileOptions options,
                                              FileDialogCallback callback);
    /// Request one save path. suggested_filename must not contain path separators.
    [[nodiscard]] DesktopRequestId save_file(SaveFileOptions options,
                                             FileDialogCallback callback);
    /// Request one directory path.
    [[nodiscard]] DesktopRequestId select_directory(DirectoryOptions options,
                                                    FileDialogCallback callback);
    /// Request opening an absolute HTTP(S) URL with a non-empty authority.
    ///
    /// Other schemes/relative URLs are rejected as InvalidArgument. Returns a
    /// non-zero ID only when the backend accepted the request.
    [[nodiscard]] DesktopRequestId open_url(std::string url,
                                            StatusCallback callback);
    /// Ask the backend to cancel an active owner-local request.
    ///
    /// Returns false for zero/stale IDs, unavailable backend, rejected
    /// cancellation or backend exception. This call does not synchronously run
    /// the request's application completion.
    [[nodiscard]] bool cancel(DesktopRequestId request_id);

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace ui
