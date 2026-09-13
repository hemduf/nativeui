#include <nativeui/desktop_services.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <variant>
#include <type_traits>

namespace ui {
namespace {

enum class RequestKind {
    OpenFile,
    OpenFiles,
    SaveFile,
    SelectDirectory,
    OpenUrl,
};

[[nodiscard]] bool ascii_alnum(char c) noexcept {
    const unsigned char value = static_cast<unsigned char>(c);
    return (value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('z')) ||
           (value >= static_cast<unsigned char>('A') && value <= static_cast<unsigned char>('Z')) ||
           (value >= static_cast<unsigned char>('0') && value <= static_cast<unsigned char>('9'));
}

[[nodiscard]] bool valid_extension(std::string_view extension) noexcept {
    if (extension.size() < 2 || extension.front() != '.' || !ascii_alnum(extension[1])) {
        return false;
    }
    for (std::size_t i = 2; i < extension.size(); ++i) {
        const char c = extension[i];
        if (!ascii_alnum(c) && c != '.' && c != '_' && c != '+' && c != '-') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_filters(const std::vector<FileFilter>& filters) noexcept {
    for (const auto& filter : filters) {
        for (const auto& extension : filter.extensions) {
            if (!valid_extension(extension)) return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_save_filename(const std::optional<std::string>& filename) noexcept {
    if (!filename) return true;
    return filename->find('/') == std::string::npos &&
           filename->find('\\') == std::string::npos;
}

[[nodiscard]] char ascii_lower(char c) noexcept {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return c;
}

[[nodiscard]] bool starts_with_ascii_ci(std::string_view value,
                                        std::string_view prefix) noexcept {
    if (value.size() < prefix.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (ascii_lower(value[i]) != prefix[i]) return false;
    }
    return true;
}

[[nodiscard]] bool valid_http_url(std::string_view url) noexcept {
    std::size_t authority_start = 0;
    if (starts_with_ascii_ci(url, "http://")) {
        authority_start = 7;
    } else if (starts_with_ascii_ci(url, "https://")) {
        authority_start = 8;
    } else {
        return false;
    }
    if (authority_start >= url.size()) return false;
    const auto authority_end = url.find_first_of("/?#", authority_start);
    return authority_end == std::string_view::npos
        ? authority_start < url.size()
        : authority_end > authority_start;
}

[[nodiscard]] FileDialogResult immediate_file_result(DesktopServiceStatus status) {
    FileDialogResult result;
    result.status = status;
    if (status == DesktopServiceStatus::Error) {
        result.error = "Desktop services backend failed to start request";
    }
    return result;
}

[[nodiscard]] FileDialogResult normalize_file_result(RequestKind kind,
                                                     FileDialogResult result) {
    if (result.status == DesktopServiceStatus::Accepted) {
        const bool cardinality_ok = kind == RequestKind::OpenFiles
            ? !result.paths.empty()
            : result.paths.size() == 1;
        if (!cardinality_ok) {
            return FileDialogResult{
                .status = DesktopServiceStatus::Error,
                .paths = {},
                .error = "Desktop services backend returned invalid selection cardinality",
            };
        }
        result.error.clear();
        return result;
    }

    result.paths.clear();
    if (result.status == DesktopServiceStatus::Error) {
        if (result.error.empty()) result.error = "Desktop services backend error";
    } else {
        result.error.clear();
    }
    return result;
}

} // namespace

struct DesktopServices::Impl final : std::enable_shared_from_this<DesktopServices::Impl> {
    struct ActiveRequest final {
        RequestKind kind{RequestKind::OpenFile};
        std::variant<FileDialogCallback, StatusCallback> callback;
    };

    Impl(Dispatcher owner_dispatcher, std::shared_ptr<DesktopServicesBackend> owner_backend)
        : dispatcher(std::move(owner_dispatcher)),
          backend(std::move(owner_backend)),
          callback_gate(std::make_shared<std::atomic<bool>>(true)) {}

    [[nodiscard]] DesktopRequestId allocate_locked() noexcept {
        for (;;) {
            DesktopRequestId candidate = next_request_id++;
            if (next_request_id == kInvalidDesktopRequestId) next_request_id = 1;
            if (candidate == kInvalidDesktopRequestId || active.contains(candidate)) continue;
            return candidate;
        }
    }

    [[nodiscard]] std::size_t url_count_locked() const noexcept {
        return static_cast<std::size_t>(std::count_if(
            active.begin(), active.end(), [](const auto& item) {
                return item.second && item.second->kind == RequestKind::OpenUrl;
            }));
    }

    [[nodiscard]] bool chooser_active_locked() const noexcept {
        return std::any_of(active.begin(), active.end(), [](const auto& item) {
            return item.second && item.second->kind != RequestKind::OpenUrl;
        });
    }

    [[nodiscard]] std::unique_ptr<ActiveRequest> take(DesktopRequestId id) {
        std::lock_guard lock{mutex};
        if (closing) return {};
        const auto found = active.find(id);
        if (found == active.end()) return {};
        auto request = std::move(found->second);
        active.erase(found);
        return request;
    }

    void post_file(FileDialogCallback callback, FileDialogResult result) {
        if (!callback) return;
        auto gate = callback_gate;
        (void)dispatcher.post(
            [gate = std::move(gate), callback = std::move(callback),
             result = std::move(result)]() mutable {
                if (!gate || !gate->load(std::memory_order_acquire)) return;
                callback(std::move(result));
            });
    }

    void post_status(StatusCallback callback, DesktopServiceStatus status) {
        if (!callback) return;
        auto gate = callback_gate;
        (void)dispatcher.post(
            [gate = std::move(gate), callback = std::move(callback), status]() mutable {
                if (!gate || !gate->load(std::memory_order_acquire)) return;
                callback(status);
            });
    }

    void deliver_file(DesktopRequestId id, FileDialogResult result) {
        auto request = take(id);
        if (!request || request->kind == RequestKind::OpenUrl) return;
        auto callback = std::get<FileDialogCallback>(std::move(request->callback));
        if (!callback_gate->load(std::memory_order_acquire)) return;
        callback(normalize_file_result(request->kind, std::move(result)));
    }

    void deliver_status(DesktopRequestId id, DesktopServiceStatus status) {
        auto request = take(id);
        if (!request || request->kind != RequestKind::OpenUrl) return;
        auto callback = std::get<StatusCallback>(std::move(request->callback));
        if (!callback_gate->load(std::memory_order_acquire)) return;
        callback(status);
    }

    void queue_file_completion(DesktopRequestId id, FileDialogResult result) {
        const std::weak_ptr<Impl> weak = this->shared_from_this();
        const bool posted = dispatcher.post(
            [weak, id, result = std::move(result)]() mutable {
                if (const auto self = weak.lock()) {
                    self->deliver_file(id, std::move(result));
                }
            });
        if (!posted) {
            // Dispatcher rejection is terminal. Release capacity without ever
            // falling back to invoking application code on the backend thread.
            (void)take(id);
        }
    }

    void queue_status_completion(DesktopRequestId id, DesktopServiceStatus status) {
        const std::weak_ptr<Impl> weak = this->shared_from_this();
        const bool posted = dispatcher.post(
            [weak, id, status]() {
                if (const auto self = weak.lock()) self->deliver_status(id, status);
            });
        if (!posted) {
            (void)take(id);
        }
    }

    template <typename Options, typename Starter>
    DesktopRequestId start_file(RequestKind kind,
                                Options options,
                                FileDialogCallback callback,
                                Starter&& starter) {
        if (!callback) return kInvalidDesktopRequestId;
        if (!dispatcher.valid()) return kInvalidDesktopRequestId;

        if constexpr (std::is_same_v<Options, OpenFileOptions>) {
            if (!valid_filters(options.filters)) {
                post_file(std::move(callback),
                          immediate_file_result(DesktopServiceStatus::InvalidArgument));
                return kInvalidDesktopRequestId;
            }
        } else if constexpr (std::is_same_v<Options, SaveFileOptions>) {
            if (!valid_filters(options.filters) ||
                !valid_save_filename(options.suggested_filename)) {
                post_file(std::move(callback),
                          immediate_file_result(DesktopServiceStatus::InvalidArgument));
                return kInvalidDesktopRequestId;
            }
        }

        if (!backend) {
            post_file(std::move(callback),
                      immediate_file_result(DesktopServiceStatus::Unsupported));
            return kInvalidDesktopRequestId;
        }

        DesktopRequestId id = kInvalidDesktopRequestId;
        {
            std::lock_guard lock{mutex};
            if (closing) return kInvalidDesktopRequestId;
            if (!chooser_active_locked()) {
                id = allocate_locked();
                active.emplace(
                    id, std::make_unique<ActiveRequest>(
                            ActiveRequest{kind, std::move(callback)}));
            }
        }
        if (id == kInvalidDesktopRequestId) {
            post_file(std::move(callback), immediate_file_result(DesktopServiceStatus::Busy));
            return id;
        }

        const std::weak_ptr<Impl> weak = this->shared_from_this();
        DesktopServiceStatus start_status = DesktopServiceStatus::Error;
        try {
            start_status = starter(
                *backend, id, options,
                [weak, id](FileDialogResult result) mutable {
                    if (const auto self = weak.lock()) {
                        self->queue_file_completion(id, std::move(result));
                    }
                });
        } catch (...) {
            start_status = DesktopServiceStatus::Error;
        }

        if (start_status == DesktopServiceStatus::Accepted) {
            std::lock_guard lock{mutex};
            return !closing && active.contains(id) ? id : kInvalidDesktopRequestId;
        }

        auto request = take(id);
        if (request) {
            auto failed_callback = std::get<FileDialogCallback>(std::move(request->callback));
            post_file(std::move(failed_callback), immediate_file_result(start_status));
        }
        return kInvalidDesktopRequestId;
    }

    Dispatcher dispatcher;
    std::shared_ptr<DesktopServicesBackend> backend;
    std::shared_ptr<std::atomic<bool>> callback_gate;
    std::mutex mutex;
    std::unordered_map<DesktopRequestId, std::unique_ptr<ActiveRequest>> active;
    DesktopRequestId next_request_id{1};
    bool closing{};
};

DesktopServices::DesktopServices(Dispatcher dispatcher,
                                 std::shared_ptr<DesktopServicesBackend> backend)
    : impl_(std::make_shared<Impl>(std::move(dispatcher), std::move(backend))) {}

DesktopServices::~DesktopServices() {
    if (!impl_) return;
    auto state = std::move(impl_);
    std::unordered_map<DesktopRequestId, std::unique_ptr<Impl::ActiveRequest>> discarded;
    std::vector<DesktopRequestId> ids;
    std::shared_ptr<DesktopServicesBackend> backend;
    {
        std::lock_guard lock{state->mutex};
        if (state->closing) return;
        state->closing = true;
        state->callback_gate->store(false, std::memory_order_release);
        ids.reserve(state->active.size());
        for (const auto& [id, request] : state->active) {
            (void)request;
            ids.push_back(id);
        }
        discarded.swap(state->active);
        backend = state->backend;
    }
    if (backend) {
        for (const auto id : ids) {
            try {
                (void)backend->cancel(id);
            } catch (...) {
            }
        }
    }
}

DesktopRequestId DesktopServices::open_file(OpenFileOptions options,
                                            FileDialogCallback callback) {
    if (!impl_) return kInvalidDesktopRequestId;
    return impl_->start_file(
        RequestKind::OpenFile, std::move(options), std::move(callback),
        [](DesktopServicesBackend& backend, DesktopRequestId id,
           const OpenFileOptions& value, FileDialogCallback completion) {
            return backend.start_open_file(id, value, std::move(completion));
        });
}

DesktopRequestId DesktopServices::open_files(OpenFileOptions options,
                                             FileDialogCallback callback) {
    if (!impl_) return kInvalidDesktopRequestId;
    return impl_->start_file(
        RequestKind::OpenFiles, std::move(options), std::move(callback),
        [](DesktopServicesBackend& backend, DesktopRequestId id,
           const OpenFileOptions& value, FileDialogCallback completion) {
            return backend.start_open_files(id, value, std::move(completion));
        });
}

DesktopRequestId DesktopServices::save_file(SaveFileOptions options,
                                            FileDialogCallback callback) {
    if (!impl_) return kInvalidDesktopRequestId;
    return impl_->start_file(
        RequestKind::SaveFile, std::move(options), std::move(callback),
        [](DesktopServicesBackend& backend, DesktopRequestId id,
           const SaveFileOptions& value, FileDialogCallback completion) {
            return backend.start_save_file(id, value, std::move(completion));
        });
}

DesktopRequestId DesktopServices::select_directory(DirectoryOptions options,
                                                   FileDialogCallback callback) {
    if (!impl_ || !callback) return kInvalidDesktopRequestId;
    return impl_->start_file(
        RequestKind::SelectDirectory, std::move(options), std::move(callback),
        [](DesktopServicesBackend& backend, DesktopRequestId id,
           const DirectoryOptions& value, FileDialogCallback completion) {
            return backend.start_select_directory(id, value, std::move(completion));
        });
}

DesktopRequestId DesktopServices::open_url(std::string url,
                                           StatusCallback callback) {
    if (!impl_ || !callback) return kInvalidDesktopRequestId;
    auto state = impl_;
    if (!state->dispatcher.valid()) return kInvalidDesktopRequestId;
    if (!valid_http_url(url)) {
        state->post_status(std::move(callback), DesktopServiceStatus::InvalidArgument);
        return kInvalidDesktopRequestId;
    }
    if (!state->backend) {
        state->post_status(std::move(callback), DesktopServiceStatus::Unsupported);
        return kInvalidDesktopRequestId;
    }

    DesktopRequestId id = kInvalidDesktopRequestId;
    {
        std::lock_guard lock{state->mutex};
        if (state->closing) return kInvalidDesktopRequestId;
        if (state->url_count_locked() < kDesktopServicesMaxActiveUrls) {
            id = state->allocate_locked();
            state->active.emplace(
                id, std::make_unique<Impl::ActiveRequest>(
                        Impl::ActiveRequest{RequestKind::OpenUrl, std::move(callback)}));
        }
    }
    if (id == kInvalidDesktopRequestId) {
        state->post_status(std::move(callback), DesktopServiceStatus::Busy);
        return id;
    }

    const std::weak_ptr<Impl> weak = state;
    DesktopServiceStatus start_status = DesktopServiceStatus::Error;
    try {
        start_status = state->backend->start_open_url(
            id, std::move(url),
            [weak, id](DesktopServiceStatus status) {
                if (const auto self = weak.lock()) self->queue_status_completion(id, status);
            });
    } catch (...) {
        start_status = DesktopServiceStatus::Error;
    }

    if (start_status == DesktopServiceStatus::Accepted) {
        std::lock_guard lock{state->mutex};
        return !state->closing && state->active.contains(id)
            ? id
            : kInvalidDesktopRequestId;
    }

    auto request = state->take(id);
    if (request) {
        auto failed_callback = std::get<StatusCallback>(std::move(request->callback));
        state->post_status(std::move(failed_callback), start_status);
    }
    return kInvalidDesktopRequestId;
}

bool DesktopServices::cancel(DesktopRequestId request_id) {
    if (!impl_ || request_id == kInvalidDesktopRequestId) return false;
    std::shared_ptr<DesktopServicesBackend> backend;
    {
        std::lock_guard lock{impl_->mutex};
        if (impl_->closing || !impl_->active.contains(request_id)) return false;
        backend = impl_->backend;
    }
    if (!backend) return false;
    try {
        return backend->cancel(request_id);
    } catch (...) {
        return false;
    }
}

} // namespace ui
