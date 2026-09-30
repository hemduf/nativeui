/// \file
/// Bounded asynchronous desktop file, directory and URL integration.
///
/// Request values and callbacks are owned by DesktopServices until terminal.
/// Backend completions may originate on native/worker threads; application
/// callbacks are marshalled through the bound Dispatcher. Public request and
/// cancellation paths may allocate/synchronize and are not audio/DSP real-time.
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
/// Reserved invalid request identity returned when no backend request was accepted.
inline constexpr DesktopRequestId kInvalidDesktopRequestId = 0;

/// Result/status vocabulary shared by desktop-service requests.
///
/// Immediate validation/capacity/unsupported statuses are normally delivered
/// asynchronously through the owning Dispatcher when a valid callback and
/// Dispatcher are available.
enum class DesktopServiceStatus {
    /// Backend start accepted responsibility, or terminal completion succeeded.
    Accepted,
    /// The user, caller or backend cancelled before successful completion.
    Cancelled,
    /// This service owner already uses the bounded slot for that request family.
    Busy,
    /// A bounded lower-level resource could not accept more work.
    ResourceLimit,
    /// The operation is intentionally unavailable for this runtime/backend.
    Unsupported,
    /// Public validation rejected the URL, filename or filter data.
    InvalidArgument,
    /// Backend start or terminal execution failed.
    Error,
};

/// One user-facing file-dialog filter.
///
/// Extensions use a leading dot (for example ".wav"). NativeUI validation
/// accepts ASCII alphanumeric characters after the dot plus '.', '_', '+', '-'.
struct FileFilter final {
    /// Owned user-facing label presented when the backend exposes filter names.
    std::string description;
    /// Owned leading-dot extensions such as ".wav"; validated before backend start.
    std::vector<std::string> extensions;
};

/// Options shared by single- and multiple-file open requests.
struct OpenFileOptions final {
    /// Owned dialog title; interpretation of an empty title is backend-defined.
    std::string title;
    /// Optional initial path forwarded without a public existence preflight.
    std::optional<std::filesystem::path> initial_directory;
    /// Owned filters in presentation order.
    std::vector<FileFilter> filters;
};

/// Options for one save-file request.
///
/// suggested_filename is a filename, not a path; values containing '/' or '\\'
/// are rejected as InvalidArgument.
struct SaveFileOptions final {
    /// Owned dialog title; interpretation of an empty title is backend-defined.
    std::string title;
    /// Optional initial path forwarded without a public existence preflight.
    std::optional<std::filesystem::path> initial_directory;
    /// Optional owned filename suggestion; directory separators are invalid.
    std::optional<std::string> suggested_filename;
    /// Owned filters in presentation order.
    std::vector<FileFilter> filters;
};

/// Options for one directory-selection request.
struct DirectoryOptions final {
    /// Owned dialog title; interpretation of an empty title is backend-defined.
    std::string title;
    /// Optional initial directory forwarded without a public existence preflight.
    std::optional<std::filesystem::path> initial_directory;
};

/// Normalized result delivered to a file/directory callback.
///
/// Accepted single-file/save/directory requests contain exactly one path;
/// Accepted open-files requests contain at least one path. Backend results that
/// violate that cardinality are normalized to Error. Non-Accepted results expose
/// no paths. error is populated only for Error outcomes.
struct FileDialogResult final {
    /// Terminal normalized status.
    DesktopServiceStatus status{DesktopServiceStatus::Error};
    /// Owned selected paths; success cardinality depends on the originating request.
    std::vector<std::filesystem::path> paths;
    /// Owned diagnostic text for Error only; cleared for every other status.
    std::string error;
};

/// Completion for file/directory requests.
///
/// The facade owns the callback until terminal/destruction. Delivery is through
/// the bound Dispatcher on its UI/main thread; backend/native completion threads
/// never intentionally run application code. Callback exceptions propagate from
/// the Dispatcher checkpoint, not through the backend callback boundary.
using FileDialogCallback = std::function<void(FileDialogResult)>;
/// Status-only completion with the same ownership/threading contract.
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
    ///
    /// request_id is non-zero/owner-local. options is borrowed only for this
    /// synchronous call; completion ownership transfers to the backend and may
    /// be invoked synchronously or later from a native/worker thread. Return
    /// Accepted only after taking responsibility for terminal completion.
    virtual DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                                  const OpenFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    /// Start one multiple-file chooser with the same ownership/threading contract
    /// as start_open_file(); Accepted completion must contain at least one path.
    virtual DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                                   const OpenFileOptions& options,
                                                   FileDialogCallback completion) = 0;
    /// Start one save-file chooser; Accepted completion must contain one path.
    /// options is borrowed only for this synchronous start call.
    virtual DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                                  const SaveFileOptions& options,
                                                  FileDialogCallback completion) = 0;
    /// Start one directory chooser; Accepted completion must contain one path.
    virtual DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                        const DirectoryOptions& options,
                                                        FileDialogCallback completion) = 0;
    /// Start opening one already-validated absolute HTTP(S) URL.
    ///
    /// url and completion are owned parameters transferred into this call.
    /// Completion may be synchronous or cross-thread; the facade still marshals
    /// application delivery through its Dispatcher.
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

    /// Consume options/callback and request one file.
    ///
    /// Returns non-zero only after backend Accepted. Validation, Unsupported,
    /// Busy or start failure return zero while a valid callback/Dispatcher still
    /// receives the status asynchronously. Success delivers exactly one owned path.
    [[nodiscard]] DesktopRequestId open_file(OpenFileOptions options,
                                             FileDialogCallback callback);
    /// Request one or more files with open_file() ownership/error semantics.
    /// Accepted completion contains at least one owned path.
    [[nodiscard]] DesktopRequestId open_files(OpenFileOptions options,
                                              FileDialogCallback callback);
    /// Request one save path; suggested_filename must not contain separators.
    /// Accepted completion contains exactly one owned path.
    [[nodiscard]] DesktopRequestId save_file(SaveFileOptions options,
                                             FileDialogCallback callback);
    /// Request one directory path; Accepted completion contains one owned path.
    /// NativeUI performs no synchronous path-existence preflight.
    [[nodiscard]] DesktopRequestId select_directory(DirectoryOptions options,
                                                    FileDialogCallback callback);
    /// Request opening an absolute HTTP(S) URL with a non-empty authority.
    ///
    /// url/callback are consumed. Other schemes and relative URLs are
    /// InvalidArgument. Returns non-zero only after backend Accepted; terminal
    /// application delivery follows the same Dispatcher rules as file requests.
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
