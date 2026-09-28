#include "detail/linux_desktop_services.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::detail {
namespace {

constexpr std::string_view kPortalDestination = "org.freedesktop.portal.Desktop";
constexpr std::string_view kPortalDesktopPath = "/org/freedesktop/portal/desktop";
constexpr std::string_view kPortalRequestInterface = "org.freedesktop.portal.Request";
constexpr std::string_view kPortalOpenUriInterface = "org.freedesktop.portal.OpenURI";
constexpr std::string_view kPortalFileChooserInterface = "org.freedesktop.portal.FileChooser";
constexpr std::string_view kPortalResponseMember = "Response";

enum class FileChooserKind {
    OpenFile,
    OpenFiles,
    SaveFile,
    Directory,
};

[[nodiscard]] DesktopServiceStatus map_immediate_error(LinuxDbusErrorCode code) noexcept {
    switch (code) {
    case LinuxDbusErrorCode::None:
        return DesktopServiceStatus::Accepted;
    case LinuxDbusErrorCode::InitializationFailed:
    case LinuxDbusErrorCode::BusUnavailable:
    case LinuxDbusErrorCode::Disconnected:
        return DesktopServiceStatus::Unsupported;
    case LinuxDbusErrorCode::ResourceLimit:
        return DesktopServiceStatus::ResourceLimit;
    case LinuxDbusErrorCode::InvalidArgument:
        return DesktopServiceStatus::InvalidArgument;
    case LinuxDbusErrorCode::Cancelled:
    case LinuxDbusErrorCode::Shutdown:
        return DesktopServiceStatus::Cancelled;
    case LinuxDbusErrorCode::Timeout:
    case LinuxDbusErrorCode::RemoteError:
    case LinuxDbusErrorCode::LocalProtocolError:
        return DesktopServiceStatus::Error;
    }
    return DesktopServiceStatus::Error;
}

[[nodiscard]] DesktopServiceStatus map_completion_error(
    const LinuxDbusCompletion& completion) noexcept {
    if (completion.code == LinuxDbusErrorCode::RemoteError &&
        (completion.remote_error_name == "org.freedesktop.DBus.Error.ServiceUnknown" ||
         completion.remote_error_name == "org.freedesktop.DBus.Error.NameHasNoOwner")) {
        return DesktopServiceStatus::Unsupported;
    }
    return map_immediate_error(completion.code);
}

[[nodiscard]] FileDialogResult file_result(DesktopServiceStatus status,
                                           std::string diagnostic = {}) {
    FileDialogResult result;
    result.status = status;
    if (status == DesktopServiceStatus::Error) {
        result.error = diagnostic.empty()
            ? "XDG Desktop Portal FileChooser request failed"
            : std::move(diagnostic);
    }
    return result;
}

[[nodiscard]] std::string request_token(LinuxDbusClientId token_namespace,
                                        DesktopRequestId request_id) {
    return "nativeui_" + std::to_string(token_namespace) + "_" +
           std::to_string(request_id);
}

[[nodiscard]] std::string expected_request_path(std::string unique_name,
                                                std::string_view token) {
    if (unique_name.empty()) return {};
    if (unique_name.front() == ':') unique_name.erase(unique_name.begin());
    std::replace(unique_name.begin(), unique_name.end(), '.', '_');
    return "/org/freedesktop/portal/desktop/request/" + unique_name + "/" +
           std::string{token};
}

[[nodiscard]] LinuxDbusValue portal_options(std::string token) {
    std::vector<std::pair<std::string, LinuxDbusValue>> entries;
    entries.emplace_back(
        "handle_token",
        LinuxDbusValue::variant(LinuxDbusValue::string(std::move(token))));
    return LinuxDbusValue::dictionary(std::move(entries));
}

[[nodiscard]] LinuxDbusValue portal_folder(const std::filesystem::path& path) {
    const std::string native = path.generic_string();
    std::vector<LinuxDbusValue> bytes;
    bytes.reserve(native.size() + 1);
    for (const unsigned char byte : native) {
        bytes.push_back(LinuxDbusValue::byte(byte));
    }
    bytes.push_back(LinuxDbusValue::byte(0));
    return LinuxDbusValue::array("y", std::move(bytes));
}

[[nodiscard]] LinuxDbusValue portal_filters(const std::vector<FileFilter>& filters) {
    std::vector<LinuxDbusValue> encoded;
    encoded.reserve(filters.size());
    for (const auto& filter : filters) {
        std::vector<LinuxDbusValue> patterns;
        patterns.reserve(filter.extensions.size());
        for (const auto& extension : filter.extensions) {
            patterns.push_back(LinuxDbusValue::structure({
                LinuxDbusValue::uint32(0),
                LinuxDbusValue::string("*" + extension),
            }));
        }
        encoded.push_back(LinuxDbusValue::structure({
            LinuxDbusValue::string(filter.description),
            LinuxDbusValue::array("(us)", std::move(patterns)),
        }));
    }
    return LinuxDbusValue::array("(sa(us))", std::move(encoded));
}

[[nodiscard]] LinuxDbusValue file_chooser_options(
    std::string token,
    const std::optional<std::filesystem::path>& initial_directory,
    const std::vector<FileFilter>& filters,
    bool multiple,
    bool directory,
    const std::optional<std::string>& suggested_filename) {
    std::vector<std::pair<std::string, LinuxDbusValue>> entries;
    entries.emplace_back(
        "handle_token",
        LinuxDbusValue::variant(LinuxDbusValue::string(std::move(token))));
    if (multiple) {
        entries.emplace_back(
            "multiple", LinuxDbusValue::variant(LinuxDbusValue::boolean(true)));
    }
    if (directory) {
        entries.emplace_back(
            "directory", LinuxDbusValue::variant(LinuxDbusValue::boolean(true)));
    }
    if (initial_directory) {
        entries.emplace_back(
            "current_folder",
            LinuxDbusValue::variant(portal_folder(*initial_directory)));
    }
    if (suggested_filename) {
        entries.emplace_back(
            "current_name",
            LinuxDbusValue::variant(LinuxDbusValue::string(*suggested_filename)));
    }
    if (!filters.empty()) {
        entries.emplace_back(
            "filters", LinuxDbusValue::variant(portal_filters(filters)));
    }
    return LinuxDbusValue::dictionary(std::move(entries));
}

[[nodiscard]] DesktopServiceStatus portal_response_status(
    const LinuxDbusSignal& signal) noexcept {
    if (signal.arguments.size() < 2 ||
        signal.arguments[0].kind != LinuxDbusValueKind::UInt32 ||
        signal.arguments[1].kind != LinuxDbusValueKind::Dictionary) {
        return DesktopServiceStatus::Error;
    }
    switch (signal.arguments[0].uint32_value) {
    case 0:
        return DesktopServiceStatus::Accepted;
    case 1:
        return DesktopServiceStatus::Cancelled;
    default:
        return DesktopServiceStatus::Error;
    }
}

[[nodiscard]] const LinuxDbusValue* dictionary_value(const LinuxDbusValue& dictionary,
                                                     std::string_view key) noexcept {
    if (dictionary.kind != LinuxDbusValueKind::Dictionary) return nullptr;
    for (const auto& [candidate, variant] : dictionary.entries) {
        if (candidate == key && variant.kind == LinuxDbusValueKind::Variant &&
            variant.elements.size() == 1) {
            return &variant.elements.front();
        }
    }
    return nullptr;
}

[[nodiscard]] int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

[[nodiscard]] char ascii_lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool ascii_equal_ci(std::string_view lhs,
                                  std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower(lhs[i]) != ascii_lower(rhs[i])) return false;
    }
    return true;
}

[[nodiscard]] bool decode_local_file_uri(std::string_view uri,
                                         std::filesystem::path& path,
                                         std::string& error) {
    constexpr std::string_view prefix = "file://";
    if (uri.size() < prefix.size()) {
        error = "Portal returned a non-file URI";
        return false;
    }
    for (std::size_t i = 0; i < prefix.size(); ++i) {
        if (ascii_lower(uri[i]) != prefix[i]) {
            error = "Portal returned a non-file URI";
            return false;
        }
    }

    std::string_view remainder = uri.substr(prefix.size());
    std::string_view encoded_path;
    if (!remainder.empty() && remainder.front() == '/') {
        encoded_path = remainder;
    } else {
        const auto slash = remainder.find('/');
        if (slash == std::string_view::npos) {
            error = "Portal returned a file URI without a local path";
            return false;
        }
        const auto authority = remainder.substr(0, slash);
        if (!authority.empty() && !ascii_equal_ci(authority, "localhost")) {
            error = "Portal returned a non-local file URI";
            return false;
        }
        encoded_path = remainder.substr(slash);
    }

    if (encoded_path.empty() || encoded_path.find_first_of("?#") != std::string_view::npos) {
        error = "Portal returned an invalid local file URI";
        return false;
    }

    std::string decoded;
    decoded.reserve(encoded_path.size());
    for (std::size_t i = 0; i < encoded_path.size(); ++i) {
        const char c = encoded_path[i];
        if (c != '%') {
            if (c == '\0') {
                error = "Portal returned a file URI containing NUL";
                return false;
            }
            decoded.push_back(c);
            continue;
        }
        if (i + 2 >= encoded_path.size()) {
            error = "Portal returned a malformed percent-encoded file URI";
            return false;
        }
        const int high = hex_value(encoded_path[i + 1]);
        const int low = hex_value(encoded_path[i + 2]);
        if (high < 0 || low < 0) {
            error = "Portal returned a malformed percent-encoded file URI";
            return false;
        }
        const char byte = static_cast<char>((high << 4) | low);
        if (byte == '\0') {
            error = "Portal returned a file URI containing NUL";
            return false;
        }
        decoded.push_back(byte);
        i += 2;
    }

    try {
        path = std::filesystem::path{decoded};
    } catch (...) {
        error = "Portal returned a file URI that cannot be represented as a filesystem path";
        return false;
    }
    return true;
}

[[nodiscard]] FileDialogResult parse_file_response(const LinuxDbusSignal& signal,
                                                    FileChooserKind kind) {
    if (signal.arguments.size() < 2 ||
        signal.arguments[0].kind != LinuxDbusValueKind::UInt32 ||
        signal.arguments[1].kind != LinuxDbusValueKind::Dictionary) {
        return file_result(DesktopServiceStatus::Error,
                           "Portal returned a malformed FileChooser response");
    }

    const auto response = signal.arguments[0].uint32_value;
    if (response == 1) return file_result(DesktopServiceStatus::Cancelled);
    if (response != 0) {
        return file_result(DesktopServiceStatus::Error,
                           "Portal FileChooser request failed");
    }

    const auto* uris = dictionary_value(signal.arguments[1], "uris");
    if (!uris || uris->kind != LinuxDbusValueKind::Array ||
        uris->element_signature != "s") {
        return file_result(DesktopServiceStatus::Error,
                           "Portal FileChooser response is missing the URI selection");
    }

    FileDialogResult result;
    result.status = DesktopServiceStatus::Accepted;
    result.paths.reserve(uris->elements.size());
    for (const auto& uri : uris->elements) {
        if (uri.kind != LinuxDbusValueKind::String) {
            return file_result(DesktopServiceStatus::Error,
                               "Portal FileChooser returned a non-string URI");
        }
        std::filesystem::path path;
        std::string error;
        if (!decode_local_file_uri(uri.text, path, error)) {
            return file_result(DesktopServiceStatus::Error, std::move(error));
        }
        result.paths.push_back(std::move(path));
    }

    const bool cardinality_ok = kind == FileChooserKind::OpenFiles
        ? !result.paths.empty()
        : result.paths.size() == 1;
    if (!cardinality_ok) {
        return file_result(DesktopServiceStatus::Error,
                           "Portal FileChooser returned invalid selection cardinality");
    }
    return result;
}

class LinuxPortalDesktopServicesBackend final : public DesktopServicesBackend {
public:
    LinuxPortalDesktopServicesBackend(std::shared_ptr<LinuxPortalBus> bus,
                                      Dispatcher dispatcher,
                                      LinuxDbusClientId token_namespace)
        : state_(std::make_shared<State>(bus, dispatcher, token_namespace)),
          file_state_(std::make_shared<FileChooserState>(
              std::move(bus), std::move(dispatcher), token_namespace)) {}

    ~LinuxPortalDesktopServicesBackend() override {
        if (file_state_) file_state_->shutdown();
        if (state_) state_->shutdown();
    }

    DesktopServiceStatus start_open_file(DesktopRequestId request_id,
                                         const OpenFileOptions& options,
                                         FileDialogCallback completion) override {
        if (!file_state_ || request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        return file_state_->start(
            FileChooserKind::OpenFile, request_id, options.title,
            options.initial_directory, options.filters, std::nullopt,
            std::move(completion));
    }

    DesktopServiceStatus start_open_files(DesktopRequestId request_id,
                                          const OpenFileOptions& options,
                                          FileDialogCallback completion) override {
        if (!file_state_ || request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        return file_state_->start(
            FileChooserKind::OpenFiles, request_id, options.title,
            options.initial_directory, options.filters, std::nullopt,
            std::move(completion));
    }

    DesktopServiceStatus start_save_file(DesktopRequestId request_id,
                                         const SaveFileOptions& options,
                                         FileDialogCallback completion) override {
        if (!file_state_ || request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        return file_state_->start(
            FileChooserKind::SaveFile, request_id, options.title,
            options.initial_directory, options.filters, options.suggested_filename,
            std::move(completion));
    }

    DesktopServiceStatus start_select_directory(DesktopRequestId request_id,
                                                const DirectoryOptions& options,
                                                FileDialogCallback completion) override {
        if (!file_state_ || request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        return file_state_->start(
            FileChooserKind::Directory, request_id, options.title,
            options.initial_directory, {}, std::nullopt,
            std::move(completion));
    }

    DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                        std::string url,
                                        StatusCallback completion) override {
        if (!state_ || request_id == kInvalidDesktopRequestId || !completion) {
            return DesktopServiceStatus::InvalidArgument;
        }
        return state_->start_open_url(request_id, std::move(url), std::move(completion));
    }

    bool cancel(DesktopRequestId request_id) override {
        if (file_state_ && file_state_->cancel(request_id)) return true;
        return state_ && state_->cancel(request_id);
    }

private:
    struct ActiveFileRequest final {
        LinuxDbusSubscriptionId subscription{kInvalidLinuxDbusSubscriptionId};
        LinuxDbusRequestId method_request{kInvalidLinuxDbusRequestId};
        std::string handle_path;
        FileChooserKind kind{FileChooserKind::OpenFile};
        FileDialogCallback completion;
    };

    struct FileChooserState final : std::enable_shared_from_this<FileChooserState> {
        FileChooserState(std::shared_ptr<LinuxPortalBus> owner_bus,
                         Dispatcher owner_dispatcher,
                         LinuxDbusClientId owner_namespace)
            : bus(std::move(owner_bus)),
              dispatcher(std::move(owner_dispatcher)),
              token_namespace(owner_namespace) {}

        [[nodiscard]] LinuxDbusSignalMatch response_match(std::string path) const {
            LinuxDbusSignalMatch match;
            match.path = std::move(path);
            match.interface = std::string{kPortalRequestInterface};
            match.member = std::string{kPortalResponseMember};
            return match;
        }

        [[nodiscard]] LinuxDbusSignalCallback response_callback(
            DesktopRequestId request_id,
            FileChooserKind kind) {
            const std::weak_ptr<FileChooserState> weak = this->shared_from_this();
            return [weak, request_id, kind](LinuxDbusSignal signal) mutable {
                if (const auto self = weak.lock()) {
                    self->finish(request_id, parse_file_response(signal, kind));
                }
            };
        }

        [[nodiscard]] DesktopServiceStatus start(
            FileChooserKind kind,
            DesktopRequestId request_id,
            const std::string& title,
            const std::optional<std::filesystem::path>& initial_directory,
            const std::vector<FileFilter>& filters,
            const std::optional<std::string>& suggested_filename,
            FileDialogCallback completion) {
            if (!bus || token_namespace == kInvalidLinuxDbusClientId) {
                return DesktopServiceStatus::Unsupported;
            }

            const std::string token = request_token(token_namespace, request_id);
            const std::string handle = expected_request_path(bus->unique_name(), token);
            if (handle.empty()) return DesktopServiceStatus::Unsupported;

            const auto subscription = bus->subscribe_signal(
                dispatcher, response_match(handle), response_callback(request_id, kind));
            if (!subscription.ok()) return map_immediate_error(subscription.code);

            {
                std::lock_guard lock{mutex};
                if (closing || active.contains(request_id)) {
                    (void)bus->unsubscribe_signal(subscription.id);
                    return closing ? DesktopServiceStatus::Cancelled
                                   : DesktopServiceStatus::InvalidArgument;
                }
                try {
                    active.emplace(
                        request_id,
                        ActiveFileRequest{subscription.id, kInvalidLinuxDbusRequestId,
                                          handle, kind, std::move(completion)});
                } catch (...) {
                    (void)bus->unsubscribe_signal(subscription.id);
                    return DesktopServiceStatus::Error;
                }
            }

            LinuxDbusMethodCall call{
                std::string{kPortalDestination},
                std::string{kPortalDesktopPath},
                std::string{kPortalFileChooserInterface},
                kind == FileChooserKind::SaveFile ? "SaveFile" : "OpenFile",
            };
            call.arguments = {
                LinuxDbusValue::string(""),
                LinuxDbusValue::string(title),
                file_chooser_options(
                    token, initial_directory, filters,
                    kind == FileChooserKind::OpenFiles,
                    kind == FileChooserKind::Directory,
                    suggested_filename),
            };

            const std::weak_ptr<FileChooserState> weak = this->shared_from_this();
            const auto request = bus->call_method(
                dispatcher, call,
                [weak, request_id](LinuxDbusCompletion result) mutable {
                    if (const auto self = weak.lock()) {
                        self->initial_reply(request_id, std::move(result));
                    }
                });
            if (!request.ok()) {
                discard_without_completion(request_id);
                return map_immediate_error(request.code);
            }

            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found != active.end()) {
                    found->second.method_request = request.id;
                    return DesktopServiceStatus::Accepted;
                }
            }

            (void)bus->cancel_request(request.id);
            return DesktopServiceStatus::Cancelled;
        }

        void initial_reply(DesktopRequestId request_id, LinuxDbusCompletion completion) {
            if (completion.code != LinuxDbusErrorCode::None) {
                const auto status = map_completion_error(completion);
                finish(request_id, file_result(status, completion.message));
                return;
            }
            if (completion.values.size() != 1 ||
                completion.values.front().kind != LinuxDbusValueKind::ObjectPath ||
                completion.values.front().text.empty()) {
                finish(request_id,
                       file_result(DesktopServiceStatus::Error,
                                   "Portal FileChooser returned an invalid request handle"));
                return;
            }

            LinuxDbusSubscriptionId old_subscription = kInvalidLinuxDbusSubscriptionId;
            FileChooserKind kind{FileChooserKind::OpenFile};
            std::string returned_handle = completion.values.front().text;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                found->second.method_request = kInvalidLinuxDbusRequestId;
                if (found->second.handle_path == returned_handle) return;
                old_subscription = found->second.subscription;
                kind = found->second.kind;
            }

            const auto replacement = bus->subscribe_signal(
                dispatcher, response_match(returned_handle),
                response_callback(request_id, kind));
            if (!replacement.ok()) {
                finish(request_id,
                       file_result(map_immediate_error(replacement.code),
                                   "Unable to subscribe to Portal FileChooser response"));
                return;
            }

            bool installed = false;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found != active.end() && found->second.subscription == old_subscription) {
                    found->second.subscription = replacement.id;
                    found->second.handle_path = std::move(returned_handle);
                    installed = true;
                }
            }
            if (installed) {
                (void)bus->unsubscribe_signal(old_subscription);
            } else {
                (void)bus->unsubscribe_signal(replacement.id);
            }
        }

        void discard_without_completion(DesktopRequestId request_id) {
            LinuxDbusSubscriptionId subscription = kInvalidLinuxDbusSubscriptionId;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                subscription = found->second.subscription;
                active.erase(found);
            }
            if (subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(subscription);
            }
        }

        void finish(DesktopRequestId request_id, FileDialogResult result) {
            ActiveFileRequest request;
            {
                std::lock_guard lock{mutex};
                if (closing) return;
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                request = std::move(found->second);
                active.erase(found);
            }

            if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(request.subscription);
            }
            if (request.method_request != kInvalidLinuxDbusRequestId) {
                (void)bus->cancel_request(request.method_request);
            }
            if (request.completion) request.completion(std::move(result));
        }

        [[nodiscard]] bool cancel(DesktopRequestId request_id) {
            ActiveFileRequest request;
            {
                std::lock_guard lock{mutex};
                if (closing) return false;
                const auto found = active.find(request_id);
                if (found == active.end()) return false;
                request = std::move(found->second);
                active.erase(found);
            }

            if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(request.subscription);
            }
            if (request.method_request != kInvalidLinuxDbusRequestId) {
                (void)bus->cancel_request(request.method_request);
            }

            LinuxDbusMethodCall close{
                std::string{kPortalDestination},
                request.handle_path,
                std::string{kPortalRequestInterface},
                "Close",
            };
            (void)bus->call_method(dispatcher, close, [](LinuxDbusCompletion) {});

            if (request.completion) {
                request.completion(file_result(DesktopServiceStatus::Cancelled));
            }
            return true;
        }

        void shutdown() noexcept {
            std::vector<ActiveFileRequest> discarded;
            {
                std::lock_guard lock{mutex};
                if (closing) return;
                closing = true;
                discarded.reserve(active.size());
                for (auto& [id, request] : active) {
                    (void)id;
                    discarded.push_back(std::move(request));
                }
                active.clear();
            }

            if (!bus) return;
            for (const auto& request : discarded) {
                if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                    (void)bus->unsubscribe_signal(request.subscription);
                }
                if (request.method_request != kInvalidLinuxDbusRequestId) {
                    (void)bus->cancel_request(request.method_request);
                }
            }
        }

        std::shared_ptr<LinuxPortalBus> bus;
        Dispatcher dispatcher;
        LinuxDbusClientId token_namespace{kInvalidLinuxDbusClientId};
        std::mutex mutex;
        std::unordered_map<DesktopRequestId, ActiveFileRequest> active;
        bool closing{};
    };

    struct ActiveRequest final {
        LinuxDbusSubscriptionId subscription{kInvalidLinuxDbusSubscriptionId};
        LinuxDbusRequestId method_request{kInvalidLinuxDbusRequestId};
        std::string handle_path;
        StatusCallback completion;
    };

    struct State final : std::enable_shared_from_this<State> {
        State(std::shared_ptr<LinuxPortalBus> owner_bus,
              Dispatcher owner_dispatcher,
              LinuxDbusClientId owner_namespace)
            : bus(std::move(owner_bus)),
              dispatcher(std::move(owner_dispatcher)),
              token_namespace(owner_namespace) {}

        [[nodiscard]] LinuxDbusSignalMatch response_match(std::string path) const {
            LinuxDbusSignalMatch match;
            match.path = std::move(path);
            match.interface = std::string{kPortalRequestInterface};
            match.member = std::string{kPortalResponseMember};
            return match;
        }

        [[nodiscard]] LinuxDbusSignalCallback response_callback(DesktopRequestId request_id) {
            const std::weak_ptr<State> weak = this->shared_from_this();
            return [weak, request_id](LinuxDbusSignal signal) mutable {
                if (const auto self = weak.lock()) {
                    self->finish(request_id, portal_response_status(signal));
                }
            };
        }

        [[nodiscard]] DesktopServiceStatus start_open_url(DesktopRequestId request_id,
                                                          std::string url,
                                                          StatusCallback completion) {
            if (!bus || token_namespace == kInvalidLinuxDbusClientId) {
                return DesktopServiceStatus::Unsupported;
            }

            const std::string token = request_token(token_namespace, request_id);
            const std::string handle = expected_request_path(bus->unique_name(), token);
            if (handle.empty()) return DesktopServiceStatus::Unsupported;

            const auto subscription = bus->subscribe_signal(
                dispatcher, response_match(handle), response_callback(request_id));
            if (!subscription.ok()) return map_immediate_error(subscription.code);

            {
                std::lock_guard lock{mutex};
                if (closing || active.contains(request_id)) {
                    (void)bus->unsubscribe_signal(subscription.id);
                    return closing ? DesktopServiceStatus::Cancelled
                                   : DesktopServiceStatus::InvalidArgument;
                }
                try {
                    active.emplace(
                        request_id,
                        ActiveRequest{subscription.id, kInvalidLinuxDbusRequestId,
                                      handle, std::move(completion)});
                } catch (...) {
                    (void)bus->unsubscribe_signal(subscription.id);
                    return DesktopServiceStatus::Error;
                }
            }

            LinuxDbusMethodCall call{
                std::string{kPortalDestination},
                std::string{kPortalDesktopPath},
                std::string{kPortalOpenUriInterface},
                "OpenURI",
            };
            call.arguments = {
                LinuxDbusValue::string(""),
                LinuxDbusValue::string(std::move(url)),
                portal_options(token),
            };

            const std::weak_ptr<State> weak = this->shared_from_this();
            const auto request = bus->call_method(
                dispatcher, call,
                [weak, request_id](LinuxDbusCompletion result) mutable {
                    if (const auto self = weak.lock()) {
                        self->initial_reply(request_id, std::move(result));
                    }
                });
            if (!request.ok()) {
                discard_without_completion(request_id);
                return map_immediate_error(request.code);
            }

            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found != active.end()) {
                    found->second.method_request = request.id;
                    return DesktopServiceStatus::Accepted;
                }
            }

            (void)bus->cancel_request(request.id);
            return DesktopServiceStatus::Cancelled;
        }

        void initial_reply(DesktopRequestId request_id, LinuxDbusCompletion completion) {
            if (completion.code != LinuxDbusErrorCode::None) {
                finish(request_id, map_completion_error(completion));
                return;
            }
            if (completion.values.size() != 1 ||
                completion.values.front().kind != LinuxDbusValueKind::ObjectPath ||
                completion.values.front().text.empty()) {
                finish(request_id, DesktopServiceStatus::Error);
                return;
            }

            LinuxDbusSubscriptionId old_subscription = kInvalidLinuxDbusSubscriptionId;
            std::string returned_handle = completion.values.front().text;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                found->second.method_request = kInvalidLinuxDbusRequestId;
                if (found->second.handle_path == returned_handle) return;
                old_subscription = found->second.subscription;
            }

            const auto replacement = bus->subscribe_signal(
                dispatcher, response_match(returned_handle), response_callback(request_id));
            if (!replacement.ok()) {
                finish(request_id, map_immediate_error(replacement.code));
                return;
            }

            bool installed = false;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found != active.end() && found->second.subscription == old_subscription) {
                    found->second.subscription = replacement.id;
                    found->second.handle_path = std::move(returned_handle);
                    installed = true;
                }
            }
            if (installed) {
                (void)bus->unsubscribe_signal(old_subscription);
            } else {
                (void)bus->unsubscribe_signal(replacement.id);
            }
        }

        void discard_without_completion(DesktopRequestId request_id) {
            LinuxDbusSubscriptionId subscription = kInvalidLinuxDbusSubscriptionId;
            {
                std::lock_guard lock{mutex};
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                subscription = found->second.subscription;
                active.erase(found);
            }
            if (subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(subscription);
            }
        }

        void finish(DesktopRequestId request_id, DesktopServiceStatus status) {
            ActiveRequest request;
            {
                std::lock_guard lock{mutex};
                if (closing) return;
                const auto found = active.find(request_id);
                if (found == active.end()) return;
                request = std::move(found->second);
                active.erase(found);
            }

            if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(request.subscription);
            }
            if (request.method_request != kInvalidLinuxDbusRequestId) {
                (void)bus->cancel_request(request.method_request);
            }
            if (request.completion) request.completion(status);
        }

        [[nodiscard]] bool cancel(DesktopRequestId request_id) {
            ActiveRequest request;
            {
                std::lock_guard lock{mutex};
                if (closing) return false;
                const auto found = active.find(request_id);
                if (found == active.end()) return false;
                request = std::move(found->second);
                active.erase(found);
            }

            if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                (void)bus->unsubscribe_signal(request.subscription);
            }
            if (request.method_request != kInvalidLinuxDbusRequestId) {
                (void)bus->cancel_request(request.method_request);
            }

            LinuxDbusMethodCall close{
                std::string{kPortalDestination},
                request.handle_path,
                std::string{kPortalRequestInterface},
                "Close",
            };
            (void)bus->call_method(dispatcher, close, [](LinuxDbusCompletion) {});

            if (request.completion) request.completion(DesktopServiceStatus::Cancelled);
            return true;
        }

        void shutdown() noexcept {
            std::vector<ActiveRequest> discarded;
            {
                std::lock_guard lock{mutex};
                if (closing) return;
                closing = true;
                discarded.reserve(active.size());
                for (auto& [id, request] : active) {
                    (void)id;
                    discarded.push_back(std::move(request));
                }
                active.clear();
            }

            if (!bus) return;
            for (const auto& request : discarded) {
                if (request.subscription != kInvalidLinuxDbusSubscriptionId) {
                    (void)bus->unsubscribe_signal(request.subscription);
                }
                if (request.method_request != kInvalidLinuxDbusRequestId) {
                    (void)bus->cancel_request(request.method_request);
                }
            }
        }

        std::shared_ptr<LinuxPortalBus> bus;
        Dispatcher dispatcher;
        LinuxDbusClientId token_namespace{kInvalidLinuxDbusClientId};
        std::mutex mutex;
        std::unordered_map<DesktopRequestId, ActiveRequest> active;
        bool closing{};
    };

    std::shared_ptr<State> state_;
    std::shared_ptr<FileChooserState> file_state_;
};

} // namespace

std::shared_ptr<DesktopServicesBackend> make_linux_portal_desktop_services_backend(
    std::shared_ptr<LinuxPortalBus> bus,
    Dispatcher dispatcher,
    LinuxDbusClientId token_namespace) {
    if (!bus || token_namespace == kInvalidLinuxDbusClientId) return {};
    try {
        return std::make_shared<LinuxPortalDesktopServicesBackend>(
            std::move(bus), std::move(dispatcher), token_namespace);
    } catch (...) {
        return {};
    }
}

} // namespace ui::detail
