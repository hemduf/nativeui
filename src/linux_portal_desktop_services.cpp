#include "detail/linux_desktop_services.hpp"

#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ui::detail {
namespace {

constexpr std::string_view kPortalDestination = "org.freedesktop.portal.Desktop";
constexpr std::string_view kPortalDesktopPath = "/org/freedesktop/portal/desktop";
constexpr std::string_view kPortalRequestInterface = "org.freedesktop.portal.Request";
constexpr std::string_view kPortalOpenUriInterface = "org.freedesktop.portal.OpenURI";
constexpr std::string_view kPortalResponseMember = "Response";

[[nodiscard]] DesktopServiceStatus map_immediate_error(LinuxDbusErrorCode code) noexcept {
    switch (code) {
    case LinuxDbusErrorCode::None:
        return DesktopServiceStatus::Accepted;
    case LinuxDbusErrorCode::InitializationFailed:
    case LinuxDbusErrorCode::BusUnavailable:
    case LinuxDbusErrorCode::Disconnected:
        return DesktopServiceStatus::Unsupported;
    case LinuxDbusErrorCode::ResourceLimit:
        return DesktopServiceStatus::Busy;
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

class LinuxPortalDesktopServicesBackend final : public DesktopServicesBackend {
public:
    LinuxPortalDesktopServicesBackend(std::shared_ptr<LinuxPortalBus> bus,
                                      Dispatcher dispatcher,
                                      LinuxDbusClientId token_namespace)
        : state_(std::make_shared<State>(std::move(bus), std::move(dispatcher),
                                         token_namespace)) {}

    ~LinuxPortalDesktopServicesBackend() override {
        if (state_) state_->shutdown();
    }

    DesktopServiceStatus start_open_file(DesktopRequestId,
                                         const OpenFileOptions&,
                                         FileDialogCallback) override {
        return DesktopServiceStatus::Unsupported;
    }

    DesktopServiceStatus start_open_files(DesktopRequestId,
                                          const OpenFileOptions&,
                                          FileDialogCallback) override {
        return DesktopServiceStatus::Unsupported;
    }

    DesktopServiceStatus start_save_file(DesktopRequestId,
                                         const SaveFileOptions&,
                                         FileDialogCallback) override {
        return DesktopServiceStatus::Unsupported;
    }

    DesktopServiceStatus start_select_directory(DesktopRequestId,
                                                const DirectoryOptions&,
                                                FileDialogCallback) override {
        return DesktopServiceStatus::Unsupported;
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
        return state_ && state_->cancel(request_id);
    }

private:
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
